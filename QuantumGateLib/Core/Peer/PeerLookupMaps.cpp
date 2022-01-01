// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerLookupMaps.h"
#include "PeerManager.h"
#include "..\..\Common\Hash.h"
#include "..\..\Common\Random.h"
#include "..\..\Common\ScopeGuard.h"

namespace QuantumGate::Implementation::Core::Peer
{
	bool LookupMaps::AddPeerData(const Data_ThS& data) noexcept
	{
		const auto pluid = data.WithSharedLock()->LUID;
		if (const auto it = m_PeerDataMap.find(pluid); it == m_PeerDataMap.end())
		{
			auto success = false;

			try
			{
				const auto [it2, inserted] = m_PeerDataMap.insert({ pluid, data });
				success = inserted;
			}
			catch (...) {}

			if (success)
			{
				// If anything fails below undo previous insert upon return
				auto sg1 = MakeScopeGuard([&]() noexcept
				{
					m_PeerDataMap.erase(pluid);
				});

				if (AddPeerEndpoint(pluid, data.WithSharedLock()->Cached.PeerEndpoint))
				{
					// If anything fails below undo previous add upon return
					auto sg2 = MakeScopeGuard([&]
					{
						if (!RemovePeerEndpoint(pluid, data.WithSharedLock()->Cached.PeerEndpoint))
						{
							LogErr(L"AddPeer() couldn't remove peer %llu after failing to add", pluid);
						}
					});

					if (AddPeerUUID(pluid, data.WithSharedLock()->PeerUUID))
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

	bool LookupMaps::RemovePeerData(const Data_ThS& data) noexcept
	{
		const auto pluid = data.WithSharedLock()->LUID;

		const auto success1 = RemovePeerUUID(pluid, data.WithSharedLock()->PeerUUID);
		const auto success2 = RemovePeerEndpoint(pluid, data.WithSharedLock()->Cached.PeerEndpoint);
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

	bool LookupMaps::AddPeerUUID(const PeerLUID pluid, const PeerUUID uuid) noexcept
	{
		if (const auto it = m_UUIDMap.find(uuid); it != m_UUIDMap.end())
		{
			return AddLUID(pluid, it->second);
		}
		else
		{
			try
			{
				const auto [it2, inserted] = m_UUIDMap.insert({ uuid, { pluid } });
				return inserted;
			}
			catch (...) {}
		}

		return false;
	}

	bool LookupMaps::RemovePeerUUID(const PeerLUID pluid, const PeerUUID uuid) noexcept
	{
		if (const auto it = m_UUIDMap.find(uuid); it != m_UUIDMap.end())
		{
			const auto success = RemoveLUID(pluid, it->second);

			if (it->second.empty()) m_UUIDMap.erase(it);

			return success;
		}

		return false;
	}

	bool LookupMaps::AddPeerEndpoint(const PeerLUID pluid, const Endpoint& endpoint) noexcept
	{
		if (AddPeerAddress(pluid, endpoint))
		{
			// If anything fails below undo previous add upon return
			auto sg = MakeScopeGuard([&]() noexcept
			{
				if (!RemovePeerAddress(pluid, endpoint))
				{
					LogErr(L"AddPeer() couldn't remove peer %llu after failing to add", pluid);
				}
			});

			if (AddPeer(pluid, GetEndpointHash(endpoint)))
			{
				sg.Deactivate();

				return true;
			}
		}

		return false;
	}

	bool LookupMaps::RemovePeerEndpoint(const PeerLUID pluid, const Endpoint& endpoint) noexcept
	{
		const auto success1 = RemovePeerAddress(pluid, endpoint);
		const auto success2 = RemovePeer(pluid, GetEndpointHash(endpoint));

		return (success1 && success2);
	}

	Result<PeerLUID> LookupMaps::GetPeer(const Endpoint& endpoint) const noexcept
	{
		// Check if we have a peer for the specified endpoint
		if (const auto it = m_EndpointMap.find(GetEndpointHash(endpoint)); it != m_EndpointMap.end())
		{
			// Find a peer that is connected
			const auto& luid_vector = it->second;
			for (const auto& luid : luid_vector)
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

		return ResultCode::PeerNotFound;
	}

	Result<PeerLUID> LookupMaps::GetRandomPeer(const Vector<PeerLUID>& excl_pluids,
											   const Vector<Address>& excl_addr1, const Vector<Address>& excl_addr2,
											   const UInt8 excl_network_cidr4, const UInt8 excl_network_cidr6) const noexcept
	{
		auto& addrmap = m_AddressMap;
		if (!addrmap.empty())
		{
			auto tries = 0u;

			// Try 3 times to get a random relay peer
			while (tries < 3)
			{
				const auto it = std::next(std::begin(addrmap),
										  static_cast<Size>(Random::GetPseudoRandomNumber(0, addrmap.size() - 1)));

				// Address should not be in exclude lists
				const auto result1 = AreAddressesInSameNetwork(it->first, excl_addr1, excl_network_cidr4, excl_network_cidr6);
				const auto result2 = AreAddressesInSameNetwork(it->first, excl_addr2, excl_network_cidr4, excl_network_cidr6);

				if (result1.Failed() || result2.Failed()) return ResultCode::Failed;

				if (!result1.GetValue() && !result2.GetValue())
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

				++tries;
			}

			// Couldn't get a peer randomly; try linear search
			for (const auto& it : addrmap)
			{
				// Address should not be in exclude lists
				const auto result1 = AreAddressesInSameNetwork(it.first, excl_addr1, excl_network_cidr4, excl_network_cidr6);
				const auto result2 = AreAddressesInSameNetwork(it.first, excl_addr2, excl_network_cidr4, excl_network_cidr6);

				if (result1.Failed() || result2.Failed()) return ResultCode::Failed;

				if (!result1.GetValue() && !result2.GetValue())
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

		return ResultCode::PeerNotFound;
	}

	Result<> LookupMaps::QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept
	{
		try
		{
			pluids.clear();

			for (const auto& it : GetPeerDataMap())
			{
				const auto result = it.second.WithSharedLock()->MatchQuery(params);
				if (result.Succeeded())
				{
					pluids.emplace_back(*result);
				}
			}

			return ResultCode::Succeeded;
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<API::Peer::Details> LookupMaps::GetPeerDetails(const PeerLUID pluid) const noexcept
	{
		if (const auto pdataths = GetPeerData(pluid); pdataths != nullptr)
		{
			return pdataths->WithSharedLock()->GetDetails();
		}

		return ResultCode::PeerNotFound;
	}

	bool LookupMaps::HasLUID(const PeerLUID pluid, const Vector<PeerLUID>& pluids) noexcept
	{
		return (std::find(pluids.begin(), pluids.end(), pluid) != pluids.end());
	}

	bool LookupMaps::HasIPEndpoint(const UInt64 hash, const Vector<IPEndpoint>& endpoints) noexcept
	{
		const auto it = std::find_if(endpoints.begin(), endpoints.end(),
									 [&](const IPEndpoint& endpoint) noexcept
		{
			return (GetEndpointHash(endpoint) == hash);
		});

		return (it != endpoints.end());
	}

	bool LookupMaps::HasIP(const BinaryIPAddress& ip, const Vector<BinaryIPAddress>& addresses) noexcept
	{
		return (std::find(addresses.begin(), addresses.end(), ip) != addresses.end());
	}

	Result<bool> LookupMaps::AreAddressesInSameNetwork(const Address& addr, const Vector<Address>& addresses,
													   const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept
	{
		if (addresses.size() > 0)
		{
			for (const auto& address : addresses)
			{
				const auto result = AreAddressesInSameNetwork(addr, address, cidr_lbits4, cidr_lbits6);
				if (result.Succeeded() && result.GetValue()) return true;
				else if (result.Failed()) return ResultCode::Failed;
			}
		}

		return false;
	}

	Result<bool> LookupMaps::AreAddressesInSameNetwork(const Address& addr1, const Address& addr2,
													   const UInt8 cidr_lbits4, const UInt8 cidr_lbits6) noexcept
	{
		switch (addr1.GetType())
		{
			case Address::Type::IP:
			{
				if (addr2.GetType() == Address::Type::IP)
				{
					const auto cidr_lbits = (addr1.GetIPAddress().GetFamily() == BinaryIPAddress::Family::IPv4) ? cidr_lbits4 : cidr_lbits6;

					const auto [success, same_network] = BinaryIPAddress::AreInSameNetwork(addr1.GetIPAddress().GetBinary(),
																						   addr2.GetIPAddress().GetBinary(), cidr_lbits);
					if (success && same_network) return true;
					else if (!success)
					{
						LogErr(L"AreAddressesInSameNetwork() couldn't compare addresses %s, %s and CIDR %u",
							   addr1.GetString().c_str(), addr2.GetString().c_str(), cidr_lbits);
						return ResultCode::Failed;
					}
				}
				break;
			}
			case Address::Type::BTH:
			{
				return (addr1 == addr2);
			}
			default:
			{
				break;
			}
		}

		return false;
	}

	UInt64 LookupMaps::GetEndpointHash(const Endpoint& endpoint) noexcept
	{
		struct HashData final
		{
			Endpoint::Type Type{ Endpoint::Type::Unspecified };
			IPEndpoint::Protocol IPProtocol{ IPEndpoint::Protocol::Unspecified };
			BinaryIPAddress IPAddress;
			BTHEndpoint::Protocol BTHProtocol{ BTHEndpoint::Protocol::Unspecified };
			BinaryBTHAddress BTHAddress;
			GUID BTHServiceClassID{ 0 };
			UInt16 Port{ 0 };
			RelayHop RelayHop{ 0 };
		};

		HashData data;
		MemInit(&data, sizeof(data)); // Needed to zero out padding bytes for consistent hash
		data.Type = endpoint.GetType();

		switch (endpoint.GetType())
		{
			case Endpoint::Type::IP:
			{
				const auto& ep = endpoint.GetIPEndpoint();

				data.IPProtocol = ep.GetProtocol();
				data.IPAddress = ep.GetIPAddress().GetBinary();
				data.Port = ep.GetPort();
				data.RelayHop = ep.GetRelayHop();
				break;
			}
			case Endpoint::Type::BTH:
			{
				const auto& ep = endpoint.GetBTHEndpoint();

				data.BTHProtocol = ep.GetProtocol();
				data.BTHAddress = ep.GetBTHAddress().GetBinary();
				data.BTHServiceClassID = ep.GetServiceClassID();
				data.Port = ep.GetPort();
				data.RelayHop = ep.GetRelayHop();
				break;
			}
			default:
			{
				// Shouldn't get here
				assert(false);
				break;
			}
		}

		return Hash::GetNonPersistentHash(BufferView(reinterpret_cast<Byte*>(&data), sizeof(data)));
	}

	bool LookupMaps::AddLUID(const PeerLUID pluid, LUIDVector& pluids) const noexcept
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

	bool LookupMaps::RemoveLUID(const PeerLUID pluid, LUIDVector& pluids) const noexcept
	{
		if (const auto& it = std::find(pluids.begin(), pluids.end(), pluid); it != pluids.end())
		{
			pluids.erase(it);
			return true;
		}

		return false;
	}

	bool LookupMaps::AddPeerAddress(const PeerLUID pluid, const Address& addr) noexcept
	{
		if (const auto it = m_AddressMap.find(addr); it != m_AddressMap.end())
		{
			return AddLUID(pluid, it->second);
		}
		else
		{
			try
			{
				[[maybe_unused]] const auto [it2, inserted] = m_AddressMap.insert({ addr, { pluid } });
				return inserted;
			}
			catch (...) {}
		}

		return false;
	}

	bool LookupMaps::RemovePeerAddress(const PeerLUID pluid, const Address& addr) noexcept
	{
		if (const auto it = m_AddressMap.find(addr); it != m_AddressMap.end())
		{
			const auto success = RemoveLUID(pluid, it->second);

			if (it->second.empty()) m_AddressMap.erase(it);

			return success;
		}

		return false;
	}

	bool LookupMaps::AddPeer(const PeerLUID pluid, const UInt64 hash) noexcept
	{
		if (const auto it = m_EndpointMap.find(hash); it != m_EndpointMap.end())
		{
			return AddLUID(pluid, it->second);
		}
		else
		{
			try
			{

				[[maybe_unused]] const auto [it2, inserted] = m_EndpointMap.insert({ hash, { pluid } });
				return inserted;
			}
			catch (...) {}
		}

		return false;
	}

	bool LookupMaps::RemovePeer(const PeerLUID pluid, const UInt64 hash) noexcept
	{
		if (const auto it = m_EndpointMap.find(hash); it != m_EndpointMap.end())
		{
			const auto success = RemoveLUID(pluid, it->second);

			if (it->second.empty()) m_EndpointMap.erase(it);

			return success;
		}

		return false;
	}
}