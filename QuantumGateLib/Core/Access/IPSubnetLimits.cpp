// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "IPSubnetLimits.h"

#include <algorithm>
#include <regex>

namespace QuantumGate::Implementation::Core::Access
{
	Result<> IPSubnetLimits::AddLimit(const IPAddressFamily af, const String& cidr_lbits, const Size max_con) noexcept
	{
		auto result_code = ResultCode::Failed;

		try
		{
			// Looks for CIDR bits specified in the format
			// "/999" used in CIDR notations
			std::wregex r(LR"bits(^\s*\/(\d+)\s*$)bits");
			std::wsmatch m;
			if (std::regex_search(cidr_lbits, m, r))
			{
				auto lbits = std::stoi(m[1].str());

				return AddLimit(af, lbits, max_con);
			}
			else
			{
				LogErr(L"Subnet limits: could not add limit; invalid arguments given");
				result_code = ResultCode::InvalidArgument;
			}
		}
		catch (...) {}

		return result_code;
	}

	Result<> IPSubnetLimits::AddLimit(const IPAddressFamily af, const UInt8 cidr_lbits, const Size max_con) noexcept
	{
		auto result_code = ResultCode::Failed;

		try
		{
			auto subnets = GetSubnets(af);
			if (subnets != nullptr)
			{
				if ((af == IPAddressFamily::IPv4 && cidr_lbits <= 32) ||
					(af == IPAddressFamily::IPv6 && cidr_lbits <= 128))
				{
					if (!HasLimit(af, cidr_lbits))
					{
						IPSubnetLimitImpl alimit;
						alimit.AddressFamily = af;
						alimit.CIDRLeadingBits = cidr_lbits;
						alimit.MaximumConnections = max_con;

						if (IPAddress::TryParseMask(af, cidr_lbits, alimit.SubnetMask))
						{
							const auto[it, success] = subnets->Limits.insert({ cidr_lbits, alimit });
							if (success)
							{
								// If we already had connections, add them to the new limit
								for (const auto& connection : subnets->Connections)
								{
									// Limits allowed to overflow in this case because the connections
									// were already present; connections above the maximum allowed number
									// will be removed later
									if (!AddLimitConnection(it->second, IPAddress(connection.second.Address),
															connection.second.CurrentConnections, true))
									{
										LogErr(L"Subnet limits: could not add limit connection");
									}
								}

								result_code = ResultCode::Succeeded;
							}
						}
						else LogErr(L"Subnet limits: could not add limit; error while parsing CIDR bitmask");
					}
					else LogErr(L"Subnet limits: could not add limit; limit already exists");
				}
				else
				{
					LogErr(L"Subnet limits: could not add limit; invalid arguments given");
					result_code = ResultCode::InvalidArgument;
				}
			}
		}
		catch (...) {}

		return result_code;
	}

	Result<> IPSubnetLimits::RemoveLimit(const IPAddressFamily af, const String& cidr_lbits) noexcept
	{
		auto result_code = ResultCode::Failed;

		try
		{
			// Looks for CIDR bits specified in the format
			// "/999" used in CIDR notations
			std::wregex r(LR"bits(^\s*\/(\d+)\s*$)bits");
			std::wsmatch m;
			if (std::regex_search(cidr_lbits, m, r))
			{
				auto lbits = std::stoi(m[1].str());

				return RemoveLimit(af, lbits);
			}
			else
			{
				LogErr(L"Subnet limits: could not remove limit; invalid arguments given");
				result_code = ResultCode::InvalidArgument;
			}
		}
		catch (...) {}

		return result_code;
	}

	Result<> IPSubnetLimits::RemoveLimit(const IPAddressFamily af, const UInt8 cidr_lbits) noexcept
	{
		auto subnets = GetSubnets(af);
		if (subnets != nullptr)
		{
			const auto it = subnets->Limits.find(cidr_lbits);
			if (it != subnets->Limits.end())
			{
				// Remove limit details for this limit
				for (auto it2 = m_IPSubnetLimitDetails.begin(); it2 != m_IPSubnetLimitDetails.end();)
				{
					if (it2->second.AddressFamily == it->second.AddressFamily &&
						it2->second.CIDRLeadingBits == it->second.CIDRLeadingBits)
					{
						it2 = m_IPSubnetLimitDetails.erase(it2);
					}
					else ++it2;
				}

				// Remove limit
				subnets->Limits.erase(it);

				return ResultCode::Succeeded;
			}
			else LogErr(L"Subnet limits: could not remove limit; limit does not exist");
		}

		return ResultCode::Failed;
	}

