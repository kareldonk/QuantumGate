// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PublicIPEndpoints.h"
#include "..\Common\ScopeGuard.h"
#include "..\Crypto\Crypto.h"
#include "..\Common\Endian.h"
#include "..\Network\Ping.h"

using namespace std::literals;
using namespace QuantumGate::Implementation::Network;

namespace QuantumGate::Implementation::Core
{
	const bool PublicIPEndpoints::Initialize() noexcept
	{
		if (m_Initialized) return true;

		PreInitialize();

		if (!InitializeDataVerificationSockets())
		{
			LogErr(L"PublicIPEndpoints data verification sockets failed initialization");
			return false;
		}

		// Upon failure deinitialize sockets
		auto sg = MakeScopeGuard([&] { DeinitializeDataVerificationSockets(); });

		if (!m_ThreadPool.AddThread(L"QuantumGate PublicIPEndpoints DataVerification Thread",
									MakeCallback(this, &PublicIPEndpoints::DataVerificationWorkerThread)))
		{
			LogErr(L"Could not add PublicIPEndpoints data verification thread");
			return false;
		}

		if (!m_ThreadPool.AddThread(L"QuantumGate PublicIPEndpoints HopVerification Thread",
									MakeCallback(this, &PublicIPEndpoints::HopVerificationWorkerThread),
									&m_HopVerification.WithUniqueLock()->Queue.Event()))
		{
			LogErr(L"Could not add PublicIPEndpoints hop verification thread");
			return false;
		}

		m_ThreadPool.SetWorkerThreadsMaxBurst(64);
		m_ThreadPool.SetWorkerThreadsMaxSleep(1ms);

		if (!m_ThreadPool.Startup())
		{
			LogErr(L"PublicIPEndpoints threadpool initialization failed");
			return false;
		}

		sg.Deactivate();

		m_Initialized = true;

		return true;
	}

	void PublicIPEndpoints::Deinitialize() noexcept
	{
		if (!m_Initialized) return;

		m_ThreadPool.Shutdown();

		DeinitializeDataVerificationSockets();

		ResetState();

		m_Initialized = false;
	}

	void PublicIPEndpoints::PreInitialize() noexcept
	{
		ResetState();
	}

	void PublicIPEndpoints::ResetState() noexcept
	{
		m_ThreadPool.Clear();
		m_DataVerification.WithUniqueLock()->clear();
		m_IPEndpoints.WithUniqueLock()->clear();
		m_ReportingNetworks.clear();
	}

	[[nodiscard]] const bool PublicIPEndpoints::InitializeDataVerificationSockets() noexcept
	{
		auto success = true;
		auto nat_traversal = true;

		// Upon failure deinitialize sockets
		auto sg = MakeScopeGuard([&] { DeinitializeDataVerificationSockets(); });

		m_DataVerificationSockets.IPv4UDPSocket.WithUniqueLock([&](Network::Socket& socket)
		{
			auto tries = 0u;

			do
			{
				try
				{
					// Choose port randomly from dynamic port range
					m_DataVerificationSockets.Port = static_cast<UInt16>(Util::GetPseudoRandomNumber(49152, 65535));

					auto endpoint = IPEndpoint(IPAddress::AnyIPv4(), m_DataVerificationSockets.Port);
					socket = Network::Socket(endpoint.GetIPAddress().GetFamily(),
											 Network::Socket::Type::Datagram,
											 Network::IP::Protocol::UDP);

					if (socket.Bind(endpoint, nat_traversal))
					{
						success = true;
						break;
					}
					else
					{
						success = false;
						LogWarn(L"Could not bind public IP address data verification socket to endpoint %s",
								endpoint.GetString().c_str());
					}
				}
				catch (...) { success = false; }

				++tries;
			}
			while (tries < 3);
		});

		if (!success) return false;

		m_DataVerificationSockets.IPv6UDPSocket.WithUniqueLock([&](Network::Socket& socket)
		{
			try
			{
				auto endpoint = IPEndpoint(IPAddress::AnyIPv6(), m_DataVerificationSockets.Port);
				socket = Network::Socket(endpoint.GetIPAddress().GetFamily(),
										 Network::Socket::Type::Datagram,
										 Network::IP::Protocol::UDP);

				if (!socket.Bind(endpoint, nat_traversal))
				{
					success = false;
					LogWarn(L"Could not bind public IP address data verification socket to endpoint %s",
							endpoint.GetString().c_str());
				}
			}
			catch (...) { success = false; }
		});

		if (!success) return false;

		sg.Deactivate();

		return true;
	}

