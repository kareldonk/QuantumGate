// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Peer.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Manager;

	class LookupMaps final
	{
	public:
		using LUIDVector = Vector<PeerLUID>;

		// One to one relationship between PeerData and PeerLUID
		using PeerDataMap = Containers::UnorderedMap<PeerLUID, const Data_ThS&>;

		// One to many relationship between UUID and PeerLUID
		using UUIDMap = Containers::UnorderedMap<PeerUUID, LUIDVector>;

		// One to many relationship between address and PeerLUID
		using AddressMap = Containers::UnorderedMap<Address, LUIDVector>;

		// One to many relationship between address and port and PeerLUID
		using EndpointMap = Containers::UnorderedMap<UInt64, LUIDVector>;

		inline const PeerDataMap& GetPeerDataMap() const noexcept { return m_PeerDataMap; }
		inline const UUIDMap& GetUUIDMap() const noexcept { return m_UUIDMap; }
		inline const AddressMap& GetAddressMap() const noexcept { return m_AddressMap; }
		inline const EndpointMap& GetEndpointMap() const noexcept { return m_EndpointMap; }

		[[nodiscard]] bool AddPeerData(const Data_ThS& data) noexcept;
		[[nodiscard]] bool RemovePeerData(const Data_ThS& data) noexcept;
		const Data_ThS* GetPeerData(const PeerLUID pluid) const noexcept;

		[[nodiscard]] inline bool IsEmpty() const noexcept
		{
			return (m_UUIDMap.empty() && m_AddressMap.empty() && m_EndpointMap.empty() && m_PeerDataMap.empty());
		}

		inline void Clear() noexcept
		{
			m_UUIDMap.clear();
			m_AddressMap.clear();
			m_EndpointMap.clear();
			m_PeerDataMap.clear();
		}

		Result<PeerLUID> GetPeer(const Endpoint& endpoint) const noexcept;

		Result<PeerLUID> GetRandomPeer(const Vector<PeerLUID>& excl_pluids,
									   const Vector<Address>& excl_addr1, const Vector<Address>& excl_addr2,
									   const UInt8 excl_network_cidr4, const UInt8 excl_network_cidr6) const noexcept;

		Result<> QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept;

		Result<API::Peer::Details> GetPeerDetails(const PeerLUID pluid) const noexcept;

		static UInt64 GetEndpointHash(const Endpoint& endpoint) noexcept;

		[[nodiscard]] static bool HasLUID(const PeerLUID pluid, const Vector<PeerLUID>& pluids) noexcept;
		[[nodiscard]] static bool HasIPEndpoint(const UInt64 hash, const Vector<IPEndpoint>& endpoints) noexcept;
		[[nodiscard]] static bool HasIP(const BinaryIPAddress& ip, const Vector<BinaryIPAddress>& addresses) noexcept;

		static Result<bool> AreAddressesInSameNetwork(const Address& addr, const Vector<Address>& addresses,
													  const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept;

		static Result<bool> AreAddressesInSameNetwork(const Address& addr1, const Address& addr2,
													  const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept;

	private:
		[[nodiscard]] bool AddPeerUUID(const PeerLUID pluid, const PeerUUID uuid) noexcept;
		[[nodiscard]] bool RemovePeerUUID(const PeerLUID pluid, const PeerUUID uuid) noexcept;

		[[nodiscard]] bool AddPeerEndpoint(const PeerLUID pluid, const Endpoint& endpoint) noexcept;
		[[nodiscard]] bool RemovePeerEndpoint(const PeerLUID pluid, const Endpoint& endpoint) noexcept;

		[[nodiscard]] bool AddPeerAddress(const PeerLUID pluid, const Address& addr) noexcept;
		[[nodiscard]] bool RemovePeerAddress(const PeerLUID pluid, const Address& addr) noexcept;

		[[nodiscard]] bool AddPeer(const PeerLUID pluid, const UInt64 hash) noexcept;
		[[nodiscard]] bool RemovePeer(const PeerLUID pluid, const UInt64 hash) noexcept;

		[[nodiscard]] bool AddLUID(const PeerLUID pluid, LUIDVector& pluids) const noexcept;
		[[nodiscard]] bool RemoveLUID(const PeerLUID pluid, LUIDVector& pluids) const noexcept;

	private:
		UUIDMap m_UUIDMap;
		AddressMap m_AddressMap;
		EndpointMap m_EndpointMap;
		PeerDataMap m_PeerDataMap;
	};

	using LookupMaps_ThS = Concurrency::ThreadSafe<LookupMaps, std::shared_mutex>;
}