	Result<std::vector<IPSubnetLimit>> IPSubnetLimits::GetLimits() const noexcept
	{
		try
		{
			const std::vector<const IPSubnetAF*> iplimitmaps{ &m_IPv4Subnets, &m_IPv6Subnets };
			std::vector<IPSubnetLimit> iplimits;

			for (const auto limitmap : iplimitmaps)
			{
				for (const auto& limit : limitmap->Limits)
				{
					auto& ipsl = iplimits.emplace_back();
					ipsl.AddressFamily = limit.second.AddressFamily;
					ipsl.CIDRLeadingBits = Util::FormatString(L"/%u", limit.second.CIDRLeadingBits);
					ipsl.MaximumConnections = limit.second.MaximumConnections;
				}
			}

			return std::move(iplimits);
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	void IPSubnetLimits::Clear() noexcept
	{
		m_IPv4Subnets.Clear();
		m_IPv6Subnets.Clear();
		m_IPSubnetLimitDetails.clear();
	}

	const bool IPSubnetLimits::HasLimit(const IPAddressFamily af, const UInt8 cidr_lbits) const noexcept
	{
		auto subnets = GetSubnets(af);
		if (subnets != nullptr) return (subnets->Limits.find(cidr_lbits) != subnets->Limits.end());

		return false;
	}

	const bool IPSubnetLimits::AddConnection(const IPAddress& ip) noexcept
	{
		auto subnets = GetSubnets(ip.GetFamily());
		if (subnets != nullptr && CanAcceptConnection(subnets->Limits, ip))
		{
			if (AddSubnetConnection(subnets->Connections, ip))
			{
				if (AddLimitsConnection(subnets->Limits, ip))
				{
					return true;
				}
				else
				{
					if (!RemoveSubnetConnection(subnets->Connections, ip))
					{
						LogErr(L"Subnet limits: could not remove connection for address %s",
							   ip.GetString().c_str());
					}
				}
			}
		}

		return false;
	}

	const bool IPSubnetLimits::RemoveConnection(const IPAddress& ip) noexcept
	{
		auto subnets = GetSubnets(ip.GetFamily());
		if (subnets != nullptr)
		{
			if (RemoveSubnetConnection(subnets->Connections, ip))
			{
				return RemoveLimitsConnection(subnets->Limits, ip);
			}
		}

		return false;
	}

	const bool IPSubnetLimits::AddSubnetConnection(IPSubnetConnectionMap& connections, const IPAddress& ip) noexcept
	{
		try
		{
			// If we didn't yet have a connection from that IP address,
			// add it, otherwise increase its count
			const auto it = connections.find(ip.GetBinary());
			if (it == connections.end())
			{
				const auto[cit, retval] = connections.insert({ ip.GetBinary(),
															 IPSubnetConnection{ ip.GetBinary(), 1 } });
				if (!retval)
				{
					LogErr(L"Subnet limits: could not add connection for address %s", ip.GetString().c_str());
					return false;
				}
			}
			else ++it->second.CurrentConnections;

			return true;
		}
		catch (...) {}

		return false;
	}

	const bool IPSubnetLimits::RemoveSubnetConnection(IPSubnetConnectionMap& connections, const IPAddress& ip) noexcept
	{
		const auto it = connections.find(ip.GetBinary());
		if (it != connections.end())
		{
			--it->second.CurrentConnections;

			// If we don't have any connections from that IP
			// we can remove it
			if (it->second.CurrentConnections == 0)
			{
				connections.erase(it);
			}

			return true;
		}

		LogErr(L"Subnet limits: could not remove connection for address %s", ip.GetString().c_str());

		return false;
	}

	const bool IPSubnetLimits::AddLimitsConnection(const IPSubnetLimitMap& limits, const IPAddress& ip) noexcept
	{
		try
		{
			auto success = true;

			std::vector<const IPSubnetLimitImpl*> encr_limits(limits.size());

			for (const auto& limit : limits)
			{
				if (AddLimitConnection(limit.second, ip, 1, false))
				{
					encr_limits.push_back(&limit.second);
				}
				else
				{
					success = false;
					break;
				}
			}

			// If anything went wrong, undo the
			// limit connections that were added
			if (!success)
			{
				for (const auto& limit : encr_limits)
				{
					if (!RemoveLimitConnection(*limit, ip))
					{
						LogErr(L"Subnet limits: could not remove limit connection for address %s",
							   ip.GetString().c_str());
					}
				}
			}

			return success;
		}
		catch (...) {}

		return false;
	}

	const bool IPSubnetLimits::AddLimitConnection(const IPSubnetLimitImpl& limit, const IPAddress& ip,
												  const Size num, const bool allow_overflow) noexcept
	{
		try
		{
			const auto subnet = ip.GetBinary() & limit.SubnetMask;

			const auto it = m_IPSubnetLimitDetails.find(subnet);
			if (it == m_IPSubnetLimitDetails.end())
			{
				IPSubnetLimitDetail ldetail(limit.AddressFamily, limit.CIDRLeadingBits);
				ldetail.CurrentConnections = num;

				const auto[dit, retval] = m_IPSubnetLimitDetails.insert({ subnet, ldetail });
				if (!retval)
				{
					LogErr(L"Subnet limits: could not add limit details for subnet /%u, address %s",
						   limit.CIDRLeadingBits, ip.GetString().c_str());
					return false;
				}
			}
			else
			{
				if ((it->second.CurrentConnections < limit.MaximumConnections) ||
					(allow_overflow && it->second.CurrentConnections >= limit.MaximumConnections))
				{
					it->second.CurrentConnections += num;
				}
				else
				{
					LogWarn(L"Subnet limits: limit reached for subnet /%u; can't add address %s",
							limit.CIDRLeadingBits, ip.GetString().c_str());
					return false;
				}
			}

			return true;
		}
		catch (...) {}

		return false;
	}

	const bool IPSubnetLimits::RemoveLimitsConnection(const IPSubnetLimitMap& limits, const IPAddress& ip) noexcept
	{
		auto success = true;

		for (const auto& limit : limits)
		{
			if (!RemoveLimitConnection(limit.second, ip))
			{
				success = false;
			}
		}

		return success;
	}

	const bool IPSubnetLimits::RemoveLimitConnection(const IPSubnetLimitImpl& limit, const IPAddress& ip) noexcept
	{
		const auto subnet = ip.GetBinary() & limit.SubnetMask;

		const auto it = m_IPSubnetLimitDetails.find(subnet);
		if (it != m_IPSubnetLimitDetails.end())
		{
			if (it->second.CurrentConnections > 0)
			{
				--it->second.CurrentConnections;
			}
			else
			{
				LogErr(L"Subnet limits: inconsistency in limit details for subnet /%u while removing address %s",
					   limit.CIDRLeadingBits, ip.GetString().c_str());

				return false;
			}

			if (it->second.CurrentConnections == 0)
			{
				m_IPSubnetLimitDetails.erase(it);
			}
		}
		else
		{
			LogErr(L"Subnet limits: could not find limit details for subnet /%u; can't remove address %s",
				   limit.CIDRLeadingBits, ip.GetString().c_str());

			return false;
		}

		return true;
	}

	const bool IPSubnetLimits::HasConnectionOverflow(const IPAddress& ip) const noexcept
	{
		auto overflow = false;

		auto subnets = GetSubnets(ip.GetFamily());
		if (subnets != nullptr)
		{
			for (const auto& limit : subnets->Limits)
			{
				const auto subnet = ip.GetBinary() & limit.second.SubnetMask;

				const auto it = m_IPSubnetLimitDetails.find(subnet);
				if (it != m_IPSubnetLimitDetails.end())
				{
					if (it->second.CurrentConnections > limit.second.MaximumConnections)
					{
						// Too many connections on this subnet
						overflow = true;
						break;
					}
				}
			}
		}

		return overflow;
	}

	const bool IPSubnetLimits::CanAcceptConnection(const IPAddress& ip) const noexcept
	{
		auto subnets = GetSubnets(ip.GetFamily());
		if (subnets != nullptr)
		{
			return CanAcceptConnection(subnets->Limits, ip);
		}

		return false;
	}

	const bool IPSubnetLimits::CanAcceptConnection(const IPSubnetLimitMap& map, const IPAddress& ip) const noexcept
	{
		auto success = true;

		for (const auto& limit : map)
		{
			const auto subnet = ip.GetBinary() & limit.second.SubnetMask;

			const auto it = m_IPSubnetLimitDetails.find(subnet);
			if (it != m_IPSubnetLimitDetails.end())
			{
				if (it->second.CurrentConnections >= limit.second.MaximumConnections)
				{
					// No connections allowed anymore on this subnet
					success = false;
					break;
				}
			}
			else
			{
				// Subnet not found; probably no connections yet accepted;
				// check if connections are allowed
				if (limit.second.MaximumConnections == 0)
				{
					// No connections allowed on this subnet
					success = false;
					break;
				}
			}
		}

		return success;
	}

	IPSubnetAF* IPSubnetLimits::GetSubnets(const IPAddressFamily af) noexcept
	{
		switch (af)
		{
			case IPAddressFamily::IPv4:
				return &m_IPv4Subnets;
			case IPAddressFamily::IPv6:
				return &m_IPv6Subnets;
			default:
				break;
		}

		assert(false);

		return nullptr;
	}

	const IPSubnetAF* IPSubnetLimits::GetSubnets(const IPAddressFamily af) const noexcept
	{
		switch (af)
		{
			case IPAddressFamily::IPv4:
				return &m_IPv4Subnets;
			case IPAddressFamily::IPv6:
				return &m_IPv6Subnets;
			default:
				break;
		}

		assert(false);

		return nullptr;
	}
}