	void PublicIPEndpoints::DeinitializeDataVerificationSockets() noexcept
	{
		m_DataVerificationSockets.IPv4UDPSocket.WithUniqueLock([](Network::Socket& socket)
		{
			if (socket.GetIOStatus().IsOpen()) socket.Close();
		});

		m_DataVerificationSockets.IPv6UDPSocket.WithUniqueLock([](Network::Socket& socket)
		{
			if (socket.GetIOStatus().IsOpen()) socket.Close();
		});

		m_DataVerificationSockets.Port = 0;
	}

	const std::pair<bool, bool> PublicIPEndpoints::DataVerificationWorkerThread(const Concurrency::EventCondition& shutdown_event)
	{
		auto success = true;
		auto didwork = false;
		auto socket_error = false;

		const std::array<DataVerificationSockets::Socket_ThS*, 2> sockets
		{
			&m_DataVerificationSockets.IPv4UDPSocket,
			&m_DataVerificationSockets.IPv6UDPSocket
		};

		for (auto socket_ths : sockets)
		{
			// Check if we have a read event waiting for us
			if (socket_ths->WithUniqueLock()->UpdateIOStatus(0ms,
															 Socket::IOStatus::Update::Read |
															 Socket::IOStatus::Update::Exception))
			{
				if (socket_ths->WithSharedLock()->GetIOStatus().CanRead())
				{
					IPEndpoint sender_endpoint;
					std::optional<UInt64> num;

					socket_ths->WithUniqueLock([&](Network::Socket& socket)
					{
						Buffer buffer;

						if (socket.ReceiveFrom(sender_endpoint, buffer))
						{
							// Message should only contain a 64-bit number (8 bytes)
							if (buffer.GetSize() == sizeof(UInt64))
							{
								num = Endian::FromNetworkByteOrder(*reinterpret_cast<UInt64*>(buffer.GetBytes()));

								LogInfo(L"Received public IP address data verification (%llu) from endpoint %s",
										num.value(), sender_endpoint.GetString().c_str());
							}
							else
							{
								LogWarn(L"Received invalid public IP address data verification from endpoint %s",
										sender_endpoint.GetString().c_str());
							}
						}
						else
						{
							LogWarn(L"Failed to receive public IP address data verification from endpoint %s; the port may not be open",
									sender_endpoint.GetString().c_str());
						}
					});

					// If we received verification data, look it up and
					// see which IP address it should verify
					if (num.has_value())
					{
						m_DataVerification.WithUniqueLock([&](auto& verification_map)
						{
							if (const auto it = verification_map.find(*num); it != verification_map.end())
							{
								// Verification data should have been sent by the IP address that we
								// sent it to and expect, otherwise something is wrong (attack?)
								if (it->second.IPAddress == sender_endpoint.GetIPAddress().GetBinary())
								{
									m_IPEndpoints.WithUniqueLock([&](auto& ipendpoints)
									{
										if (const auto it2 = ipendpoints.find(it->second.IPAddress); it2 != ipendpoints.end())
										{
											it2->second.DataVerified = true;

											LogInfo(L"Verified public IP address %s",
													IPAddress(it->second.IPAddress).GetString().c_str());
										}
										else
										{
											// This should never happen
											LogErr(L"Failed to verify IP address %s; IP address not found in public endpoints",
												   IPAddress(it->second.IPAddress).GetString().c_str());
										}
									});

									verification_map.erase(it);
								}
								else
								{
									LogWarn(L"Received public IP address data verification (%llu) from endpoint %s, but expected it from IP address %s",
											num, sender_endpoint.GetString().c_str(), IPAddress(it->second.IPAddress).GetString().c_str());
								}
							}
							else
							{
								LogErr(L"Received unknown public IP address data verification (%llu) from endpoint %s",
									   num, sender_endpoint.GetString().c_str());
							}
						});
					}

					didwork = true;
				}
				else if (socket_ths->WithSharedLock()->GetIOStatus().HasException())
				{
					LogErr(L"Exception on public IP address data verification socket for endpoint %s (%s)",
						   socket_ths->WithSharedLock()->GetLocalEndpoint().GetString().c_str(),
						   GetSysErrorString(socket_ths->WithSharedLock()->GetIOStatus().GetErrorCode()).c_str());

					socket_error = true;
				}
			}
			else
			{
				LogErr(L"Failed to get status of public IP address data verification socket for endpoint %s",
					   socket_ths->WithSharedLock()->GetLocalEndpoint().GetString().c_str());

				socket_error = true;
			}
		}

		m_DataVerification.WithUniqueLock([&](auto& verification_map)
		{
			// We go through all pending data verifications and
			// retry and finally remove those that have timed out
			auto it = verification_map.begin();
			while (it != verification_map.end())
			{
				auto remove = false;

				switch (it->second.Status)
				{
					case DataVerification::Status::Registered:
					{
						if (SendIPAddressVerification(it->first, it->second))
						{
							didwork = true;
						}
						else remove = true;
						break;
					}
					case DataVerification::Status::VerificationSent:
					{
						if (Util::GetCurrentSteadyTime() - it->second.LastUpdateSteadyTime >= DataVerification::TimeoutPeriod)
						{
							if (it->second.NumVerificationTries < DataVerification::MaxVerificationTries)
							{
								// Retry verification
								if (SendIPAddressVerification(it->first, it->second))
								{
									didwork = true;
								}
								else remove = true;
							}
							else
							{
								LogErr(L"Public IP address data verification for %s timed out; this could be due to a firewall blocking incoming UDP traffic",
									   IPAddress(it->second.IPAddress).GetString().c_str());

								remove = true;
							}
						}
						break;
					}
					default:
					{
						break;
					}
				}

				if (remove) it = verification_map.erase(it);
				else ++it;
			}
		});

		// If we had an error on a socket, try to
		// restart them, and if this doesn't succeed
		// we'll exit the thread
		if (socket_error)
		{
			LogWarn(L"Attempting to restart PublicIPEndpoints data verification sockets due to errors");

			DeinitializeDataVerificationSockets();

			if (!InitializeDataVerificationSockets())
			{
				LogErr(L"PublicIPEndpoints data verification sockets failed initialization; will exit data verification thread");
				success = false;
			}
		}

		return std::make_pair(success, didwork);
	}

