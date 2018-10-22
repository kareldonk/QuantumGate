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
	Result<IPFilterID> IPFilters::AddFilter(const WChar* ip_cidr,
											const IPFilterType type) noexcept
	{
		switch (type)
		{
			case IPFilterType::Allowed:
			case IPFilterType::Blocked:
				try
				{
					// CIDR address notation, "address/leading bits"
					// e.g. 127.0.0.1/8, 192.168.0.0/16, fc00::/7 etc.
					// Below regex just splits the CIDR notation into an address and
					// number of leading bits; the IPAddress class will validate them
					std::wregex r(LR"r(^\s*(.*)(\/\d+)\s*$)r");
					std::wcmatch m;
					if (std::regex_search(ip_cidr, m, r))
					{
						return AddFilter(m[1].str().c_str(), m[2].str().c_str(), type);
					}
				}
				catch (...)
				{
					LogErr(L"Could not add IP filter: an exception was thrown");
					return ResultCode::Failed;
				}
				break;
			default:
				assert(false);
				break;
		}

		return ResultCode::InvalidArgument;
	}

	Result<IPFilterID> IPFilters::AddFilter(const WChar* ip_str, const WChar* mask_str,
											const IPFilterType type) noexcept
	{
		IPAddress ip, mask;

		if (IPAddress::TryParse(ip_str, ip))
		{
			if (IPAddress::TryParseMask(ip.GetFamily(), mask_str, mask))
			{
				return AddFilter(ip, mask, type);
			}
			else
			{
				LogErr(L"Could not add IP filter: Invalid IP address mask %s", mask_str);
				return ResultCode::AddressMaskInvalid;
			}
		}
		else
		{
			LogErr(L"Could not add IP filter: Unrecognized IP address %s", ip_str);
			return ResultCode::AddressInvalid;
		}
	}

	Result<IPFilterID> IPFilters::AddFilter(const String& ip_str, const String& mask_str,
											const IPFilterType type) noexcept
	{
		return AddFilter(ip_str.c_str(), mask_str.c_str(), type);
	}

	Result<IPFilterID> IPFilters::AddFilter(const IPAddress& ip, const IPAddress& mask,
											const IPFilterType type) noexcept
	{
		switch (type)
		{
			case IPFilterType::Allowed:
			case IPFilterType::Blocked:
				return AddFilterImpl(ip, mask, type);
			default:
				assert(false);
				break;
		}

		return ResultCode::InvalidArgument;
	}

	Result<IPFilterID> IPFilters::AddFilterImpl(const IPAddress& ip, const IPAddress& mask,
												const IPFilterType type) noexcept
	{
		auto result_code = ResultCode::Failed;

		try
		{
			if (ip.GetFamily() == mask.GetFamily())
			{
				IPFilterImpl ipfilter;
				ipfilter.Type = type;
				ipfilter.Address = ip;
				ipfilter.Mask = mask;
				ipfilter.ID = GetFilterID(ipfilter.Address, ipfilter.Mask);

				if (!HasFilter(ipfilter.ID, type))
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
								m_IPAllowFilters[ipfilter.ID] = ipfilter;
								break;
							case IPFilterType::Blocked:
								m_IPBlockFilters[ipfilter.ID] = ipfilter;
								break;
						}

						return ipfilter.ID;
					}
					else LogErr(L"Could not add IP filter: failed to get IP range");
				}
				else LogErr(L"Could not add IP filter: filter already exists");
			}
			else LogErr(L"Could not add IP filter: IP and mask are from different address families");
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
					flt.ID = fltpair.second.ID;
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

	Result<bool> IPFilters::IsAllowed(const WChar* ip) const noexcept
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
