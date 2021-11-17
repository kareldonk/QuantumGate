// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Access.h"
#include "..\Core\Access\AccessManager.h"

namespace QuantumGate::API::Access
{
	Manager::Manager(QuantumGate::Implementation::Core::Access::Manager* accessmgr) noexcept :
		m_AccessManager(accessmgr)
	{
		assert(m_AccessManager != nullptr);
	}

	Result<IPFilterID> Manager::AddIPFilter(const WChar* ip_cidr,
												  const IPFilterType type) noexcept
	{
		return m_AccessManager->AddIPFilter(ip_cidr, type);
	}

	Result<IPFilterID> Manager::AddIPFilter(const WChar* ip_str, const WChar* mask_str,
												  const IPFilterType type) noexcept
	{
		return m_AccessManager->AddIPFilter(ip_str, mask_str, type);
	}

	Result<IPFilterID> Manager::AddIPFilter(const String& ip_str, const String& mask_str,
												  const IPFilterType type) noexcept
	{
		return m_AccessManager->AddIPFilter(ip_str.c_str(), mask_str.c_str(), type);
	}

	Result<IPFilterID> Manager::AddIPFilter(const IPAddress& ip, const IPAddress& mask,
												  const IPFilterType type) noexcept
	{
		return m_AccessManager->AddIPFilter(ip, mask, type);
	}

	Result<> Manager::RemoveIPFilter(const IPFilterID filterid, const IPFilterType type) noexcept
	{
		return m_AccessManager->RemoveIPFilter(filterid, type);
	}

	void Manager::RemoveAllIPFilters() noexcept
	{
		m_AccessManager->RemoveAllIPFilters();
	}

	Result<Vector<IPFilter>> Manager::GetAllIPFilters() const noexcept
	{
		return m_AccessManager->GetAllIPFilters();
	}

	Result<> Manager::AddIPSubnetLimit(const IPAddress::Family af, const String& cidr_lbits_str,
											 const Size max_con) noexcept
	{
		return m_AccessManager->AddIPSubnetLimit(af, cidr_lbits_str, max_con);
	}

	Result<> Manager::AddIPSubnetLimit(const IPAddress::Family af, const UInt8 cidr_lbits, const Size max_con) noexcept
	{
		return m_AccessManager->AddIPSubnetLimit(af, cidr_lbits, max_con);
	}

	Result<> Manager::RemoveIPSubnetLimit(const IPAddress::Family af, const String& cidr_lbits_str) noexcept
	{
		return m_AccessManager->RemoveIPSubnetLimit(af, cidr_lbits_str);
	}

	Result<> Manager::RemoveIPSubnetLimit(const IPAddress::Family af, const UInt8 cidr_lbits) noexcept
	{
		return m_AccessManager->RemoveIPSubnetLimit(af, cidr_lbits);
	}

	Result<Vector<IPSubnetLimit>> Manager::GetAllIPSubnetLimits() const noexcept
	{
		return m_AccessManager->GetAllIPSubnetLimits();
	}

	Result<> Manager::SetAddressReputation(const AddressReputation& ip_rep) noexcept
	{
		return m_AccessManager->SetAddressReputation(ip_rep);
	}

	Result<> Manager::ResetAddressReputation(const WChar* ip_str) noexcept
	{
		return m_AccessManager->ResetAddressReputation(ip_str);
	}

	Result<> Manager::ResetAddressReputation(const String& ip_str) noexcept
	{
		return m_AccessManager->ResetAddressReputation(ip_str.c_str());
	}

	Result<> Manager::ResetAddressReputation(const IPAddress& ip) noexcept
	{
		return m_AccessManager->ResetAddressReputation(ip);
	}

	void Manager::ResetAllAddressReputations() noexcept
	{
		m_AccessManager->ResetAllAddressReputations();
	}

	Result<Vector<AddressReputation>> Manager::GetAllAddressReputations() const noexcept
	{
		return m_AccessManager->GetAllAddressReputations();
	}

	Result<bool> Manager::GetAddressAllowed(const WChar* addr_str, const CheckType check) const noexcept
	{
		return m_AccessManager->GetAddressAllowed(addr_str, check);
	}

	Result<bool> Manager::GetAddressAllowed(const String& addr_str, const CheckType check) const noexcept
	{
		return m_AccessManager->GetAddressAllowed(addr_str.c_str(), check);
	}

	Result<bool> Manager::GetAddressAllowed(const Address& addr, const CheckType check) const noexcept
	{
		return m_AccessManager->GetAddressAllowed(addr, check);
	}

	Result<> Manager::AddPeer(PeerSettings&& pas) noexcept
	{
		return m_AccessManager->AddPeer(std::move(pas));
	}

	Result<> Manager::UpdatePeer(PeerSettings&& pas) noexcept
	{
		return m_AccessManager->UpdatePeer(std::move(pas));
	}

	Result<> Manager::RemovePeer(const PeerUUID& puuid) noexcept
	{
		return m_AccessManager->RemovePeer(puuid);
	}

	void Manager::RemoveAllPeers() noexcept
	{
		m_AccessManager->RemoveAllPeers();
	}

	Result<bool> Manager::GetPeerAllowed(const PeerUUID& puuid) const noexcept
	{
		return m_AccessManager->GetPeerAllowed(puuid);
	}

	void Manager::SetPeerAccessDefault(const PeerAccessDefault pad) noexcept
	{
		m_AccessManager->SetPeerAccessDefault(pad);
	}

	PeerAccessDefault Manager::GetPeerAccessDefault() const noexcept
	{
		return m_AccessManager->GetPeerAccessDefault();
	}

	Result<Vector<PeerSettings>> Manager::GetAllPeers() const noexcept
	{
		return m_AccessManager->GetAllPeers();
	}
}