	const std::pair<bool, bool> PublicIPEndpoints::HopVerificationWorkerThread(const Concurrency::EventCondition& shutdown_event)
	{
		auto didwork = false;

		std::optional<HopVerification> hop_verification;

		m_HopVerification.IfUniqueLock([&](auto& verification_data)
		{
			if (!verification_data.Queue.Empty())
			{
				hop_verification = std::move(verification_data.Queue.Front());
				verification_data.Queue.Pop();

				// We had data in the queue
				// so we did work
				didwork = true;
			}
		});

		if (hop_verification.has_value())
		{
			auto verified = false;
			UInt8 hop{ 1 };

			// We ping the IP address with specific hop numbers to verify the distance
			// on the network. If the distance is small it's more likely that the
			// public IP address is the one we're using (ideally 1 or 2 hops away).
			// If the distance is further away then it may not be a public IP address
			// that we're using (and could be an attack).
			while (hop <= HopVerification::MaxHops && !shutdown_event.IsSet())
			{
				Ping ping(hop_verification->IPAddress,
						  std::chrono::duration_cast<std::chrono::milliseconds>(HopVerification::TimeoutPeriod),
						  static_cast<UInt16>(Util::GetPseudoRandomNumber(0, 256)), std::chrono::seconds(hop));
				if (ping.Execute() && ping.GetStatus() == Ping::Status::Succeeded &&
					ping.GetRespondingIPAddress() == hop_verification->IPAddress)
				{
					verified = true;
					break;
				}

				++hop;
			}

			if (verified)
			{
				m_IPEndpoints.WithUniqueLock([&](auto& ipendpoints)
				{
					if (const auto it = ipendpoints.find(hop_verification->IPAddress); it != ipendpoints.end())
					{
						it->second.HopVerified = true;

						LogInfo(L"Verified hops for public IP address %s",
								IPAddress(hop_verification->IPAddress).GetString().c_str());
					}
					else
					{
						// This should never happen
						LogErr(L"Failed to verify hops for IP address %s; IP address not found in public endpoints",
							   IPAddress(hop_verification->IPAddress).GetString().c_str());
					}
				});
			}
			else
			{
				LogWarn(L"Failed to verify hops for IP address %s; host may be further than %u hops away or behind a firewall",
						IPAddress(hop_verification->IPAddress).GetString().c_str(), HopVerification::MaxHops);
			}

			// Remove from the set so that the IP address can potentially
			// be added back to the queue if verification failed
			m_HopVerification.WithUniqueLock([&](auto& verification_data)
			{
				verification_data.Set.erase(hop_verification->IPAddress);
			});
		}

		return std::make_pair(true, didwork);
	}

