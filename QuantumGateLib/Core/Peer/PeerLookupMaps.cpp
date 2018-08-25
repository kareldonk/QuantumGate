// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerLookupMaps.h"
#include "PeerManager.h"
#include "..\..\Common\Hash.h"
#include "..\..\Common\Random.h"
#include "..\..\Common\ScopeGuard.h"

namespace QuantumGate::Implementation::Core::Peer
{
	const bool LookupMaps::AddPeerData(const Data_ThS& data) noexcept
	{
		const auto pluid = data.WithSharedLock()->LUID;
		if (const auto it = m_PeerDataMap.find(pluid); it == m_PeerDataMap.end())
		{
			auto success = false;

			try
			{
				const auto[it2, inserted] = m_PeerDataMap.insert({ pluid, data });
				success = inserted;
			}
			catch (...) {}

			if (success)
			{
				// If anything fails below undo previous insert upon return
				auto sg1 = MakeScopeGuard([&]
				{
					m_PeerDataMap.erase(pluid);
				});

				if (AddPeer(pluid, data.WithSharedLock()->Cached.PeerEndpoint))
				{
					// If anything fails below undo previous add upon return
					auto sg2 = MakeScopeGuard([&]
					{
						if (!RemovePeer(pluid, data.WithSharedLock()->Cached.PeerEndpoint))
						{
							LogErr(L"AddPeer() couldn't remove peer %llu after failing to add", pluid);
						}
					});

					if (AddPeer(pluid, data.WithSharedLock()->PeerUUID))
					{
						sg1.Deactivate();
						sg2.Deactivate();

						return true;
					}
				}
			}
		}

		return false;
	}

	const bool LookupMaps::RemovePeerData(const Data_ThS& data) noexcept
	{
		const auto pluid = data.WithSharedLock()->LUID;
		const auto success1 = RemovePeer(pluid, data.WithSharedLock()->PeerUUID);
		const auto success2 = RemovePeer(pluid, data.WithSharedLock()->Cached.PeerEndpoint);
		const auto success3 = (m_PeerDataMap.erase(pluid) > 0);

		return (success1 && success2 && success3);
	}

	const Data_ThS* LookupMaps::GetPeerData(const PeerLUID pluid) const noexcept
	{
		if (const auto it = m_PeerDataMap.find(pluid); it != m_PeerDataMap.end())
		{
			return &it->second;
		}

		return nullptr;
	}

	const bool LookupMaps::AddPeer(const PeerLUID pluid, const PeerUUID uuid) noexcept
	{
		if (const auto it = m_UUIDMap.find(uuid); it != m_UUIDMap.end())
		{
			return AddLUID(pluid, it->second);
		}
		else
		{
			try
			{
				const auto[it2, inserted] = m_UUIDMap.insert({ uuid, { pluid } });
				return inserted;
			}
			catch (...) {}
		}

		return false;
	}

	const bool LookupMaps::RemovePeer(const PeerLUID pluid, const PeerUUID uuid) noexcept
	{
		if (const auto it = m_UUIDMap.find(uuid); it != m_UUIDMap.end())
		{
			const auto success = RemoveLUID(pluid, it->second);

			if (it->second.empty()) m_UUIDMap.erase(it);

			return success;
		}

		return false;
	}

	const bool LookupMaps::AddPeer(const PeerLUID pluid, const IPEndpoint& endpoint) noexcept
	{
		if (AddPeer(pluid, endpoint.GetIPAddress().GetBinary()))
		{
			// If anything fails below undo previous add upon return
			auto sg = MakeScopeGuard([&]
			{
				if (!RemovePeer(pluid, endpoint.GetIPAddress().GetBinary()))
				{
					LogErr(L"AddPeer() couldn't remove peer %llu after failing to add", pluid);
				}
			});

			if (AddPeer(pluid, GetIPPortHash(endpoint)))
			{
				sg.Deactivate();

				return true;
			}
		}

		return false;
	}

