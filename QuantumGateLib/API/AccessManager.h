// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Core\Access\IPFilters.h"

namespace QuantumGate::Implementation::Core::Access
{
	class Manager;
}

namespace QuantumGate::API
{
	class Export AccessManager
	{
	public:
		AccessManager() = delete;
		AccessManager(QuantumGate::Implementation::Core::Access::Manager* accessmgr) noexcept;
		AccessManager(const AccessManager&) = delete;
		AccessManager(AccessManager&&) = default;
		virtual ~AccessManager() = default;
		AccessManager& operator=(const AccessManager&) = delete;
		AccessManager& operator=(AccessManager&&) = default;

		Result<IPFilterID> AddIPFilter(const String& ip_cidr,
									   const IPFilterType type) noexcept;
		Result<IPFilterID> AddIPFilter(const String& ip, const String& mask,
									   const IPFilterType type) noexcept;

		Result<> RemoveIPFilter(const IPFilterID filterid, const IPFilterType type) noexcept;
		void RemoveAllIPFilters() noexcept;

		Result<std::vector<IPFilter>> GetAllIPFilters() const noexcept;

		Result<> AddIPSubnetLimit(const IPAddressFamily af, const String& cidr_lbits, const Size max_con) noexcept;
		Result<> RemoveIPSubnetLimit(const IPAddressFamily af, const String& cidr_lbits) noexcept;

		Result<std::vector<IPSubnetLimit>> GetAllIPSubnetLimits() const noexcept;

		Result<std::vector<IPReputation>> GetAllIPReputations() const noexcept;

		Result<bool> IsIPAllowed(const String& ip, const AccessCheck check) const noexcept;
		Result<bool> IsIPAllowed(const IPAddress& ip, const AccessCheck check) const noexcept;

		Result<> AddPeer(const PeerAccessSettings&& pas) noexcept;
		Result<> UpdatePeer(const PeerAccessSettings&& pas) noexcept;
		Result<> RemovePeer(const PeerUUID& puuid) noexcept;
		void RemoveAllPeers() noexcept;

		Result<bool> IsPeerAllowed(const PeerUUID& puuid) const noexcept;

		void SetPeerAccessDefault(const PeerAccessDefault pad) noexcept;
		[[nodiscard]] const PeerAccessDefault GetPeerAccessDefault() const noexcept;

		Result<std::vector<PeerAccessSettings>> GetAllPeers() const noexcept;

	private:
		QuantumGate::Implementation::Core::Access::Manager* m_AccessManager{ nullptr };
	};
}