	const bool PublicIPEndpoints::AddIPAddressDataVerification(const BinaryIPAddress& ip) noexcept
	{
		try
		{
			const auto num = Crypto::GetCryptoRandomNumber();
			if (num.has_value())
			{
				auto verification_map = m_DataVerification.WithUniqueLock();

				const auto[it, inserted] = verification_map->emplace(
					std::make_pair(*num, DataVerification{ DataVerification::Status::Registered,
								   ip, Util::GetCurrentSteadyTime() }));
				if (inserted)
				{
					// If this fails we'll try again in the worker thread
					if (SendIPAddressVerification(it->first, it->second))
					{
						return true;
					}
				}
				else
				{
					// A data verification record already existed
					// and is probably being worked on
					return true;
				}
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

	const bool PublicIPEndpoints::AddIPAddressHopVerification(const BinaryIPAddress& ip) noexcept
	{
		try
		{
			auto verification_data = m_HopVerification.WithUniqueLock();

			const auto result = verification_data->Set.emplace(ip);
			if (result.second)
			{
				// Upon failure to add to the queue, remove from the set
				auto sg = MakeScopeGuard([&] { verification_data->Set.erase(result.first); });

				verification_data->Queue.Push(HopVerification{ ip });

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

	const bool PublicIPEndpoints::SendIPAddressVerification(const UInt64 num, DataVerification& ip_verification) noexcept
	{
		// We send a random 64-bit number to the IP address to verify, and the port
		// that we're listening on locally. If the IP address is ours the random
		// number will be received by us and we'll have verified the address.

		try
		{
			DataVerificationSockets::Socket_ThS* socket_ths = (ip_verification.IPAddress.AddressFamily == BinaryIPAddress::Family::IPv4) ?
				&m_DataVerificationSockets.IPv4UDPSocket : &m_DataVerificationSockets.IPv6UDPSocket;

			IPEndpoint endpoint(ip_verification.IPAddress, m_DataVerificationSockets.Port);

			LogInfo(L"Sending public IP address data verification (%llu) to endpoint %s", num, endpoint.GetString().c_str());

			const UInt64 num_nbo = Endian::ToNetworkByteOrder(num);
			Buffer snd_buf(reinterpret_cast<const Byte*>(&num_nbo), sizeof(num_nbo));

			if (socket_ths->WithUniqueLock()->SendTo(endpoint, snd_buf))
			{
				if (snd_buf.IsEmpty())
				{
					ip_verification.Status = DataVerification::Status::VerificationSent;
					ip_verification.LastUpdateSteadyTime = Util::GetCurrentSteadyTime();
					++ip_verification.NumVerificationTries;
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

	Result<std::pair<bool, bool>> PublicIPEndpoints::AddIPEndpoint(const IPEndpoint& pub_endpoint,
																   const IPEndpoint& rep_peer,
																   const PeerConnectionType rep_con_type,
																   const bool trusted, const bool verified) noexcept
	{
		if (rep_con_type != PeerConnectionType::Unknown &&
			pub_endpoint.GetIPAddress().GetFamily() == rep_peer.GetIPAddress().GetFamily())
		{
			// Should be in public network address range
			if (!pub_endpoint.GetIPAddress().IsLocal() &&
				!pub_endpoint.GetIPAddress().IsMulticast() &&
				!pub_endpoint.GetIPAddress().IsReserved())
			{
				BinaryIPAddress network;
				const auto cidr = (rep_peer.GetIPAddress().GetFamily() == BinaryIPAddress::Family::IPv4) ?
					ReportingPeerNetworkIPv4CIDR : ReportingPeerNetworkIPv6CIDR;

				if (BinaryIPAddress::GetNetwork(rep_peer.GetIPAddress().GetBinary(), cidr, network))
				{
					if (AddReportingNetwork(network, trusted))
					{
						// Upon failure to add the public IP address details remove the network
						auto sg = MakeScopeGuard([&] { RemoveReportingNetwork(network); });

						auto ipendpoints = m_IPEndpoints.WithUniqueLock();

						const auto[pub_ipd, new_insert] =
							GetIPEndpointDetails(pub_endpoint.GetIPAddress().GetBinary(), *ipendpoints);
						if (pub_ipd != nullptr)
						{
							sg.Deactivate();

							pub_ipd->LastUpdateSteadyTime = Util::GetCurrentSteadyTime();

							if (trusted) pub_ipd->Trusted = true;

							if (verified)
							{
								pub_ipd->DataVerified = true;
								pub_ipd->HopVerified = true;
							}

							try
							{
								// Only interested in the port for inbound peers
								// so we know what public port they actually used
								// to connect to us
								if (rep_con_type == PeerConnectionType::Inbound &&
									pub_ipd->Ports.size() < MaxPortsPerIPAddress)
								{
									pub_ipd->Ports.emplace(pub_endpoint.GetPort());
								}

								if (pub_ipd->ReportingPeerNetworkHashes.size() < MaxReportingPeerNetworks)
								{
									pub_ipd->ReportingPeerNetworkHashes.emplace(network.GetHash());
								}

								if (!pub_ipd->DataVerified)
								{
									DiscardReturnValue(AddIPAddressDataVerification(pub_endpoint.GetIPAddress().GetBinary()));
								}

								if (!pub_ipd->HopVerified)
								{
									DiscardReturnValue(AddIPAddressHopVerification(pub_endpoint.GetIPAddress().GetBinary()));
								}

								return std::make_pair(true, new_insert);
							}
							catch (...) {}
						}
					}
					else return std::make_pair(false, false);
				}
			}
		}

		return ResultCode::Failed;
	}

	std::pair<PublicIPEndpointDetails*, bool>
		PublicIPEndpoints::GetIPEndpointDetails(const BinaryIPAddress& pub_ip, IPEndpointsMap& ipendpoints) noexcept
	{
		auto new_insert = false;
		PublicIPEndpointDetails* pub_ipd{ nullptr };

		if (const auto it = ipendpoints.find(pub_ip); it != ipendpoints.end())
		{
			pub_ipd = &it->second;
		}
		else
		{
			if (ipendpoints.size() >= MaxIPEndpoints)
			{
				RemoveLeastRecentIPEndpoints(ipendpoints.size() - MaxIPEndpoints, ipendpoints);
			}

			if (ipendpoints.size() < MaxIPEndpoints)
			{
				try
				{
					const auto[iti, inserted] = ipendpoints.emplace(
						std::make_pair(pub_ip, PublicIPEndpointDetails{}));

					pub_ipd = &iti->second;
					new_insert = inserted;
				}
				catch (...) {}
			}
		}

		return std::make_pair(pub_ipd, new_insert);
	}

	const bool PublicIPEndpoints::RemoveLeastRecentIPEndpoints(Size num, IPEndpointsMap& ipendpoints) noexcept
	{
		if (!ipendpoints.empty())
		{
			try
			{
				struct MinimalIPEndpointDetails
				{
					BinaryIPAddress IPAddress;
					bool Verified{ false };
					bool Trusted{ false };
					SteadyTime LastUpdateSteadyTime;
				};

				Vector<MinimalIPEndpointDetails> temp_endp;

				std::for_each(ipendpoints.begin(), ipendpoints.end(), [&](const auto& it)
				{
					temp_endp.emplace_back(
						MinimalIPEndpointDetails{
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

				DbgInvoke([&]()
				{
					for (auto& ep : temp_endp)
					{
						Dbg(L"%s - %s - %s - %llu",
							IPAddress(ep.IPAddress).GetString().c_str(),
							ep.Trusted ? L"Trusted" : L"Not Trusted",
							ep.Verified ? L"Verified" : L"Not verified",
							ep.LastUpdateSteadyTime.time_since_epoch().count());
					}
				});

				if (num > temp_endp.size()) num = temp_endp.size();

				// Remove first few items which should be
				// least trusted and least recent;
				for (Size x = 0; x < num; x++)
				{
					ipendpoints.erase((temp_endp.begin() + x)->IPAddress);
				}
			}
			catch (...) { return false; }
		}

		return true;
	}

	Result<> PublicIPEndpoints::AddIPAddresses(Vector<BinaryIPAddress>& ips, const bool only_trusted_verified) const noexcept
	{
		try
		{
			auto ipendpoints = m_IPEndpoints.WithSharedLock();

			for (const auto it : *ipendpoints)
			{
				if (only_trusted_verified && !(it.second.IsTrusted() || it.second.IsVerified()))
				{
					continue;
				}

				if (std::find(ips.begin(), ips.end(), it.first) == ips.end())
				{
					ips.emplace_back(it.first);
				}
			}

			return ResultCode::Succeeded;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not add public IP addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<> PublicIPEndpoints::AddIPAddresses(Vector<IPAddressDetails>& ips) const noexcept
	{
		try
		{
			auto ipendpoints = m_IPEndpoints.WithSharedLock();

			for (const auto it : *ipendpoints)
			{
				const auto it2 = std::find_if(ips.begin(), ips.end(), [&](const auto& ipd)
				{
					return (ipd.IPAddress.GetBinary() == it.first);
				});

				if (it2 == ips.end())
				{
					auto& ipdetails = ips.emplace_back();
					ipdetails.IPAddress = it.first;
					ipdetails.BoundToLocalEthernetInterface = false;

					ipdetails.PublicDetails.emplace();
					ipdetails.PublicDetails->ReportedByPeers = true;
					ipdetails.PublicDetails->ReportedByTrustedPeers = it.second.IsTrusted();
					ipdetails.PublicDetails->NumReportingNetworks = it.second.ReportingPeerNetworkHashes.size();
					ipdetails.PublicDetails->Verified = it.second.IsVerified();
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
			LogErr(L"Could not add public IP addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	const bool PublicIPEndpoints::IsNewReportingNetwork(const BinaryIPAddress& network) const noexcept
	{
		return (m_ReportingNetworks.find(network) == m_ReportingNetworks.end());
	}

	const bool PublicIPEndpoints::AddReportingNetwork(const BinaryIPAddress& network, const bool trusted) noexcept
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
		catch (...) {}

		return false;
	}

	void PublicIPEndpoints::RemoveReportingNetwork(const BinaryIPAddress& network) noexcept
	{
		m_ReportingNetworks.erase(network);
	}
}