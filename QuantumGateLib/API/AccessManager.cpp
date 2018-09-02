// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AccessManager.h"
#include "..\Core\Access\AccessManager.h"

namespace QuantumGate::API
{
	AccessManager::AccessManager(QuantumGate::Implementation::Core::Access::Manager* accessmgr) noexcept :
		m_AccessManager(accessmgr)
	{
		assert(m_AccessManager != nullptr);
	}

	Result<IPFilterID> AccessManager::AddIPFilter(const String& ip_cidr,
												  const IPFilterType type) noexcept
	{
		return m_AccessManager->AddIPFilter(ip_cidr, type);
	}

	Result<IPFilterID> AccessManager::AddIPFilter(const String& ip, const String& mask,
												  const IPFilterType type) noexcept
	{
		return m_AccessManager->AddIPFilter(ip, mask, type);
	}

	Result<> AccessManager::RemoveIPFilter(const IPFilterID filterid, const IPFilterType type) noexcept
	{
		return m_AccessManager->RemoveIPFilter(filterid, type);
	}

	void AccessManager::RemoveAllIPFilters() noexcept
	{
		m_AccessManager->RemoveAllIPFilters();
	}

	Result<std::vector<IPFilter>> AccessManager::GetAllIPFilters() const noexcept
	{
		return m_AccessManager->GetAllIPFilters();
	}

	Result<> AccessManager::AddIPSubnetLimit(const IPAddressFamily af, const String& cidr_lbits,
											 const Size max_con) noexcept
	{
		return m_AccessManager->AddIPSubnetLimit(af, cidr_lbits, max_con);
	}

	Result<> AccessManager::RemoveIPSubnetLimit(const IPAddressFamily af, const String& cidr_lbits) noexcept
	{
		return m_AccessManager->RemoveIPSubnetLimit(af, cidr_lbits);
	}

	Result<std::vector<IPSubnetLimit>> AccessManager::GetAllIPSubnetLimits() const noexcept
	{
		return m_AccessManager->GetAllIPSubnetLimits();
	}

	Result<> AccessManager::SetIPReputation(const IPReputation& ip_rep) noexcept
	{
		return m_AccessManager->SetIPReputation(ip_rep);
	}

	Result<> AccessManager::ResetIPReputation(const String& ip) noexcept
	{
		return m_AccessManager->ResetIPReputation(ip);
	}

	Result<> AccessManager::ResetIPReputation(const IPAddress& ip) noexcept
	{
		return m_AccessManager->ResetIPReputation(ip);
	}

	void AccessManager::ResetAllIPReputations() noexcept
	{
		m_AccessManager->ResetAllIPReputations();
	}

	Result<std::vector<IPReputation>> AccessManager::GetAllIPReputations() const noexcept
	{
		return m_AccessManager->GetAllIPReputations();
	}

	Result<bool> AccessManager::IsIPAllowed(const String& ip, const AccessCheck check) const noexcept
	{
		return m_AccessManager->IsIPAllowed(ip, check);
	}

	Result<bool> AccessManager::IsIPAllowed(const IPAddress& ip, const AccessCheck check) const noexcept
	{
		return m_AccessManager->IsIPAllowed(ip, check);
	}

	Result<> AccessManager::AddPeer(const PeerAccessSettings&& pas) noexcept
	{
		return m_AccessManager->AddPeer(std::move(pas));
	}

	Result<> AccessManager::UpdatePeer(const PeerAccessSettings&& pas) noexcept
	{
		return m_AccessManager->UpdatePeer(std::move(pas));
	}

	Result<> AccessManager::RemovePeer(const PeerUUID& puuid) noexcept
	{
		return m_AccessManager->RemovePeer(puuid);
	}

	void AccessManager::RemoveAllPeers() noexcept
	{
		m_AccessManager->RemoveAllPeers();
	}

	Result<bool> AccessManager::IsPeerAllowed(const PeerUUID& puuid) const noexcept
	{
		return m_AccessManager->IsPeerAllowed(puuid);
	}

	void AccessManager::SetPeerAccessDefault(const PeerAccessDefault pad) noexcept
	{
		m_AccessManager->SetPeerAccessDefault(pad);
	}

	const PeerAccessDefault AccessManager::GetPeerAccessDefault() const noexcept
	{
		return m_AccessManager->GetPeerAccessDefault();
	}

	Result<std::vector<PeerAccessSettings>> AccessManager::GetAllPeers() const noexcept
	{
		return m_AccessManager->GetAllPeers();
	}
}