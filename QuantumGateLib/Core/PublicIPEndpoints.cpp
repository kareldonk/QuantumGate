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
	bool PublicIPEndpoints::HopVerificationDetails::Verify(const bool has_locally_bound_pubip) noexcept
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

		Ping ping(IPAddress, static_cast<UInt16>(Util::GetPseudoRandomNumber(0, 255)),
				  std::chrono::duration_cast<std::chrono::milliseconds>(HopVerificationDetails::TimeoutPeriod),
				  std::chrono::seconds(max_hops));

		if (ping.Execute() && ping.GetStatus() == Ping::Status::Succeeded &&
			ping.GetRespondingIPAddress() == IPAddress &&
			ping.GetRoundTripTime() <= HopVerificationDetails::MaxRTT)
		{
			return true;
		}
		else
		{
			LogWarn(L"Failed to verify hops for IP address %s; host may be further than %u hops away or behind a firewall",
					Network::IPAddress(IPAddress).GetString().c_str(), max_hops);
		}

		return false;
	}

	PublicIPEndpoints::DataVerificationDetails::DataVerificationDetails(const BinaryIPAddress ip) noexcept :
		m_IPAddress(ip), m_StartSteadyTime(Util::GetCurrentSteadyTime())
	{}

	bool PublicIPEndpoints::DataVerificationDetails::InitializeSocket(const bool nat_traversal) noexcept
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

				const auto endpoint = IPEndpoint((m_IPAddress.AddressFamily == BinaryIPAddress::Family::IPv4) ?
												 IPAddress::AnyIPv4() : IPAddress::AnyIPv6(), port);
				m_Socket = Network::Socket(endpoint.GetIPAddress().GetFamily(),
										   Network::Socket::Type::Datagram,
										   Network::IP::Protocol::UDP);

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

	bool PublicIPEndpoints::DataVerificationDetails::SendVerification() noexcept
	{
		// We send a random 64-bit number to the IP address and the port
		// that we're listening on locally. If the IP address is ours the random
		// number will be received by us and we'll have partially verified the address.
		// An attacker could intercept and send the 64-bit number back to us, which
		// is why we also verify the number of hops between us and the IP address.

		try
		{
			const IPEndpoint endpoint(m_IPAddress, m_Socket.GetLocalEndpoint().GetPort());

			const auto num = Crypto::GetCryptoRandomNumber();
			if (num.has_value())
			{
				m_ExpectedData = *num;

				LogInfo(L"Sending public IP address data verification (%llu) to endpoint %s",
						*num, endpoint.GetString().c_str());

				const UInt64 num_nbo = Endian::ToNetworkByteOrder(*num);
				Buffer snd_buf(reinterpret_cast<const Byte*>(&num_nbo), sizeof(num_nbo));

				if (m_Socket.SendTo(endpoint, snd_buf))
				{
					if (snd_buf.IsEmpty())
					{
						m_StartSteadyTime = Util::GetCurrentSteadyTime();
						return true;
					}
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

	Result<bool> PublicIPEndpoints::DataVerificationDetails::ReceiveVerification() noexcept
	{
		// Wait for read event on socket
		if (m_Socket.UpdateIOStatus(1s,
									Socket::IOStatus::Update::Read |
									Socket::IOStatus::Update::Exception))
		{
			if (m_Socket.GetIOStatus().CanRead())
			{
				IPEndpoint sender_endpoint;
				std::optional<UInt64> num;
				Buffer rcv_buffer;

				if (m_Socket.ReceiveFrom(sender_endpoint, rcv_buffer))
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
				else
				{
					LogWarn(L"Failed to receive public IP address data verification from endpoint %s; the port may not be open",
							sender_endpoint.GetString().c_str());
				}

				// If we received verification data
				if (num.has_value())
				{
					// Verification data should match and should have been sent by the IP address that we
					// sent it to and expect to hear from, otherwise something is wrong (attack?)
					if (m_ExpectedData == num &&
						m_IPAddress == sender_endpoint.GetIPAddress().GetBinary())
					{
						return true;
					}
					else
					{
						LogWarn(L"Received public IP address data verification (%llu) from endpoint %s, but expected %llu from IP address %s",
								num, sender_endpoint.GetString().c_str(), m_ExpectedData, Network::IPAddress(m_IPAddress).GetString().c_str());
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

	bool PublicIPEndpoints::DataVerificationDetails::Verify(const bool nat_traversal) noexcept
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
				   Network::IPAddress(m_IPAddress).GetString().c_str());

			m_Status = Status::Timedout;
			return false;
		}

		if (m_Status == Status::Failed)
		{
			LogErr(L"Public IP address data verification failed for IP address %s",
				   Network::IPAddress(m_IPAddress).GetString().c_str());
			return false;
		}

		return true;
	}

	bool PublicIPEndpoints::Initialize() noexcept
	{
		assert(!m_Initialized);

		if (m_Initialized) return true;

		PreInitialize();

		if (!m_ThreadPool.AddThread(L"QuantumGate PublicIPEndpoints DataVerification Thread",
									MakeCallback(this, &PublicIPEndpoints::DataVerificationWorkerThread),
									&m_DataVerification.WithUniqueLock()->Queue.Event()))
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

		const auto& settings = m_Settings.GetCache();

		m_ThreadPool.SetWorkerThreadsMaxBurst(settings.Local.Concurrency.WorkerThreadsMaxBurst);
		m_ThreadPool.SetWorkerThreadsMaxSleep(settings.Local.Concurrency.WorkerThreadsMaxSleep);

		if (!m_ThreadPool.Startup())
		{
			LogErr(L"PublicIPEndpoints threadpool initialization failed");
			return false;
		}

		m_Initialized = true;

		return true;
	}

	void PublicIPEndpoints::Deinitialize() noexcept
	{
		assert(m_Initialized);

		if (!m_Initialized) return;

		m_ThreadPool.Shutdown();

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

		m_DataVerification.WithUniqueLock()->Clear();
		m_HopVerification.WithUniqueLock()->Clear();

		m_IPEndpoints.WithUniqueLock()->clear();
		m_ReportingNetworks.clear();
	}

	const std::pair<bool, bool> PublicIPEndpoints::DataVerificationWorkerThread(const Concurrency::EventCondition& shutdown_event)
	{
		auto didwork = false;

		std::optional<DataVerificationDetails> data_verification;

		m_DataVerification.IfUniqueLock([&](auto& verification_data)
		{
			if (!verification_data.Queue.Empty())
			{
				data_verification = std::move(verification_data.Queue.Front());
				verification_data.Queue.Pop();

				// We had data in the queue
				// so we did work
				didwork = true;
			}
		});

		if (data_verification.has_value())
		{
			const auto& settings = m_Settings.GetCache();

			if (data_verification->Verify(settings.Local.NATTraversal) &&
				data_verification->IsVerified())
			{
				m_IPEndpoints.WithUniqueLock([&](auto& ipendpoints)
				{
					if (const auto it = ipendpoints.find(data_verification->GetIPAddress()); it != ipendpoints.end())
					{
						it->second.DataVerified = true;

						LogInfo(L"Data verification succeeded for public IP address %s",
								IPAddress(data_verification->GetIPAddress()).GetString().c_str());
					}
					else
					{
						// We should never get here
						LogErr(L"Failed to verify IP address %s; IP address not found in public endpoints",
							   IPAddress(data_verification->GetIPAddress()).GetString().c_str());
					}
				});
			}

			if (data_verification->IsVerifying())
			{
				// Put at the back of the queue again so we can try again later
				m_DataVerification.WithUniqueLock()->Queue.Push(std::move(*data_verification));
			}
			else
			{
				// Remove from the set so that the IP address can potentially
				// be added back to the queue if verification failed
				m_DataVerification.WithUniqueLock([&](auto& verification_data)
				{
					verification_data.Set.erase(data_verification->GetIPAddress());
				});
			}
		}

		return std::make_pair(true, didwork);
	}

	const std::pair<bool, bool> PublicIPEndpoints::HopVerificationWorkerThread(const Concurrency::EventCondition& shutdown_event)
	{
		auto didwork = false;

		std::optional<HopVerificationDetails> hop_verification;

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
			if (hop_verification->Verify(HasLocallyBoundPublicIPAddress()))
			{
				m_IPEndpoints.WithUniqueLock([&](auto& ipendpoints)
				{
					if (const auto it = ipendpoints.find(hop_verification->IPAddress); it != ipendpoints.end())
					{
						it->second.HopVerified = true;

						LogInfo(L"Hop verification succeeded for public IP address %s",
								IPAddress(hop_verification->IPAddress).GetString().c_str());
					}
					else
					{
						// We should never get here
						LogErr(L"Failed to verify hops for IP address %s; IP address not found in public endpoints",
							   IPAddress(hop_verification->IPAddress).GetString().c_str());
					}
				});
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

	bool PublicIPEndpoints::AddIPAddressDataVerification(const BinaryIPAddress& ip) noexcept
	{
		try
		{
			auto data_verification = m_DataVerification.WithUniqueLock();

			const auto result = data_verification->Set.emplace(ip);
			if (result.second)
			{
				// Upon failure to add to the queue, remove from the set
				auto sg = MakeScopeGuard([&] { data_verification->Set.erase(result.first); });

				data_verification->Queue.Push(DataVerificationDetails{ ip });

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

	bool PublicIPEndpoints::AddIPAddressHopVerification(const BinaryIPAddress& ip) noexcept
	{
		try
		{
			auto verification_data = m_HopVerification.WithUniqueLock();

			const auto result = verification_data->Set.emplace(ip);
			if (result.second)
			{
				// Upon failure to add to the queue, remove from the set
				auto sg = MakeScopeGuard([&] { verification_data->Set.erase(result.first); });

				verification_data->Queue.Push(HopVerificationDetails{ ip });

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

	Result<std::pair<bool, bool>> PublicIPEndpoints::AddIPEndpoint(const IPEndpoint& pub_endpoint,
																   const IPEndpoint& rep_peer,
																   const PeerConnectionType rep_con_type,
																   const bool trusted, const bool verified) noexcept
	{
		if (rep_con_type != PeerConnectionType::Unknown &&
			pub_endpoint.GetIPAddress().GetFamily() == rep_peer.GetIPAddress().GetFamily())
		{
			// Should be in public network address range
			if (pub_endpoint.GetIPAddress().IsPublic())
			{
				BinaryIPAddress network;
				const auto cidr = (rep_peer.GetIPAddress().GetFamily() == BinaryIPAddress::Family::IPv4) ?
					ReportingPeerNetworkIPv4CIDR : ReportingPeerNetworkIPv6CIDR;

				if (BinaryIPAddress::GetNetwork(rep_peer.GetIPAddress().GetBinary(), cidr, network))
				{
					if (AddReportingNetwork(network, trusted))
					{
						// Upon failure to add the public IP address details remove the network
						auto sg = MakeScopeGuard([&]() noexcept { RemoveReportingNetwork(network); });

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
							}
							catch (...) {}

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

		// If we already have a record for the IP address simply return
		// it, otherwise we'll add a new one below
		if (const auto it = ipendpoints.find(pub_ip); it != ipendpoints.end())
		{
			pub_ipd = &it->second;
		}
		else
		{
			if (ipendpoints.size() >= MaxIPEndpoints)
			{
				// No room for new IP endpoints, so we need to remove the
				// ones that are least relevant before we can add a new one
				RemoveLeastRelevantIPEndpoints((ipendpoints.size() - MaxIPEndpoints) + 1, ipendpoints);
			}

			assert(ipendpoints.size() < MaxIPEndpoints);

			if (ipendpoints.size() < MaxIPEndpoints)
			{
				try
				{
					const auto[iti, inserted] = ipendpoints.emplace(
						std::make_pair(pub_ip, PublicIPEndpointDetails{}));

					pub_ipd = &iti->second;
					new_insert = inserted;
				}
				catch (...)
				{
					LogErr(L"Failed to insert new public IP endpoint due to exception");
				}
			}
		}

		return std::make_pair(pub_ipd, new_insert);
	}

	bool PublicIPEndpoints::RemoveLeastRelevantIPEndpoints(Size num, IPEndpointsMap& ipendpoints) noexcept
	{
		if (!ipendpoints.empty())
		{
			try
			{
				struct MinimalIPEndpointDetails final
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

				DbgInvoke([&]() noexcept
				{
					Dbg(L"\r\nSorted IPEndpointDetails:");

					for (auto& ep : temp_endp)
					{
						Dbg(L"%s - %s - %s - %llu",
							IPAddress(ep.IPAddress).GetString().c_str(),
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
					ipendpoints.erase(it->IPAddress);
				}
			}
			catch (...)
			{
				LogErr(L"Failed to remove least relevant public IP endpoints due to exception");
				return false;
			}
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
		catch (...)
		{
			LogErr(L"Could not add public IP addresses due to unknown exception");
		}

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
		catch (...)
		{
			LogErr(L"Could not add public IP addresses due to unknown exception");
		}

		return ResultCode::Failed;
	}

	bool PublicIPEndpoints::IsNewReportingNetwork(const BinaryIPAddress& network) const noexcept
	{
		return (m_ReportingNetworks.find(network) == m_ReportingNetworks.end());
	}

	bool PublicIPEndpoints::AddReportingNetwork(const BinaryIPAddress& network, const bool trusted) noexcept
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

	void PublicIPEndpoints::RemoveReportingNetwork(const BinaryIPAddress& network) noexcept
	{
		m_ReportingNetworks.erase(network);
	}
}