	const bool LookupMaps::RemovePeer(const PeerLUID pluid, const IPEndpoint& endpoint) noexcept
	{
		const auto success1 = RemovePeer(pluid, endpoint.GetIPAddress().GetBinary());
		const auto success2 = RemovePeer(pluid, GetIPPortHash(endpoint));

		return (success1 && success2);
	}

	Result<PeerLUID> LookupMaps::GetRandomPeer(const std::vector<PeerLUID>& excl_pluids,
											   const std::vector<BinaryIPAddress>& excl_addr,
											   const UInt8 excl_network_cidr4, const UInt8 excl_network_cidr6) const noexcept
	{
		auto& ipmap = m_IPMap;
		if (!ipmap.empty())
		{
			if (const auto result = GetNetworks(excl_addr,
												excl_network_cidr4, excl_network_cidr6); result.Succeeded())
			{
				const auto& excl_networks = result.Value();

				auto tries = 0u;

				// Try 3 times to get a random relay peer
				while (tries < 3)
				{
					const auto it = std::next(std::begin(ipmap),
											  static_cast<Size>(Random::GetPseudoRandomNumber(0, ipmap.size() - 1)));

					// IP should not be in exclude lists
					const auto result2 = IsIPInNetwork(it->first, excl_network_cidr4,
													   excl_network_cidr6, excl_networks);
					if (result2.Failed()) return ResultCode::Failed;

					if (!result2.Value() && !HasIP(it->first, excl_addr))
					{
						const auto it2 = std::next(std::begin(it->second),
												   static_cast<Size>(Random::GetPseudoRandomNumber(0, it->second.size() - 1)));

						const auto& luid = *it2;

						// LUID should not be in the exclude list
						if (!HasLUID(luid, excl_pluids))
						{
							// Peer should be in the ready state
							if (const auto peerths = GetPeerData(luid); peerths != nullptr)
							{
								if (peerths->WithSharedLock()->Status == Status::Ready)
								{
									return luid;
								}
							}
						}
					}

					tries++;
				}

				// Couldn't get a peer randomly; try linear search
				for (const auto& it : ipmap)
				{
					// IP should not be in exclude lists
					const auto result2 = IsIPInNetwork(it.first, excl_network_cidr4,
													   excl_network_cidr6, excl_networks);
					if (result2.Failed()) return ResultCode::Failed;

					if (!result2.Value() && !HasIP(it.first, excl_addr))
					{
						for (const auto& luid : it.second)
						{
							// LUID should not be in the exclude list
							if (!HasLUID(luid, excl_pluids))
							{
								// Peer should be in the ready state
								if (const auto peerths = GetPeerData(luid); peerths != nullptr)
								{
									if (peerths->WithSharedLock()->Status == Status::Ready)
									{
										return luid;
									}
								}
							}
						}
					}
				}
			}
			else return ResultCode::Failed;
		}

		return ResultCode::PeerNotFound;
	}

