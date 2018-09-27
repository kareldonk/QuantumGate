// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "IPFilters.h"
#include "..\..\Common\Hash.h"
#include "..\..\Common\Endian.h"
#include "..\..\Network\Socket.h"

#include <regex>

namespace QuantumGate::Implementation::Core::Access
{
	Result<IPFilterID> IPFilters::AddFilter(const String& ip_cidr,
											const IPFilterType type) noexcept
	{
		switch (type)
		{
			case IPFilterType::Allowed:
			case IPFilterType::Blocked:
				return AddFilterImpl(ip_cidr, type);
		}

		return ResultCode::InvalidArgument;
	}

	Result<IPFilterID> IPFilters::AddFilter(const String& ip, const String& mask,
											const IPFilterType type) noexcept
	{
		switch (type)
		{
			case IPFilterType::Allowed:
			case IPFilterType::Blocked:
				return AddFilterImpl(ip, mask, type);
		}

		return ResultCode::InvalidArgument;
	}

	Result<IPFilterID> IPFilters::AddFilterImpl(const String& ip_cidr,
												const IPFilterType type) noexcept
	{
		try
		{
			// CIDR address notation, "address/leading bits"
			// e.g. 127.0.0.1/8, 192.168.0.0/16, fc00::/7 etc.
			// Below regex just splits the CIDR notation into an address and
			// number of leading bits; the IPAddress class will validate them
			std::wregex r(LR"r(^\s*(.*)(\/\d+)\s*$)r");
			std::wsmatch m;
			if (std::regex_search(ip_cidr, m, r))
			{
				return AddFilterImpl(m[1].str(), m[2].str(), type);
			}
		}
		catch (...) {}

		return ResultCode::InvalidArgument;
	}

	Result<IPFilterID> IPFilters::AddFilterImpl(const String& ip, const String& mask,
												const IPFilterType type) noexcept
	{
		auto result_code = ResultCode::Failed;

		try
		{
			IPFilterImpl ipfilter;
			ipfilter.Type = type;

			if (IPAddress::TryParse(ip, ipfilter.Address))
			{
				if (IPAddress::TryParseMask(ipfilter.Address.GetFamily(), mask, ipfilter.Mask))
				{
					ipfilter.FilterID = GetFilterID(ipfilter.Address, ipfilter.Mask);

					if (!HasFilter(ipfilter.FilterID, type))
					{
						const auto range = BinaryIPAddress::GetAddressRange(ipfilter.Address.GetBinary(),
																			ipfilter.Mask.GetBinary());
						if (range.has_value())
						{
							ipfilter.StartAddress = range->first;
							ipfilter.EndAddress = range->second;

							switch (ipfilter.Type)
							{
								case IPFilterType::Allowed:
									m_IPAllowFilters[ipfilter.FilterID] = ipfilter;
									break;
								case IPFilterType::Blocked:
									m_IPBlockFilters[ipfilter.FilterID] = ipfilter;
									break;
							}

							return ipfilter.FilterID;
						}
						else LogErr(L"Could not add IP filter: failed to get IP range");
					}
					else LogErr(L"Could not add IP filter: filter already exists");
				}
				else
				{
					LogErr(L"Could not add IP filter: Invalid IP address mask %s", mask.c_str());
					result_code = ResultCode::AddressMaskInvalid;
				}
			}
			else
			{
				LogErr(L"Could not add IP filter: Unrecognized IP address %s", ip.c_str());
				result_code = ResultCode::AddressInvalid;
			}
		}
		catch (...) {}

		return result_code;
	}

	Result<> IPFilters::RemoveFilter(const IPFilterID filterid, const IPFilterType type) noexcept
	{
		try
		{
			IPFilterMap* fltmap{ nullptr };

			switch (type)
			{
				case IPFilterType::Allowed:
					fltmap = &m_IPAllowFilters;
					break;
				case IPFilterType::Blocked:
					fltmap = &m_IPBlockFilters;
					break;
				default:
					return ResultCode::InvalidArgument;
			}

			if (fltmap->erase(filterid) != 0)
			{
				return ResultCode::Succeeded;
			}
			else LogErr(L"Could not remove IP filter: filter does not exist");
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	void IPFilters::Clear() noexcept
	{
		m_IPAllowFilters.clear();
		m_IPBlockFilters.clear();
	}

	const bool IPFilters::HasFilter(const IPFilterID filterid, const IPFilterType type) const noexcept
	{
		const IPFilterMap* fltmap{ nullptr };

		switch (type)
		{
			case IPFilterType::Allowed:
				fltmap = &m_IPAllowFilters;
				break;
			case IPFilterType::Blocked:
				fltmap = &m_IPBlockFilters;
				break;
			default:
				return false;
		}

		return (fltmap->find(filterid) != fltmap->end());
	}

	Result<Vector<IPFilter>> IPFilters::GetFilters() const noexcept
	{
		try
		{
			const std::array<const IPFilterMap*, 2> ipfiltermaps{ &m_IPAllowFilters, &m_IPBlockFilters };
			Vector<IPFilter> ipfilters;

			for (const auto& fltmap : ipfiltermaps)
			{
				for (const auto& fltpair : *fltmap)
				{
					auto& flt = ipfilters.emplace_back();
					flt.FilterID = fltpair.second.FilterID;
					flt.Type = fltpair.second.Type;
					flt.Address = fltpair.second.Address;
					flt.Mask = fltpair.second.Mask;
				}
			}

			return ipfilters;
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	const bool IPFilters::IsIPInFilterMap(const IPAddress& address, const IPFilterMap& filtermap) const noexcept
	{
		for (const auto& fltpair : filtermap)
		{
			const auto& filter = fltpair.second;

			if (filter.Address.GetFamily() == address.GetFamily())
			{
				const auto[success, inrange] = BinaryIPAddress::IsInAddressRange(address.GetBinary(),
																				 filter.StartAddress, filter.EndAddress);
				if (success && inrange)
				{
					// If the IP address was within the filter range of at least one filter
					// we can stop looking immediately
					return true;
				}
			}
		}

		return false;
	}

	const IPFilterID IPFilters::GetFilterID(const IPAddress& ip, const IPAddress& mask) const noexcept
	{
		return Hash::GetNonPersistentHash(ip.GetString() + mask.GetString());
	}

	Result<bool> IPFilters::IsAllowed(const String& ip) const noexcept
	{
		IPAddress ipaddr;
		if (IPAddress::TryParse(ip, ipaddr))
		{
			return IsAllowed(ipaddr);
		}

		LogErr(L"Could not check if IP is allowed: unrecognized IP address");

		return ResultCode::AddressInvalid;
	}

	Result<bool> IPFilters::IsAllowed(const IPAddress& ipaddr) const noexcept
	{
		// If the IP address is not in the blocked filter ranges we can return true immediately
		if (!IsIPInFilterMap(ipaddr, m_IPBlockFilters))
		{
			return true;
		}
		else
		{
			// If the IP address is in the blocked filter ranges check if it's also in the allowed
			// filter ranges, in which case it was explicitly allowed through
			if (IsIPInFilterMap(ipaddr, m_IPAllowFilters))
			{
				return true;
			}
		}

		// If we get here, the IP address was in the blocked filter ranges and 
		// not in the allowed filter ranges, in which case it's not allowed
		return false;
	}
}
