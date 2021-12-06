// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PublicEndpoints.h"
#include "..\Common\ScopeGuard.h"
#include "..\Crypto\Crypto.h"
#include "..\Common\Endian.h"
#include "..\Network\Ping.h"
#include "..\Network\NetworkUtils.h"

using namespace std::literals;
using namespace QuantumGate::Implementation::Network;

namespace QuantumGate::Implementation::Core
{
	bool PublicEndpoints::HopVerificationDetails::Verify(const bool has_locally_bound_pubip) noexcept
	{
		// We ping the IP address with specific maximum number of hops to verify the
		// distance on the network. If the distance is small it's more likely that the
		// public IP address is one that we're using (ideally 0 - 2 hops away).
		// If the distance is further away then it may not be a public IP address
		// that we're using (and could be an attack).
		const auto max_hops = std::invoke([&]() -> UInt8
		{
			if (has_locally_bound_pubip)
			{
				// We are directly connected to the Internet via a public IP
				// configured on a local ethernet interface, so we should reach
				// ourselves in zero hops
				return 0u;
			}

			return HopVerificationDetails::MaxHops;
		});

		Ping ping(IPAddress.GetBinary(), static_cast<UInt16>(Util::GetPseudoRandomNumber(0, 255)),
				  std::chrono::duration_cast<std::chrono::milliseconds>(HopVerificationDetails::TimeoutPeriod),
				  std::chrono::seconds(max_hops));

		if (ping.Execute() && ping.GetStatus() == Ping::Status::Succeeded &&
			ping.GetRespondingIPAddress() == IPAddress.GetBinary() &&
			ping.GetRoundTripTime() <= HopVerificationDetails::MaxRTT)
		{
			return true;
		}
		else
		{
			LogWarn(L"Failed to verify hops for IP address %s; host may be further than %u hops away or behind a firewall",
					IPAddress.GetString().c_str(), max_hops);
		}

		return false;
	}

	PublicEndpoints::DataVerificationDetails::DataVerificationDetails(const IPAddress& ip) noexcept :
		m_IPAddress(ip), m_StartSteadyTime(Util::GetCurrentSteadyTime())
	{}

	bool PublicEndpoints::DataVerificationDetails::InitializeSocket(const bool nat_traversal) noexcept
	{
		auto tries = 0u;

		// We try this a few times because the randomly chosen port might be in use
		// or there might be some other temporary issue
		do
		{
			try
			{
				// Choose port randomly from dynamic port range (RFC 6335)
				const auto port = static_cast<UInt16>(Util::GetPseudoRandomNumber(49152, 65535));

				const auto endpoint = IPEndpoint(IPEndpoint::Protocol::UDP,
												 (m_IPAddress.GetFamily() == IPAddress::Family::IPv4) ?
												 IPAddress::AnyIPv4() : IPAddress::AnyIPv6(), port);
				m_Socket = Network::Socket(endpoint.GetIPAddress().GetFamily() == IPAddress::Family::IPv4 ?
										   Network::AddressFamily::IPv4 : Network::AddressFamily::IPv6,
										   Network::Socket::Type::Datagram,
										   Network::Protocol::UDP);

				if (m_Socket.Bind(endpoint, nat_traversal))
				{
					return true;
				}
				else
				{
					LogWarn(L"Could not bind public IP address data verification socket to endpoint %s",
							endpoint.GetString().c_str());
				}
			}
			catch (...) {}

			++tries;
		}
		while (tries < 3);

		return false;
	}