	Result<std::vector<PeerLUID>> LookupMaps::QueryPeers(const PeerQueryParameters& params) const noexcept
	{
		try
		{
			std::vector<PeerLUID> pluids;

			for (const auto& it : GetPeerDataMap())
			{
				const auto result = it.second.WithSharedLock()->MatchQuery(params);
				if (result.Succeeded())
				{
					pluids.emplace_back(*result);
				}
			}

			return std::move(pluids);
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<PeerDetails> LookupMaps::GetPeerDetails(const PeerLUID pluid) const noexcept
	{
		auto result_code = ResultCode::Failed;
		PeerDetails pdetails;

		if (const auto pdataths = GetPeerData(pluid); pdataths != nullptr)
		{
			pdataths->WithSharedLock([&](const Data& peer_data)
			{
				// Only if peer status is ready (handshake succeeded, etc.)
				if (peer_data.Status == Status::Ready)
				{
					pdetails.PeerUUID = peer_data.PeerUUID;
					pdetails.ConnectionType = peer_data.Type;
					pdetails.IsRelayed = peer_data.IsRelayed;
					pdetails.IsAuthenticated = peer_data.IsAuthenticated;
					pdetails.IsUsingGlobalSharedSecret = peer_data.IsUsingGlobalSharedSecret;
					pdetails.LocalIPEndpoint = peer_data.Cached.LocalEndpoint;
					pdetails.PeerIPEndpoint = peer_data.Cached.PeerEndpoint;
					pdetails.PeerProtocolVersion = peer_data.PeerProtocolVersion;
					pdetails.LocalSessionID = peer_data.LocalSessionID;
					pdetails.PeerSessionID = peer_data.PeerSessionID;
					pdetails.ConnectedTime = std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() -
																								   peer_data.Cached.ConnectedSteadyTime);
					pdetails.BytesReceived = peer_data.Cached.BytesReceived;
					pdetails.BytesSent = peer_data.Cached.BytesSent;
					pdetails.ExtendersBytesReceived = peer_data.ExtendersBytesReceived;
					pdetails.ExtendersBytesSent = peer_data.ExtendersBytesSent;

					result_code = ResultCode::Succeeded;
				}
				else result_code = ResultCode::PeerNotReady;
			});
		}
		else result_code = ResultCode::PeerNotFound;

		if (result_code == ResultCode::Succeeded) return std::move(pdetails);
		return result_code;
	}

	const bool LookupMaps::HasLUID(const PeerLUID pluid, const std::vector<PeerLUID>& pluids) noexcept
	{
		return (std::find(pluids.begin(), pluids.end(), pluid) != pluids.end());
	}

	const bool LookupMaps::HasIPPort(const UInt64 hash, const std::vector<IPEndpoint>& endpoints) noexcept
	{
		const auto it = std::find_if(endpoints.begin(), endpoints.end(),
									 [&](const IPEndpoint& endpoint)
		{
			return (GetIPPortHash(endpoint) == hash);
		});

		return (it != endpoints.end());
	}

	const bool LookupMaps::HasIP(const BinaryIPAddress& ip,
								 const std::vector<BinaryIPAddress>& addresses) noexcept
	{
		return (std::find(addresses.begin(), addresses.end(), ip) != addresses.end());
	}

	Result<bool> LookupMaps::IsIPInNetwork(const BinaryIPAddress& ip,
										   const UInt8 cidr_lbits4, const UInt8 cidr_lbits6,
										   const std::vector<BinaryIPAddress>& networks) noexcept
	{
		if (networks.size() > 0)
		{
			auto cidr_lbits = cidr_lbits4;
			if (ip.AddressFamily == IPAddressFamily::IPv6) cidr_lbits = cidr_lbits6;

			BinaryIPAddress subnet_mask;

			if (IPAddress::TryParseMask(ip.AddressFamily, cidr_lbits, subnet_mask))
			{
				auto ipnetwork = ip & subnet_mask;

				if (std::find(networks.begin(), networks.end(), ipnetwork) != networks.end())
				{
					return true;
				}
			}
			else
			{
				LogErr(L"IsIPInExcludedNetwork() couldn't parse subnet mask for IP address %s and CIDR %u",
					   IPAddress(ip).GetCString(), cidr_lbits);
				return ResultCode::Failed;
			}
		}

		return false;
	}

	Result<bool> LookupMaps::IsIPInNetwork(const BinaryIPAddress& ip, const std::vector<BinaryIPAddress>& addresses,
										   const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept
	{
		if (const auto result = GetNetworks(addresses, cidr_lbits4, cidr_lbits6); result.Succeeded())
		{
			const auto result2 = IsIPInNetwork(ip, cidr_lbits4, cidr_lbits6, result.Value());
			if (result2.Succeeded()) return result2.Value();
		}

		return ResultCode::Failed;
	}

	Result<bool> LookupMaps::AreIPsInSameNetwork(const BinaryIPAddress& ip1, const BinaryIPAddress& ip2,
												 const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept
	{
		return IsIPInNetwork(ip1, { ip2 }, cidr_lbits4, cidr_lbits6);
	}

	const UInt64 LookupMaps::GetIPPortHash(const IPEndpoint& endpoint) noexcept
	{
		struct IPPort
		{
			BinaryIPAddress IP;
			UInt16 Port{ 0 };
		};

		IPPort ipport;
		MemInit(&ipport, sizeof(ipport)); // Needed to zero out padding bytes for consistent hash
		ipport.IP = endpoint.GetIPAddress().GetBinary();
		ipport.Port = endpoint.GetPort();

		return Hash::GetNonPersistentHash(BufferView(reinterpret_cast<Byte*>(&ipport), sizeof(ipport)));
	}

	const bool LookupMaps::AddLUID(const PeerLUID pluid, LUIDVector& pluids) const noexcept
	{
		// If LUID exists there's a problem; it should be unique
		if (const auto& it = std::find(pluids.begin(), pluids.end(), pluid); it == pluids.end())
		{
			// If LUID doesn't exist add it
			try
			{
				pluids.emplace_back(pluid);
				return true;
			}
			catch (...) {}
		}
		else
		{
			LogErr(L"AddLUID() couldn't add LUID %llu because it already exists", pluid);
		}

		return false;
	}

	const bool LookupMaps::RemoveLUID(const PeerLUID pluid, LUIDVector& pluids) const noexcept
	{
		if (const auto& it = std::find(pluids.begin(), pluids.end(), pluid); it != pluids.end())
		{
			pluids.erase(it);
			return true;
		}

		return false;
	}

	const bool LookupMaps::AddPeer(const PeerLUID pluid, const BinaryIPAddress& ip) noexcept
	{
		if (const auto it = m_IPMap.find(ip); it != m_IPMap.end())
		{
			return AddLUID(pluid, it->second);
		}
		else
		{
			try
			{
				[[maybe_unused]] const auto[it2, inserted] = m_IPMap.insert({ ip, { pluid } });
				return inserted;
			}
			catch (...) {}
		}

		return false;
	}

	const bool LookupMaps::RemovePeer(const PeerLUID pluid, const BinaryIPAddress& ip) noexcept
	{
		if (const auto it = m_IPMap.find(ip); it != m_IPMap.end())
		{
			const auto success = RemoveLUID(pluid, it->second);

			if (it->second.empty()) m_IPMap.erase(it);

			return success;
		}

		return false;
	}

	const bool LookupMaps::AddPeer(const PeerLUID pluid, const UInt64 hash) noexcept
	{
		if (const auto it = m_IPPortMap.find(hash); it != m_IPPortMap.end())
		{
			return AddLUID(pluid, it->second);
		}
		else
		{
			try
			{

				[[maybe_unused]] const auto[it2, inserted] = m_IPPortMap.insert({ hash, { pluid } });
				return inserted;
			}
			catch (...) {}
		}

		return false;
	}

	const bool LookupMaps::RemovePeer(const PeerLUID pluid, const UInt64 hash) noexcept
	{
		if (const auto it = m_IPPortMap.find(hash); it != m_IPPortMap.end())
		{
			const auto success = RemoveLUID(pluid, it->second);

			if (it->second.empty()) m_IPPortMap.erase(it);

			return success;
		}

		return false;
	}

	Result<std::vector<BinaryIPAddress>> LookupMaps::GetNetworks(const std::vector<BinaryIPAddress>& addresses,
																 const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept
	{
		try
		{
			std::vector<BinaryIPAddress> excl_networks;

			for (const auto& addr : addresses)
			{
				BinaryIPAddress subnet_mask;

				auto cidr_lbits = cidr_lbits4;
				if (addr.AddressFamily == IPAddressFamily::IPv6) cidr_lbits = cidr_lbits6;

				if (IPAddress::TryParseMask(addr.AddressFamily, cidr_lbits, subnet_mask))
				{
					auto network = addr & subnet_mask;

					if (std::find(excl_networks.begin(), excl_networks.end(), network) == excl_networks.end())
					{
						excl_networks.emplace_back(network);
					}
				}
				else
				{
					LogErr(L"GetExcludedNetworks() couldn't parse subnet mask for IP address %s and CIDR %u",
						   IPAddress(addr).GetCString(), cidr_lbits);
					return ResultCode::Failed;
				}
			}

			return excl_networks;
		}
		catch (...) {}

		return ResultCode::Failed;
	}
}