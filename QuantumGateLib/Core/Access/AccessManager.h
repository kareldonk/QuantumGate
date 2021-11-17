// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IPFilters.h"
#include "AddressAccessControl.h"
#include "IPSubnetLimits.h"
#include "PeerAccessControl.h"
#include "..\..\Common\Dispatcher.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Manager;
}

namespace QuantumGate::Implementation::Core::Access
{
	class Manager final
	{
	public:
		using AccessUpdateCallbacks = Dispatcher<void() noexcept>;
		using AccessUpdateCallbackHandle = AccessUpdateCallbacks::FunctionHandle;
		using AccessUpdateCallbacks_ThS = Concurrency::ThreadSafe<AccessUpdateCallbacks, std::mutex>;

		Manager() = delete;
		Manager(const Settings_CThS& settings) noexcept;
		virtual ~Manager() = default;
		Manager(const Manager&) = delete;
		Manager(Manager&&) noexcept = default;
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) noexcept = default;

		Result<IPFilterID> AddIPFilter(const WChar* ip_cidr,
									   const IPFilterType type) noexcept;
		Result<IPFilterID> AddIPFilter(const WChar* ip_str, const WChar* mask_str,
									   const IPFilterType type) noexcept;
		Result<IPFilterID> AddIPFilter(const IPAddress& ip, const IPAddress& mask,
									   const IPFilterType type) noexcept;

		Result<> RemoveIPFilter(const IPFilterID filterid, const IPFilterType type) noexcept;
		void RemoveAllIPFilters() noexcept;

		Result<Vector<IPFilter>> GetAllIPFilters() const noexcept;

		Result<> SetAddressReputation(const Address& addr, const Int16 score,
									  const std::optional<Time>& time = std::nullopt) noexcept;
		Result<> SetAddressReputation(const AddressReputation& addr_rep) noexcept;
		Result<> ResetAddressReputation(const WChar* addr_str) noexcept;
		Result<> ResetAddressReputation(const Address& addr) noexcept;
		void ResetAllAddressReputations() noexcept;
		Result<std::pair<Int16, bool>> UpdateAddressReputation(const Address& addr,
															   const AddressReputationUpdate rep_update) noexcept;
		Result<Vector<AddressReputation>> GetAllAddressReputations() const noexcept;

		[[nodiscard]] bool AddConnectionAttempt(const Address& addr) noexcept;
		[[nodiscard]] bool AddRelayConnectionAttempt(const Address& addr) noexcept;

		Result<> AddIPSubnetLimit(const IPAddress::Family af, const String& cidr_lbits, const Size max_con) noexcept;
		Result<> AddIPSubnetLimit(const IPAddress::Family af, const UInt8 cidr_lbits, const Size max_con) noexcept;
		Result<> RemoveIPSubnetLimit(const IPAddress::Family af, const String& cidr_lbits) noexcept;
		Result<> RemoveIPSubnetLimit(const IPAddress::Family af, const UInt8 cidr_lbits) noexcept;

		Result<Vector<IPSubnetLimit>> GetAllIPSubnetLimits() const noexcept;

		[[nodiscard]] bool AddIPConnection(const IPAddress& ip) noexcept;
		[[nodiscard]] bool RemoveIPConnection(const IPAddress& ip) noexcept;

		Result<bool> GetAddressAllowed(const WChar* addr_str, const CheckType check) noexcept;
		Result<bool> GetAddressAllowed(const Address& addr, const CheckType check) noexcept;

		Result<bool> GetConnectionFromAddressAllowed(const Address& addr, const CheckType check) noexcept;

		Result<> AddPeer(PeerSettings&& pas) noexcept;
		Result<> UpdatePeer(PeerSettings&& pas) noexcept;
		Result<> RemovePeer(const PeerUUID& puuid) noexcept;
		void RemoveAllPeers() noexcept;

		Result<bool> GetPeerAllowed(const PeerUUID& puuid) const noexcept;

		const ProtectedBuffer* GetPeerPublicKey(const PeerUUID& puuid) const noexcept;

		void SetPeerAccessDefault(const PeerAccessDefault pad) noexcept;
		[[nodiscard]] PeerAccessDefault GetPeerAccessDefault() const noexcept;

		Result<Vector<PeerSettings>> GetAllPeers() const noexcept;

		inline AccessUpdateCallbacks_ThS& GetAccessUpdateCallbacks() noexcept { return m_AccessUpdateCallbacks; }

	private:
		const Settings_CThS& m_Settings;

		IPFilters_ThS m_IPFilters;
		AddressAccessControl_ThS m_AddressAccessControl{ m_Settings };
		IPSubnetLimits_ThS m_SubnetLimits;
		PeerAccessControl_ThS m_PeerAccessControl{ m_Settings };

		AccessUpdateCallbacks_ThS m_AccessUpdateCallbacks;
	};
}