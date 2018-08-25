// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Peer.h"

#include <unordered_map>

namespace QuantumGate::Implementation::Core::Peer
{
	class Manager;

	class LookupMaps
	{
	public:
		using LUIDVector = std::vector<PeerLUID>;

		// One to one relationship between PeerData and PeerLUID
		using PeerDataMap = std::unordered_map<PeerLUID, const Data_ThS&>;

		// One to many relationship between UUID and PeerLUID
		using UUIDMap = std::unordered_map<PeerUUID, LUIDVector>;

		// One to many relationship between IP address and PeerLUID
		using IPMap = std::unordered_map<BinaryIPAddress, LUIDVector>;

		// One to many relationship between IP address and port and PeerLUID
		using IPPortMap = std::unordered_map<UInt64, LUIDVector>;

		inline const PeerDataMap& GetPeerDataMap() const noexcept { return m_PeerDataMap; }
		inline const UUIDMap& GetUUIDMap() const noexcept { return m_UUIDMap; }
		inline const IPMap& GetIPMap() const noexcept { return m_IPMap; }
		inline const IPPortMap& GetIPPortMap() const noexcept { return m_IPPortMap; }

		[[nodiscard]] const bool AddPeerData(const Data_ThS& data) noexcept;
		[[nodiscard]] const bool RemovePeerData(const Data_ThS& data) noexcept;
		const Data_ThS* GetPeerData(const PeerLUID pluid) const noexcept;

		[[nodiscard]] inline const bool IsEmpty() const noexcept
		{
			return (m_UUIDMap.empty() && m_IPMap.empty() && m_IPPortMap.empty() && m_PeerDataMap.empty());
		}

		inline void Clear() noexcept
		{
			m_UUIDMap.clear();
			m_IPMap.clear();
			m_IPPortMap.clear();
			m_PeerDataMap.clear();
		}

		Result<PeerLUID> GetRandomPeer(const std::vector<PeerLUID>& excl_pluids,
									   const std::vector<BinaryIPAddress>& excl_addr,
									   const UInt8 excl_network_cidr4, const UInt8 excl_network_cidr6) const noexcept;

		Result<std::vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;

		Result<PeerDetails> GetPeerDetails(const PeerLUID pluid) const noexcept;

		static const UInt64 GetIPPortHash(const IPEndpoint& endpoint) noexcept;

		static Result<std::vector<BinaryIPAddress>> GetNetworks(const std::vector<BinaryIPAddress>& addresses,
																const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept;

		[[nodiscard]] static const bool HasLUID(const PeerLUID pluid, const std::vector<PeerLUID>& pluids) noexcept;
		[[nodiscard]] static const bool HasIPPort(const UInt64 hash, const std::vector<IPEndpoint>& endpoints) noexcept;
		[[nodiscard]] static const bool HasIP(const BinaryIPAddress& ip, const std::vector<BinaryIPAddress>& addresses) noexcept;

		static Result<bool> IsIPInNetwork(const BinaryIPAddress& ip,
										  const UInt8 cidr_lbits4, const UInt8 cidr_lbits6,
										  const std::vector<BinaryIPAddress>& networks) noexcept;

		static Result<bool> IsIPInNetwork(const BinaryIPAddress& ip, const std::vector<BinaryIPAddress>& addresses,
										  const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept;

		static Result<bool> AreIPsInSameNetwork(const BinaryIPAddress& ip1, const BinaryIPAddress& ip2,
												const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept;

	private:
		[[nodiscard]] const bool AddPeer(const PeerLUID pluid, const PeerUUID uuid) noexcept;
		[[nodiscard]] const bool RemovePeer(const PeerLUID pluid, const PeerUUID uuid) noexcept;

		[[nodiscard]] const bool AddPeer(const PeerLUID pluid, const IPEndpoint& endpoint) noexcept;
		[[nodiscard]] const bool RemovePeer(const PeerLUID pluid, const IPEndpoint& endpoint) noexcept;

		[[nodiscard]] const bool AddPeer(const PeerLUID pluid, const BinaryIPAddress& ip) noexcept;
		[[nodiscard]] const bool RemovePeer(const PeerLUID pluid, const BinaryIPAddress& ip) noexcept;

		[[nodiscard]] const bool AddPeer(const PeerLUID pluid, const UInt64 hash) noexcept;
		[[nodiscard]] const bool RemovePeer(const PeerLUID pluid, const UInt64 hash) noexcept;

		[[nodiscard]] const bool AddLUID(const PeerLUID pluid, LUIDVector& pluids) const noexcept;
		[[nodiscard]] const bool RemoveLUID(const PeerLUID pluid, LUIDVector& pluids) const noexcept;

	private:
		UUIDMap m_UUIDMap;
		IPMap m_IPMap;
		IPPortMap m_IPPortMap;
		PeerDataMap m_PeerDataMap;
	};

	using LookupMaps_ThS = Concurrency::ThreadSafe<LookupMaps, std::shared_mutex>;
}