	bool PublicEndpoints::DataVerificationDetails::SendVerification() noexcept
	{
		// We send a random 64-bit number to the IP address and the port
		// that we're listening on locally. If the IP address is ours the random
		// number will be received by us and we'll have partially verified the address.
		// An attacker could intercept and send the 64-bit number back to us, which
		// is why we also verify the number of hops between us and the IP address.

		try
		{
			const IPEndpoint endpoint(IPEndpoint::Protocol::UDP, m_IPAddress, m_Socket.GetLocalEndpoint().GetIPEndpoint().GetPort());

			const auto num = Crypto::GetCryptoRandomNumber();
			if (num.has_value())
			{
				m_ExpectedData = *num;

				LogInfo(L"Sending public IP address data verification (%llu) to endpoint %s",
						*num, endpoint.GetString().c_str());

				const UInt64 num_nbo = Endian::ToNetworkByteOrder(*num);
				Buffer snd_buf(reinterpret_cast<const Byte*>(&num_nbo), sizeof(num_nbo));

				const auto result = m_Socket.SendTo(endpoint, snd_buf);
				if (result.Succeeded() && *result == snd_buf.GetSize())
				{
					m_StartSteadyTime = Util::GetCurrentSteadyTime();
					return true;
				}
			}

			LogErr(L"Failed to send public IP address data verification to endpoint %s", endpoint.GetString().c_str());
		}
		catch (const std::exception& e)
		{
			LogErr(L"Failed to send public IP address data verification due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...)
		{
			LogErr(L"Failed to send public IP address data verification due to unknown exception");
		}

		return false;
	}

	Result<bool> PublicEndpoints::DataVerificationDetails::ReceiveVerification() noexcept
	{
		// Wait for read event on socket
		if (m_Socket.UpdateIOStatus(1s))
		{
			if (m_Socket.GetIOStatus().CanRead())
			{
				Endpoint sender_endpoint;
				std::optional<UInt64> num;
				Buffer rcv_buffer;

				const auto result = m_Socket.ReceiveFrom(sender_endpoint, rcv_buffer);
				if (result.Succeeded())
				{
					if (*result > 0)
					{
						// Message should only contain a 64-bit number (8 bytes)
						if (rcv_buffer.GetSize() == sizeof(UInt64))
						{
							num = Endian::FromNetworkByteOrder(*reinterpret_cast<UInt64*>(rcv_buffer.GetBytes()));

							LogInfo(L"Received public IP address data verification (%llu) from endpoint %s",
									num.value(), sender_endpoint.GetString().c_str());
						}
						else
						{
							LogWarn(L"Received invalid public IP address data verification from endpoint %s",
									sender_endpoint.GetString().c_str());
						}
					}
				}
				else
				{
					LogWarn(L"Failed to receive public IP address data verification from endpoint %s (%s)",
							sender_endpoint.GetString().c_str(), result.GetErrorString().c_str());
				}

				// If we received verification data
				if (num.has_value())
				{
					// Verification data should match and should have been sent by the IP address that we
					// sent it to and expect to hear from, otherwise something is wrong (attack?)
					if (m_ExpectedData == num &&
						m_IPAddress == sender_endpoint.GetIPEndpoint().GetIPAddress())
					{
						return true;
					}
					else
					{
						LogWarn(L"Received public IP address data verification (%llu) from endpoint %s, but expected %llu from IP address %s",
								num, sender_endpoint.GetString().c_str(), m_ExpectedData, m_IPAddress.GetString().c_str());
					}
				}
			}
			else if (m_Socket.GetIOStatus().HasException())
			{
				LogErr(L"Exception on public IP address data verification socket for endpoint %s (%s)",
					   m_Socket.GetLocalEndpoint().GetString().c_str(),
					   GetSysErrorString(m_Socket.GetIOStatus().GetErrorCode()).c_str());
				return ResultCode::Failed;
			}
		}
		else
		{
			LogErr(L"Failed to get status of public IP address data verification socket for endpoint %s",
				   m_Socket.GetLocalEndpoint().GetString().c_str());
			return ResultCode::Failed;
		}

		return false;
	}

	bool PublicEndpoints::DataVerificationDetails::Verify(const bool nat_traversal) noexcept
	{
		if (m_Status == Status::Initialized)
		{
			if (!InitializeSocket(nat_traversal) || !SendVerification())
			{
				m_Status = Status::Failed;
			}
			else
			{
				m_Status = Status::Verifying;
			}
		}

		if (m_Status == Status::Verifying)
		{
			const auto result = ReceiveVerification();
			if (result.Succeeded() && *result)
			{
				m_Status = Status::Succeeded;
			}
			else if (result.Failed())
			{
				m_Status = Status::Failed;
			}
		}

		if (m_Status == Status::Verifying && (Util::GetCurrentSteadyTime() - m_StartSteadyTime > TimeoutPeriod))
		{
			LogErr(L"Public IP address data verification for %s timed out; this could be due to a router/firewall blocking UDP traffic",
				   m_IPAddress.GetString().c_str());

			m_Status = Status::Timedout;
			return false;
		}

		if (m_Status == Status::Failed)
		{
			LogErr(L"Public IP address data verification failed for IP address %s",
				   m_IPAddress.GetString().c_str());
			return false;
		}

		return true;
	}

	bool PublicEndpoints::Initialize() noexcept
	{
		assert(!m_Initialized);

		if (m_Initialized) return true;

		PreInitialize();

		if (!m_ThreadPool.AddThread(L"QuantumGate PublicEndpoints DataVerification Thread",
									MakeCallback(this, &PublicEndpoints::DataVerificationWorkerThread),
									MakeCallback(this, &PublicEndpoints::DataVerificationWorkerThreadWait),
									MakeCallback(this, &PublicEndpoints::DataVerificationWorkerThreadWaitInterrupt)))
		{
			LogErr(L"Could not add PublicEndpoints data verification thread");
			return false;
		}

		if (!m_ThreadPool.AddThread(L"QuantumGate PublicEndpoints HopVerification Thread",
									MakeCallback(this, &PublicEndpoints::HopVerificationWorkerThread),
									MakeCallback(this, &PublicEndpoints::HopVerificationWorkerThreadWait),
									MakeCallback(this, &PublicEndpoints::HopVerificationWorkerThreadWaitInterrupt)))
		{
			LogErr(L"Could not add PublicEndpoints hop verification thread");
			return false;
		}

		if (!m_ThreadPool.Startup())
		{
			LogErr(L"PublicEndpoints threadpool initialization failed");
			return false;
		}

		m_Initialized = true;

		return true;
	}

	void PublicEndpoints::Deinitialize() noexcept
	{
		assert(m_Initialized);

		if (!m_Initialized) return;

		m_ThreadPool.Shutdown();

		ResetState();

		m_Initialized = false;
	}

	void PublicEndpoints::PreInitialize() noexcept
	{
		ResetState();
	}

	void PublicEndpoints::ResetState() noexcept
	{
		m_ThreadPool.Clear();

		m_DataVerification.Clear();
		m_HopVerification.Clear();

		m_Endpoints.WithUniqueLock()->clear();
		m_ReportingNetworks.clear();
	}

	void PublicEndpoints::DataVerificationWorkerThreadWait(const Concurrency::Event& shutdown_event)
	{
		m_DataVerification.Queue.Wait(shutdown_event);
	}

	void PublicEndpoints::DataVerificationWorkerThreadWaitInterrupt()
	{
		m_DataVerification.Queue.InterruptWait();
	}

	void PublicEndpoints::DataVerificationWorkerThread(const Concurrency::Event& shutdown_event)
	{
		std::optional<DataVerificationDetails> data_verification;

		m_DataVerification.Queue.PopFrontIf([&](auto& fdata) noexcept -> bool
		{
			data_verification = std::move(fdata);
			return true;
		});

		if (data_verification.has_value())
		{
			const auto& settings = m_Settings.GetCache();

			if (data_verification->Verify(settings.Local.Listeners.TCP.NATTraversal || settings.Local.Listeners.UDP.NATTraversal) &&
				data_verification->IsVerified())
			{
				m_Endpoints.WithUniqueLock([&](auto& ipendpoints)
				{
					if (const auto it = ipendpoints.find(data_verification->GetIPAddress()); it != ipendpoints.end())
					{
						it->second.DataVerified = true;

						LogInfo(L"Data verification succeeded for public IP address %s",
								data_verification->GetIPAddress().GetString().c_str());
					}
					else
					{
						// We should never get here
						LogErr(L"Failed to verify IP address %s; IP address not found in public endpoints",
							   data_verification->GetIPAddress().GetString().c_str());
					}
				});
			}

			if (data_verification->IsVerifying())
			{
				// Put at the back of the queue again so we can try again later
				m_DataVerification.Queue.Push(std::move(*data_verification));
			}
			else
			{
				// Remove from the set so that the IP address can potentially
				// be added back to the queue if verification failed
				m_DataVerification.Set.WithUniqueLock()->erase(data_verification->GetIPAddress());
			}
		}
	}

	void PublicEndpoints::HopVerificationWorkerThreadWait(const Concurrency::Event& shutdown_event)
	{
		m_HopVerification.Queue.Wait(shutdown_event);
	}

	void PublicEndpoints::HopVerificationWorkerThreadWaitInterrupt()
	{
		m_HopVerification.Queue.InterruptWait();
	}

	void PublicEndpoints::HopVerificationWorkerThread(const Concurrency::Event& shutdown_event)
	{
		std::optional<HopVerificationDetails> hop_verification;

		m_HopVerification.Queue.PopFrontIf([&](auto& fdata) noexcept -> bool
		{
			hop_verification = std::move(fdata);
			return true;
		});

		if (hop_verification.has_value())
		{
			if (hop_verification->Verify(HasLocallyBoundPublicIPAddress()))
			{
				m_Endpoints.WithUniqueLock([&](auto& ipendpoints)
				{
					if (const auto it = ipendpoints.find(hop_verification->IPAddress); it != ipendpoints.end())
					{
						it->second.HopVerified = true;

						LogInfo(L"Hop verification succeeded for public IP address %s",
								hop_verification->IPAddress.GetString().c_str());
					}
					else
					{
						// We should never get here
						LogErr(L"Failed to verify hops for IP address %s; IP address not found in public endpoints",
							   hop_verification->IPAddress.GetString().c_str());
					}
				});
			}

			// Remove from the set so that the IP address can potentially
			// be added back to the queue if verification failed
			m_HopVerification.Set.WithUniqueLock()->erase(hop_verification->IPAddress);
		}
	}

	bool PublicEndpoints::AddIPAddressDataVerification(const IPAddress& ip) noexcept
	{
		try
		{
			auto ipaddress_set = m_DataVerification.Set.WithUniqueLock();

			const auto result = ipaddress_set->emplace(ip);
			if (result.second)
			{
				// Upon failure to add to the queue, remove from the set
				auto sg = MakeScopeGuard([&]() noexcept { ipaddress_set->erase(result.first); });

				m_DataVerification.Queue.Push(DataVerificationDetails{ ip });

				sg.Deactivate();

				return true;
			}
			else
			{
				// A data verification record already existed
				// and is probably being worked on
				return true;
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Failed to add public IP address data verification due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...)
		{
			LogErr(L"Failed to add public IP address data verification due to unknown exception");
		}

		return false;
	}

	bool PublicEndpoints::AddIPAddressHopVerification(const IPAddress& ip) noexcept
	{
		try
		{
			auto ipaddress_set = m_HopVerification.Set.WithUniqueLock();

			const auto result = ipaddress_set->emplace(ip);
			if (result.second)
			{
				// Upon failure to add to the queue, remove from the set
				auto sg = MakeScopeGuard([&]() noexcept { ipaddress_set->erase(result.first); });

				m_HopVerification.Queue.Push(HopVerificationDetails{ ip });

				sg.Deactivate();

				return true;
			}
			else
			{
				// A hop verification record already existed
				// and is probably being worked on
				return true;
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Failed to add public IP address hop verification due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...)
		{
			LogErr(L"Failed to add public IP address hop verification due to unknown exception");
		}

		return false;
	}

	Result<std::pair<bool, bool>> PublicEndpoints::AddEndpoint(const Endpoint& pub_endpoint, const Endpoint& rep_peer,
															   const PeerConnectionType rep_con_type, const bool trusted,
															   const bool verified) noexcept
	{
		assert(pub_endpoint.GetType() == rep_peer.GetType());

		if (rep_con_type != PeerConnectionType::Unknown)
		{
			const auto get_network = [](const Endpoint& pub_endpoint, const Endpoint& rep_peer) noexcept -> std::optional<Address>
			{
				switch (pub_endpoint.GetType())
				{
					case Endpoint::Type::IP:
					{
						const auto& pub_ipep = pub_endpoint.GetIPEndpoint();
						const auto& rep_ipep = rep_peer.GetIPEndpoint();

						assert(pub_ipep.GetProtocol() == rep_ipep.GetProtocol());

						if (pub_ipep.GetIPAddress().GetFamily() == rep_ipep.GetIPAddress().GetFamily())
						{
							// Should be in public network address range
							if (pub_ipep.GetIPAddress().IsPublic())
							{
								const auto cidr = (rep_ipep.GetIPAddress().GetFamily() == IPAddress::Family::IPv4) ?
									ReportingPeerNetworkIPv4CIDR : ReportingPeerNetworkIPv6CIDR;

								BinaryIPAddress network;
								if (BinaryIPAddress::GetNetwork(rep_ipep.GetIPAddress().GetBinary(), cidr, network))
								{
									return IPAddress(network);
								}
							}
						}
						break;
					}
					case Endpoint::Type::BTH:
					{
						const auto& pub_bthep = pub_endpoint.GetBTHEndpoint();
						const auto& rep_bthep = rep_peer.GetBTHEndpoint();

						assert(pub_bthep.GetProtocol() == rep_bthep.GetProtocol());

						if (pub_bthep.GetBTHAddress().GetFamily() == rep_bthep.GetBTHAddress().GetFamily())
						{
							return rep_bthep.GetBTHAddress();
						}
						break;
					}
					default:
					{
						assert(false);
						break;
					}
				}

				return std::nullopt;
			};

			const auto network = get_network(pub_endpoint, rep_peer);
			if (network.has_value())
			{
				if (AddReportingNetwork(network.value(), trusted))
				{
					// Upon failure to add the public address details remove the network
					auto sg = MakeScopeGuard([&]() noexcept { RemoveReportingNetwork(network.value()); });

					auto endpoints = m_Endpoints.WithUniqueLock();

					const auto [pub_epd, new_insert] =
						GetEndpointDetails(pub_endpoint, *endpoints);
					if (pub_epd != nullptr)
					{
						sg.Deactivate();

						pub_epd->LastUpdateSteadyTime = Util::GetCurrentSteadyTime();

						if (trusted) pub_epd->Trusted = true;

						if (verified)
						{
							pub_epd->DataVerified = true;
							pub_epd->HopVerified = true;
						}

						try
						{
							// Only interested in the protocol and port for inbound peers
							// so we know what protocol and public port they actually used
							// to connect to us
							if (rep_con_type == PeerConnectionType::Inbound)
							{
								if (pub_epd->PortsMap.size() < MaxProtocolsPerAddress)
								{

									// If protocol does't exist it will get inserted
									auto& ports = pub_epd->PortsMap[GetEndpointNetworkProtocol(pub_endpoint)];
									if (ports.size() < MaxPortsPerProtocol)
									{
										ports.emplace(GetEndpointPort(pub_endpoint));
									}
								}
							}

							if (pub_epd->ReportingPeerNetworkHashes.size() < MaxReportingPeerNetworks)
							{
								pub_epd->ReportingPeerNetworkHashes.emplace(network->GetHash());
							}
						}
						catch (...) {}

						// Verification only for IP endpoints
						if (pub_endpoint.GetType() == Endpoint::Type::IP)
						{
							const auto& pub_ipep = pub_endpoint.GetIPEndpoint();

							if (!pub_epd->DataVerified)
							{
								DiscardReturnValue(AddIPAddressDataVerification(pub_ipep.GetIPAddress()));
							}

							if (!pub_epd->HopVerified)
							{
								DiscardReturnValue(AddIPAddressHopVerification(pub_ipep.GetIPAddress()));
							}
						}

						return std::make_pair(true, new_insert);
					}
				}
				else return std::make_pair(false, false);
			}
		}

		return ResultCode::Failed;
	}

	std::pair<PublicEndpointDetails*, bool>
		PublicEndpoints::GetEndpointDetails(const Network::Address& pub_addr, EndpointsMap& endpoints) noexcept
	{
		auto new_insert = false;
		PublicEndpointDetails* pub_epd{ nullptr };

		// If we already have a record for the address simply return
		// it, otherwise we'll add a new one below
		if (const auto it = endpoints.find(pub_addr); it != endpoints.end())
		{
			pub_epd = &it->second;
		}
		else
		{
			if (endpoints.size() >= MaxEndpoints)
			{
				// No room for new endpoints, so we need to remove the
				// ones that are least relevant before we can add a new one
				RemoveLeastRelevantEndpoints((endpoints.size() - MaxEndpoints) + 1, endpoints);
			}

			assert(endpoints.size() < MaxEndpoints);

			if (endpoints.size() < MaxEndpoints)
			{
				try
				{
					const auto [iti, inserted] = endpoints.emplace(
						std::make_pair(pub_addr, PublicEndpointDetails{}));

					pub_epd = &iti->second;
					new_insert = inserted;
				}
				catch (...)
				{
					LogErr(L"Failed to insert new public endpoint due to exception");
				}
			}
		}

		return std::make_pair(pub_epd, new_insert);
	}

	bool PublicEndpoints::RemoveLeastRelevantEndpoints(Size num, EndpointsMap& endpoints) noexcept
	{
		if (!endpoints.empty())
		{
			try
			{
				struct MinimalEndpointDetails final
				{
					Network::Address Address;
					bool Verified{ false };
					bool Trusted{ false };
					SteadyTime LastUpdateSteadyTime;
				};

				Vector<MinimalEndpointDetails> temp_endp;

				std::for_each(endpoints.begin(), endpoints.end(), [&](const auto& it)
				{
					temp_endp.emplace_back(
						MinimalEndpointDetails{
							it.first,
							it.second.IsVerified(),
							it.second.IsTrusted(),
							it.second.LastUpdateSteadyTime });
				});

				// Sort by least trusted and least recent
				std::sort(temp_endp.begin(), temp_endp.end(), [](const auto& a, const auto& b) noexcept
				{
					if (!a.Trusted && b.Trusted) return true;
					if (a.Trusted && !b.Trusted) return false;

					if (!a.Verified && b.Verified) return true;
					if (a.Verified && !b.Verified) return false;

					return (a.LastUpdateSteadyTime < b.LastUpdateSteadyTime);
				});

				DbgInvoke([&]() noexcept
				{
					Dbg(L"\r\nSorted EndpointDetails:");

					for (auto& ep : temp_endp)
					{
						Dbg(L"%s - %s - %s - %lld",
							ep.Address.GetString().c_str(),
							ep.Trusted ? L"Trusted" : L"Not Trusted",
							ep.Verified ? L"Verified" : L"Not verified",
							ep.LastUpdateSteadyTime.time_since_epoch().count());
					}

					Dbg(L"\r\n");
				});

				if (num > temp_endp.size()) num = temp_endp.size();

				// Remove first few items which should be
				// least trusted and least recent;
				for (auto it = temp_endp.begin(); it < temp_endp.begin() + num; ++it)
				{
					endpoints.erase(it->Address);
				}
			}
			catch (...)
			{
				LogErr(L"Failed to remove least relevant public endpoints due to exception");
				return false;
			}
		}

		return true;
	}

	Result<> PublicEndpoints::AddAddresses(Vector<Network::Address>& addrs, const bool only_trusted_verified) const noexcept
	{
		try
		{
			auto endpoints = m_Endpoints.WithSharedLock();

			for (const auto& it : *endpoints)
			{
				if (only_trusted_verified && !(it.second.IsTrusted() || it.second.IsVerified()))
				{
					continue;
				}

				if (std::find(addrs.begin(), addrs.end(), it.first) == addrs.end())
				{
					addrs.emplace_back(it.first);
				}
			}

			return ResultCode::Succeeded;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not add public addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...)
		{
			LogErr(L"Could not add public addresses due to unknown exception");
		}

		return ResultCode::Failed;
	}

	Result<> PublicEndpoints::AddAddresses(Vector<API::Local::Environment::AddressDetails>& addrs) const noexcept
	{
		try
		{
			auto endpoints = m_Endpoints.WithSharedLock();

			for (const auto& it : *endpoints)
			{
				const auto it2 = std::find_if(addrs.begin(), addrs.end(), [&](const auto& addrd)
				{
					return (addrd.Address == it.first);
				});

				if (it2 == addrs.end())
				{
					auto& adetails = addrs.emplace_back();
					adetails.Address = it.first;
					adetails.BoundToLocalInterface = false;

					adetails.PublicDetails.emplace();
					adetails.PublicDetails->ReportedByPeers = true;
					adetails.PublicDetails->ReportedByTrustedPeers = it.second.IsTrusted();
					adetails.PublicDetails->NumReportingNetworks = it.second.ReportingPeerNetworkHashes.size();
					adetails.PublicDetails->Verified = it.second.IsVerified();
				}
				else
				{
					// May be a locally configured IP that's also
					// publicly visible; add the public details 
					if (!it2->PublicDetails.has_value())
					{
						it2->PublicDetails.emplace();
						it2->PublicDetails->ReportedByPeers = true;
						it2->PublicDetails->ReportedByTrustedPeers = it.second.IsTrusted();
						it2->PublicDetails->NumReportingNetworks = it.second.ReportingPeerNetworkHashes.size();
						it2->PublicDetails->Verified = it.second.IsVerified();
					}
				}
			}

			return ResultCode::Succeeded;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not add public addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...)
		{
			LogErr(L"Could not add public addresses due to unknown exception");
		}

		return ResultCode::Failed;
	}

	bool PublicEndpoints::IsNewReportingNetwork(const Network::Address& network) const noexcept
	{
		return (m_ReportingNetworks.find(network) == m_ReportingNetworks.end());
	}

	bool PublicEndpoints::AddReportingNetwork(const Network::Address& network, const bool trusted) noexcept
	{
		if (!IsNewReportingNetwork(network))
		{
			if (!trusted) return false;
			else
			{
				// If the peer is trusted we are very much interested
				// in the public IP and port that it reports back to us
				// even if we already heard from the network it's on
				return true;
			}
		}

		while (m_ReportingNetworks.size() >= MaxReportingPeerNetworks)
		{
			// Remove the network that's least recent
			const auto it = std::min_element(m_ReportingNetworks.begin(), m_ReportingNetworks.end(),
											 [](const auto& a, const auto& b)
			{
				return (a.second < b.second);
			});

			m_ReportingNetworks.erase(it);
		}

		try
		{
			m_ReportingNetworks.emplace(std::make_pair(network, Util::GetCurrentSteadyTime()));
			return true;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not add reporting network due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...)
		{
			LogErr(L"Could not add reporting network due to unknown exception");
		}

		return false;
	}

	void PublicEndpoints::RemoveReportingNetwork(const Network::Address& network) noexcept
	{
		m_ReportingNetworks.erase(network);
	}
}