// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "AccessManager.h"
#include "..\Peer\PeerManager.h"

namespace QuantumGate::Implementation::Core::Access
{
	Manager::Manager(const Settings_CThS& settings) noexcept :
		m_Settings(settings)
	{}

	Result<IPFilterID> Manager::AddIPFilter(const WChar* ip_cidr,
											const IPFilterType type) noexcept
	{
		auto result = m_IPFilters.WithUniqueLock()->AddFilter(ip_cidr, type);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<IPFilterID> Manager::AddIPFilter(const WChar* ip_str, const WChar* mask_str,
											const IPFilterType type) noexcept
	{
		auto result = m_IPFilters.WithUniqueLock()->AddFilter(ip_str, mask_str, type);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<IPFilterID> Manager::AddIPFilter(const IPAddress& ip, const IPAddress& mask,
											const IPFilterType type) noexcept
	{
		auto result = m_IPFilters.WithUniqueLock()->AddFilter(ip, mask, type);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<> Manager::RemoveIPFilter(const IPFilterID filterid, const IPFilterType type) noexcept
	{
		auto result = m_IPFilters.WithUniqueLock()->RemoveFilter(filterid, type);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	void Manager::RemoveAllIPFilters() noexcept
	{
		m_IPFilters.WithUniqueLock()->Clear();
		m_AccessUpdateCallbacks.WithUniqueLock()();
	}

	Result<Vector<IPFilter>> Manager::GetAllIPFilters() const noexcept
	{
		return m_IPFilters.WithSharedLock()->GetFilters();
	}

	Result<> Manager::SetAddressReputation(const Address& addr, const Int16 score,
										   const std::optional<Time>& time) noexcept
	{
		auto result = m_AddressAccessControl.WithUniqueLock()->SetReputation(addr, score, time);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<> Manager::SetAddressReputation(const AddressReputation& addr_rep) noexcept
	{
		auto result = m_AddressAccessControl.WithUniqueLock()->SetReputation(addr_rep.Address,
																			 addr_rep.Score,
																			 addr_rep.LastUpdateTime);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<> Manager::ResetAddressReputation(const WChar* addr_str) noexcept
	{
		Address addr;
		if (Address::TryParse(addr_str, addr))
		{
			return ResetAddressReputation(addr);
		}

		return ResultCode::AddressInvalid;
	}

	Result<> Manager::ResetAddressReputation(const Address& addr) noexcept
	{
		auto result = m_AddressAccessControl.WithUniqueLock()->ResetReputation(addr);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	void Manager::ResetAllAddressReputations() noexcept
	{
		m_AddressAccessControl.WithUniqueLock()->ResetAllReputations();
		m_AccessUpdateCallbacks.WithUniqueLock()();
	}

	Result<std::pair<Int16, bool>> Manager::UpdateAddressReputation(const Address& addr,
																	const AddressReputationUpdate rep_update) noexcept
	{
		auto result = m_AddressAccessControl.WithUniqueLock()->UpdateReputation(addr, rep_update);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<Vector<AddressReputation>> Manager::GetAllAddressReputations() const noexcept
	{
		return m_AddressAccessControl.WithUniqueLock()->GetReputations();
	}

	bool Manager::AddIPConnection(const IPAddress& ip) noexcept
	{
		return m_SubnetLimits.WithUniqueLock()->AddConnection(ip);
	}

	bool Manager::RemoveIPConnection(const IPAddress& ip) noexcept
	{
		return m_SubnetLimits.WithUniqueLock()->RemoveConnection(ip);
	}

	bool Manager::AddConnectionAttempt(const Address& addr) noexcept
	{
		if (!m_AddressAccessControl.WithUniqueLock()->AddConnectionAttempt(addr))
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
			return false;
		}

		return true;
	}

	bool Manager::AddRelayConnectionAttempt(const Address& addr) noexcept
	{
		if (!m_AddressAccessControl.WithUniqueLock()->AddRelayConnectionAttempt(addr))
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
			return false;
		}

		return true;
	}

	Result<> Manager::AddIPSubnetLimit(const IPAddress::Family af, const String& cidr_lbits,
									   const Size max_con) noexcept
	{
		auto result = m_SubnetLimits.WithUniqueLock()->AddLimit(af, cidr_lbits, max_con);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<> Manager::AddIPSubnetLimit(const IPAddress::Family af, const UInt8 cidr_lbits, const Size max_con) noexcept
	{
		auto result = m_SubnetLimits.WithUniqueLock()->AddLimit(af, cidr_lbits, max_con);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<> Manager::RemoveIPSubnetLimit(const IPAddress::Family af, const String& cidr_lbits) noexcept
	{
		auto result = m_SubnetLimits.WithUniqueLock()->RemoveLimit(af, cidr_lbits);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<> Manager::RemoveIPSubnetLimit(const IPAddress::Family af, const UInt8 cidr_lbits) noexcept
	{
		auto result = m_SubnetLimits.WithUniqueLock()->RemoveLimit(af, cidr_lbits);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<Vector<IPSubnetLimit>> Manager::GetAllIPSubnetLimits() const noexcept
	{
		return m_SubnetLimits.WithSharedLock()->GetLimits();
	}

	Result<bool> Manager::GetAddressAllowed(const WChar* addr_str, const CheckType check) noexcept
	{
		Address addr;
		if (Address::TryParse(addr_str, addr))
		{
			return GetAddressAllowed(addr, check);
		}

		return ResultCode::AddressInvalid;
	}

	Result<bool> Manager::GetAddressAllowed(const Address& addr, const CheckType check) noexcept
	{
		switch (check)
		{
			case CheckType::IPFilters:
			{
				if (addr.GetType() == Address::Type::IP)
				{
					return m_IPFilters.WithSharedLock()->GetAllowed(addr.GetIPAddress());
				}
				break;
			}
			case CheckType::AddressReputations:
			{
				return m_AddressAccessControl.WithUniqueLock()->HasAcceptableReputation(addr);
			}
			case CheckType::IPSubnetLimits:
			{
				if (addr.GetType() == Address::Type::IP)
				{
					return !m_SubnetLimits.WithSharedLock()->HasConnectionOverflow(addr.GetIPAddress());
				}
				break;
			}
			case CheckType::All:
			{
				switch (addr.GetType())
				{
					case Address::Type::IP:
					{
						if (const auto result = m_IPFilters.WithSharedLock()->GetAllowed(addr.GetIPAddress()); result)
						{
							if (*result &&
								m_AddressAccessControl.WithUniqueLock()->HasAcceptableReputation(addr) &&
								!m_SubnetLimits.WithSharedLock()->HasConnectionOverflow(addr.GetIPAddress()))
							{
								return true;
							}
						}
						break;
					}
					case Address::Type::BTH:
					{
						return m_AddressAccessControl.WithUniqueLock()->HasAcceptableReputation(addr);
					}
					default:
					{
						break;
					}
				}
				break;
			}
			default:
			{
				// Shouldn't ever get here
				assert(false);
				return ResultCode::InvalidArgument;
			}
		}

		// Blocked by default
		return false;
	}

	Result<bool> Manager::GetConnectionFromAddressAllowed(const Address& addr, const CheckType check) noexcept
	{
		switch (check)
		{
			case CheckType::IPFilters:
			{
				if (addr.GetType() == Address::Type::IP)
				{
					return m_IPFilters.WithSharedLock()->GetAllowed(addr.GetIPAddress());
				}
				break;
			}
			case CheckType::AddressReputations:
			{
				return m_AddressAccessControl.WithUniqueLock()->HasAcceptableReputation(addr);
			}
			case CheckType::IPSubnetLimits:
			{
				if (addr.GetType() == Address::Type::IP)
				{
					return m_SubnetLimits.WithSharedLock()->CanAcceptConnection(addr.GetIPAddress());
				}
				break;
			}
			case CheckType::All:
			{
				switch (addr.GetType())
				{
					case Address::Type::IP:
					{
						if (const auto result = m_IPFilters.WithSharedLock()->GetAllowed(addr.GetIPAddress()); result)
						{
							if (*result &&
								m_AddressAccessControl.WithUniqueLock()->HasAcceptableReputation(addr) &&
								m_SubnetLimits.WithSharedLock()->CanAcceptConnection(addr.GetIPAddress()))
							{
								return true;
							}
						}
						break;
					}
					case Address::Type::BTH:
					{
						return m_AddressAccessControl.WithUniqueLock()->HasAcceptableReputation(addr);
					}
					default:
					{
						break;
					}
				}
				break;
			}
			default:
			{
				// Shouldn't ever get here
				assert(false);
				break;
			}
		}

		// Blocked by default
		return false;
	}

	Result<> Manager::AddPeer(PeerSettings&& pas) noexcept
	{
		auto result = m_PeerAccessControl.WithUniqueLock()->AddPeer(std::move(pas));
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<> Manager::UpdatePeer(PeerSettings&& pas) noexcept
	{
		auto result = m_PeerAccessControl.WithUniqueLock()->UpdatePeer(std::move(pas));
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	Result<> Manager::RemovePeer(const PeerUUID& puuid) noexcept
	{
		auto result = m_PeerAccessControl.WithUniqueLock()->RemovePeer(puuid);
		if (result.Succeeded())
		{
			m_AccessUpdateCallbacks.WithUniqueLock()();
		}

		return result;
	}

	void Manager::RemoveAllPeers() noexcept
	{
		m_PeerAccessControl.WithUniqueLock()->Clear();
		m_AccessUpdateCallbacks.WithUniqueLock()();
	}

	Result<bool> Manager::GetPeerAllowed(const PeerUUID& puuid) const noexcept
	{
		return m_PeerAccessControl.WithSharedLock()->GetAllowed(puuid);
	}

	const ProtectedBuffer* Manager::GetPeerPublicKey(const PeerUUID& puuid) const noexcept
	{
		return m_PeerAccessControl.WithSharedLock()->GetPublicKey(puuid);
	}

	void Manager::SetPeerAccessDefault(const PeerAccessDefault pad) noexcept
	{
		m_PeerAccessControl.WithUniqueLock()->SetAccessDefault(pad);
		m_AccessUpdateCallbacks.WithUniqueLock()();
	}

	PeerAccessDefault Manager::GetPeerAccessDefault() const noexcept
	{
		return m_PeerAccessControl.WithSharedLock()->GetAccessDefault();
	}

	Result<Vector<PeerSettings>> Manager::GetAllPeers() const noexcept
	{
		return m_PeerAccessControl.WithSharedLock()->GetPeers();
	}
}