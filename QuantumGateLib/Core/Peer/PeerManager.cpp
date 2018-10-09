// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerManager.h"
#include "..\..\Common\Random.h"
#include "..\..\Common\ScopeGuard.h"
#include "..\..\Memory\BufferReader.h"
#include "..\..\Memory\BufferWriter.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::Peer
{
	Manager::Manager(const Settings_CThS& settings, LocalEnvironment_ThS& environment,
					 KeyGeneration::Manager& keymgr, Access::Manager& accessmgr,
					 Extender::Manager& extenders) noexcept :
		m_Settings(settings), m_LocalEnvironment(environment), m_KeyGenerationManager(keymgr),
		m_AccessManager(accessmgr), m_ExtenderManager(extenders)
	{}

	const Settings& Manager::GetSettings() const noexcept
	{
		return m_Settings.GetCache();
	}

	const bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"Peermanager starting...");

		if (!(StartupThreadPools() && AddCallbacks()))
		{
			RemoveCallbacks();
			ShutdownThreadPools();

			LogErr(L"Peermanager startup failed");

			return false;
		}

		LogSys(L"Peermanager startup successful");

		m_Running = true;

		return true;
	}

	void Manager::Shutdown() noexcept
	{
		if (!m_Running) return;

		m_Running = false;

		LogSys(L"Peermanager shutting down...");

		RemoveCallbacks();
		ShutdownThreadPools();

		LogSys(L"Peermanager shut down");
	}

	const bool Manager::StartupRelays() noexcept
	{
		return m_RelayManager.Startup();
	}

	const void Manager::ShutdownRelays() noexcept
	{
		m_RelayManager.Shutdown();
	}

	const bool Manager::StartupThreadPools() noexcept
	{
		PreStartupThreadPools();

		Size numthreadpools{ 1 };
		Size numthreadsperpool{ 2 };

		const auto& settings = GetSettings();

		const auto cth = std::thread::hardware_concurrency();
		numthreadpools = (cth > settings.Local.Concurrency.MinThreadPools) ?
			cth : settings.Local.Concurrency.MinThreadPools;

		if (numthreadsperpool < settings.Local.Concurrency.MinThreadsPerPool)
		{
			numthreadsperpool = settings.Local.Concurrency.MinThreadsPerPool;
		}

		// Must have at least one thread pool, and at least two threads per pool 
		// one of which will be the primary thread
		assert(numthreadpools > 0 && numthreadsperpool > 1);

		LogSys(L"Creating %u peer %s with %u worker %s %s",
			   numthreadpools, numthreadpools > 1 ? L"threadpools" : L"threadpool",
			   numthreadsperpool, numthreadsperpool > 1 ? L"threads" : L"thread",
			   numthreadpools > 1 ? L"each" : L"");

		auto error = false;

		// Create the threadpools
		for (Size i = 0; i < numthreadpools; ++i)
		{
			try
			{
				auto thpool = std::make_unique<ThreadPool>();

				thpool->SetWorkerThreadsMaxBurst(settings.Local.Concurrency.WorkerThreadsMaxBurst);
				thpool->SetWorkerThreadsMaxSleep(settings.Local.Concurrency.WorkerThreadsMaxSleep);

				// Create the worker threads
				for (Size x = 0; x < numthreadsperpool; ++x)
				{
					// First thread is primary worker thread
					if (x == 0)
					{
						if (!thpool->AddThread(L"QuantumGate Peers Thread (Main)",
											   MakeCallback(this, &Manager::PrimaryThreadProcessor)))
						{
							error = true;
						}
					}
					else
					{
						if (!thpool->AddThread(L"QuantumGate Peers Thread",
											   MakeCallback(this, &Manager::WorkerThreadProcessor),
											   &thpool->GetData().Queue.WithUniqueLock()->Event()))
						{
							error = true;
						}
					}

					if (error) break;
				}

				if (!error && thpool->Startup())
				{
					m_ThreadPools[i] = std::move(thpool);
				}
				else
				{
					LogErr(L"Couldn't start a peers threadpool");
					error = true;
				}
			}
			catch (...) { error = true; }

			if (error) break;
		}

		return !error;
	}

	void Manager::ShutdownThreadPools() noexcept
	{
		for (const auto& thpool : m_ThreadPools)
		{
			thpool.second->Shutdown();
			thpool.second->Clear();
			thpool.second->GetData().Queue.WithUniqueLock()->Clear();
		}

		// Disconnect and remove all peers
		DisconnectAndRemoveAll();

		DbgInvoke([&]()
		{
			// If all threads are shut down, and peers
			// are cleared the peercount should be zero
			for (const auto& thpool : m_ThreadPools)
			{
				assert(thpool.second->GetData().PeerCollection.Count == 0);
			}
		});

		// If all peers were disconnected and our bookkeeping
		// was done right then the below should be true
		assert(m_LookupMaps.WithSharedLock()->IsEmpty());
		assert(m_AllPeers.Count == 0);
		assert(m_AllPeers.Map.WithSharedLock()->empty());

		ResetState();
	}

	void Manager::PreStartupThreadPools() noexcept
	{
		ResetState();
	}

	void Manager::ResetState() noexcept
	{
		m_LookupMaps.WithUniqueLock()->Clear();

		m_AllPeers.Count = 0;
		m_AllPeers.Map.WithUniqueLock()->clear();

		m_ThreadPools.clear();
	}

	const bool Manager::AddCallbacks() noexcept
	{
		auto success = true;

		m_AccessManager.GetAccessUpdateCallbacks().WithUniqueLock([&](auto& callbacks)
		{
			m_AccessUpdateCallbackHandle = callbacks.Add(MakeCallback(this, &Manager::OnAccessUpdate));
			if (!m_AccessUpdateCallbackHandle)
			{
				LogErr(L"Couldn't register 'AccessUpdateCallback' for peers");
				success = false;
			}
		});

		if (success)
		{
			m_ExtenderManager.GetExtenderUpdateCallbacks().WithUniqueLock([&](auto& callbacks)
			{

				m_ExtenderUpdateCallbackHandle = callbacks.Add(MakeCallback(this, &Manager::OnLocalExtenderUpdate));
				if (!m_ExtenderUpdateCallbackHandle)
				{

					LogErr(L"Couldn't register 'ExtenderUpdateCallback' for peers");
					success = false;
				}
			});
		}

		if (success)
		{
			m_ExtenderManager.GetUnhandledExtenderMessageCallbacks().WithUniqueLock([&](auto& callbacks)
			{
				m_UnhandledExtenderMessageCallbackHandle = callbacks.Add(MakeCallback(this,
																					  &Manager::OnUnhandledExtenderMessage));
				if (!m_UnhandledExtenderMessageCallbackHandle)
				{

					LogErr(L"Couldn't register 'UnhandledExtenderMessageCallback' for peers");
					success = false;
				}
			});
		}

		return success;
	}

	void Manager::RemoveCallbacks() noexcept
	{
		m_AccessManager.GetAccessUpdateCallbacks().WithUniqueLock([&](auto& callbacks)
		{
			callbacks.Remove(m_AccessUpdateCallbackHandle);
		});

		m_ExtenderManager.GetExtenderUpdateCallbacks().WithUniqueLock([&](auto& callbacks)
		{
			callbacks.Remove(m_ExtenderUpdateCallbackHandle);
		});

		m_ExtenderManager.GetUnhandledExtenderMessageCallbacks().WithUniqueLock([&](auto& callbacks)
		{
			callbacks.Remove(m_UnhandledExtenderMessageCallbackHandle);
		});
	}

	const std::pair<bool, bool> Manager::PrimaryThreadProcessor(ThreadPoolData& thpdata,
																const Concurrency::EventCondition& shutdown_event)
	{
		auto didwork = false;
		std::list<std::shared_ptr<Peer_ThS>> remove_list;

		const auto& settings = GetSettings();
		const auto noise_enabled = settings.Noise.Enabled;
		const auto max_handshake_duration = settings.Local.MaxHandshakeDuration;
		const auto max_connect_duration = settings.Local.ConnectTimeout;

		// Access check required?
		const auto luv = m_AllPeers.AccessUpdateFlag.load();
		if (thpdata.PeerCollection.AccessUpdateFlag != luv)
		{
			thpdata.PeerCollection.AccessUpdateFlag = luv;

			thpdata.PeerCollection.Map.WithSharedLock([&](const PeerMap& peers)
			{
				for (auto it = peers.begin(); it != peers.end(); ++it)
				{
					it->second->WithUniqueLock([&](Peer& peer)
					{
						peer.SetNeedsAccessCheck();
					});
				}
			});
		}

		thpdata.PeerCollection.Map.WithSharedLock([&](const PeerMap& peers)
		{
			for (auto it = peers.begin(); it != peers.end() && !shutdown_event.IsSet(); ++it)
			{
				auto& peerths = it->second;

				peerths->IfUniqueLock([&](Peer& peer)
				{
					// If the peer is already in the worker queue or thread, skip it
					if (peer.IsInQueue()) return;

					if (peer.CheckStatus(noise_enabled, max_connect_duration, max_handshake_duration))
					{
						if (peer.HasPendingEvents())
						{
							// If there are events to be processed add the peer to the queue;
							// Peer should not already be in queue if we get here
							assert(!peer.IsInQueue());

							Dbg(L"Adding peer %s to queue", peer.GetPeerName().c_str());

							thpdata.Queue.WithUniqueLock()->Push(peerths, [&]() noexcept { peer.SetInQueue(true); });

							didwork = true;
						}
					}

					// If we should disconnect for some reason
					if (peer.ShouldDisconnect())
					{
						Disconnect(peer, false);

						// Collect the peer for removal
						remove_list.emplace_back(peerths);
					}
				});
			}
		});

		// Remove all peers that were collected for removal
		if (!remove_list.empty())
		{
			LogDbg(L"Removing peers");
			Remove(remove_list);

			remove_list.clear();
			didwork = true;
		}

		return std::make_pair(true, didwork);
	}

	const std::pair<bool, bool> Manager::WorkerThreadProcessor(ThreadPoolData& thpdata,
															   const Concurrency::EventCondition& shutdown_event)
	{
		auto didwork = false;

		std::shared_ptr<Peer_ThS> peerths = nullptr;

		thpdata.Queue.IfUniqueLock([&](auto& queue)
		{
			if (!queue.Empty())
			{
				peerths = std::move(queue.Front());
				queue.Pop();

				// We had peers in the queue so we did work
				didwork = true;
			}
		});

		if (peerths != nullptr)
		{
			peerths->WithUniqueLock([&](Peer& peer)
			{
				peer.SetInQueue(false);

				if (peer.ProcessEvents())
				{
					// If we still have events waiting to be processed add the
					// peer back to the queue immediately to avoid extra delays
					if (peer.UpdateSocketStatus() && peer.HasPendingEvents())
					{
						// Peer should not already be in queue if we get here
						assert(!peer.IsInQueue());

						thpdata.Queue.WithUniqueLock()->Push(peerths, [&]() noexcept { peer.SetInQueue(true); });
					}
				}
			});
		}

		return std::make_pair(true, didwork);
	}

	std::shared_ptr<Peer_ThS> Manager::Get(const PeerLUID pluid) const noexcept
	{
		std::shared_ptr<Peer_ThS> rval(nullptr);

		m_AllPeers.Map.WithSharedLock([&](const PeerMap& peers)
		{
			const auto p = peers.find(pluid);
			if (p != peers.end()) rval = p->second;
		});

		return rval;
	}

	Result<PeerLUID> Manager::GetRelayPeer(const Vector<BinaryIPAddress>& excl_addr1,
										   const Vector<BinaryIPAddress>& excl_addr2) const noexcept
	{
		const auto& settings = GetSettings();
		return m_LookupMaps.WithSharedLock()->GetRandomPeer({}, excl_addr1, excl_addr2,
															settings.Relay.IPv4ExcludedNetworksCIDRLeadingBits,
															settings.Relay.IPv6ExcludedNetworksCIDRLeadingBits);
	}

	Result<bool> Manager::AreRelayIPsInSameNetwork(const BinaryIPAddress& ip1, const BinaryIPAddress& ip2) const noexcept
	{
		const auto& settings = GetSettings();
		return LookupMaps::AreIPsInSameNetwork(ip1, ip2,
											   settings.Relay.IPv4ExcludedNetworksCIDRLeadingBits,
											   settings.Relay.IPv6ExcludedNetworksCIDRLeadingBits);
	}

	Result<bool> Manager::AreRelayIPsInSameNetwork(const BinaryIPAddress& ip,
												   const Vector<BinaryIPAddress>& addresses) noexcept
	{
		const auto& settings = GetSettings();
		return LookupMaps::AreIPsInSameNetwork(ip, addresses,
											   settings.Relay.IPv4ExcludedNetworksCIDRLeadingBits,
											   settings.Relay.IPv6ExcludedNetworksCIDRLeadingBits);
	}

	std::shared_ptr<Peer_ThS> Manager::Create(const PeerConnectionType pctype,
											  std::optional<ProtectedBuffer>&& shared_secret) noexcept
	{
		try
		{
			auto peer = std::make_shared<Peer_ThS>(*this, GateType::Socket, pctype, std::move(shared_secret));
			if (peer->WithUniqueLock()->Initialize()) return peer;
		}
		catch (...) {}

		return nullptr;
	}

	std::shared_ptr<Peer_ThS> Manager::Create(const IPAddressFamily af, const Int32 type,
											  const Int32 protocol, const PeerConnectionType pctype,
											  std::optional<ProtectedBuffer>&& shared_secret) noexcept
	{
		try
		{
			auto peer = std::make_shared<Peer_ThS>(*this, af, type, protocol, pctype, std::move(shared_secret));
			if (peer->WithUniqueLock()->Initialize()) return peer;
		}
		catch (...) {}

		return nullptr;
	}

	std::shared_ptr<Peer_ThS> Manager::CreateRelay(const PeerConnectionType pctype,
												   std::optional<ProtectedBuffer>&& shared_secret) noexcept
	{
		try
		{
			auto peer = std::make_shared<Peer_ThS>(*this, GateType::RelaySocket, pctype, std::move(shared_secret));
			if (peer->WithUniqueLock()->Initialize()) return peer;
		}
		catch (...) {}

		return nullptr;
	}

	const bool Manager::Add(std::shared_ptr<Peer_ThS>& peerths) noexcept
	{
		auto success = false;

		peerths->WithUniqueLock([&](Peer& peer)
		{
			// Try to add connection to access manager; if this fails
			// the connection was not allowed
			if (m_AccessManager.AddIPConnection(peer.GetPeerIPAddress()))
			{
				PeerMap::iterator apit;

				try
				{
					// If this fails there was already a peer in the map (this should not happen)
					[[maybe_unused]] const auto[it, inserted] =
						m_AllPeers.Map.WithUniqueLock()->insert({ peer.GetLUID(), peerths });

					assert(inserted);
					if (!inserted)
					{
						LogErr(L"Couldn't add new peer; a peer with LUID %llu already exists", peer.GetLUID());
						return;
					}

					apit = it;
				}
				catch (...) { return; }

				++m_AllPeers.Count;

				auto sg = MakeScopeGuard([&]
				{
					m_AllPeers.Map.WithUniqueLock()->erase(apit);
					--m_AllPeers.Count;
				});

				// Get the threadpool with the least amount of peers so that the connections
				// eventually get distributed among all available pools
				const auto thpit = std::min_element(m_ThreadPools.begin(), m_ThreadPools.end(),
													[](const auto& a, const auto& b)
				{
					return (a.second->GetData().PeerCollection.Count <
							b.second->GetData().PeerCollection.Count);
				});

				assert(thpit != m_ThreadPools.end());

				// Add peer to the threadpool
				peer.SetThreadPoolKey(thpit->first);

				try
				{
					// If this fails there was already a peer in the map (this should not happen)
					[[maybe_unused]] const auto[it, inserted] =
						thpit->second->GetData().PeerCollection.Map.WithUniqueLock()->insert({ peer.GetLUID(), peerths });

					assert(inserted);
					if (!inserted)
					{
						LogErr(L"Couldn't add new peer; a peer with LUID %llu already exists", peer.GetLUID());
						return;
					}
				}
				catch (...) { return; }

				++thpit->second->GetData().PeerCollection.Count;

				sg.Deactivate();

				success = true;
			}
			else
			{
				LogErr(L"Couldn't add new peer with LUID %llu; IP address %s is not allowed",
					   peer.GetLUID(), peer.GetPeerIPAddress().GetString().c_str());
			}
		});

		return success;
	}

	void Manager::Remove(const Peer& peer) noexcept
	{
		if (m_AllPeers.Map.WithUniqueLock()->erase(peer.GetLUID()) > 0)
		{
			--m_AllPeers.Count;
		}

		const auto& thpool = m_ThreadPools[peer.GetThreadPoolKey()];

		if (thpool->GetData().PeerCollection.Map.WithUniqueLock()->erase(peer.GetLUID()) > 0)
		{
			--thpool->GetData().PeerCollection.Count;
		}
	}

	void Manager::Remove(const std::list<std::shared_ptr<Peer_ThS>>& peerlist) noexcept
	{
		m_AllPeers.Map.WithUniqueLock([&](PeerMap& peers)
		{
			for (const auto& peerths : peerlist)
			{
				Remove(*peerths->WithSharedLock());
			}
		});
	}

	void Manager::RemoveAll() noexcept
	{
		m_AllPeers.Map.WithUniqueLock()->clear();
		m_AllPeers.Count = 0;

		for (const auto& thpool : m_ThreadPools)
		{
			thpool.second->GetData().PeerCollection.Map.WithUniqueLock()->clear();
			thpool.second->GetData().PeerCollection.Count = 0;
		}
	}

	Result<> Manager::DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept
	{
		auto result_code = ResultCode::Failed;

		if (auto peerths = Get(pluid); peerths != nullptr)
		{
			peerths->WithUniqueLock([&](Peer& peer) noexcept
			{
				// Peer should not already be disconnected
				if (peer.GetStatus() != Status::Disconnected)
				{
					if (function) peer.AddDisconnectCallback(std::move(function));

					// Set the disconnect condition so that the peer
					// gets disconnected as soon as possible
					peer.SetDisconnectCondition(DisconnectCondition::DisconnectRequest);

					result_code = ResultCode::Succeeded;
				}
			});
		}
		else result_code = ResultCode::PeerNotFound;

		return result_code;
	}

	void Manager::Disconnect(Peer& peer, const bool graceful) noexcept
	{
		// Remove connection from access manager
		if (!m_AccessManager.RemoveIPConnection(peer.GetPeerIPAddress()))
		{
			LogErr(L"Could not remove connection for endpoint %s from access manager", peer.GetPeerName().c_str());
		}

		if (peer.GetIOStatus().IsOpen())
		{
			LogInfo(L"Disconnecting from endpoint %s", peer.GetPeerName().c_str());
			peer.Close(graceful);
		}
	}

	void Manager::DisconnectAndRemoveAll() noexcept
	{
		m_AllPeers.Map.WithSharedLock([&](const PeerMap& peers)
		{
			for (auto& it : peers)
			{
				it.second->WithUniqueLock([&](Peer& peer) noexcept
				{
					Disconnect(peer, false);
				});
			}
		});

		RemoveAll();
	}

	const bool Manager::Accept(std::shared_ptr<Peer_ThS>& peerths) noexcept
	{
		return Add(peerths);
	}

	Result<std::pair<PeerLUID, bool>> Manager::ConnectTo(ConnectParameters&& params, ConnectCallback&& function) noexcept
	{
		auto result_code = ResultCode::Failed;

		if (const auto allowed = m_AccessManager.IsIPConnectionAllowed(params.PeerIPEndpoint.GetIPAddress(),
																	   AccessCheck::All); allowed && *allowed)
		{
			if (params.Relay.Hops == 0)
			{
				auto reused = false;
				PeerLUID pluid{ 0 };

				auto peerths = Get(Peer::MakeLUID(params.PeerIPEndpoint));

				// If not yet connected, make new connection,
				// otherwise reuse existing connection
				if (peerths == nullptr)
				{
					LogInfo(L"Connecting to peer %s", params.PeerIPEndpoint.GetString().c_str());

					pluid = Peer::MakeLUID(params.PeerIPEndpoint);

					if (DirectConnectTo(std::move(params), std::move(function)))
					{
						result_code = ResultCode::Succeeded;
					}
				}
				else
				{
					peerths->WithUniqueLock([&](Peer& peer)
					{
						if ((peer.GetIOStatus().IsConnecting() || peer.GetIOStatus().IsConnected()) &&
							!peer.GetIOStatus().HasException())
						{
							LogDbg(L"Reusing existing connection to peer %s", peer.GetPeerName().c_str());

							result_code = ResultCode::Succeeded;
							pluid = peer.GetLUID();
							reused = true;
						}
						else
						{
							LogErr(L"Error on existing connection to peer %s; retry connecting", peer.GetPeerName().c_str());

							// Set the disconnect condition so that the peer gets disconnected as soon as possible
							peer.SetDisconnectCondition(DisconnectCondition::ConnectError);
							result_code = ResultCode::FailedRetry;
						}
					});
				}

				if (result_code == ResultCode::Succeeded) return { std::make_pair(pluid, reused) };
			}
			else
			{
				LogInfo(L"Connecting to peer %s (Relayed)", params.PeerIPEndpoint.GetString().c_str());

				return RelayConnectTo(std::move(params), std::move(function));
			}
		}
		else
		{
			LogErr(L"Could not connect to peer %s; IP address is not allowed", params.PeerIPEndpoint.GetString().c_str());
			result_code = ResultCode::NotAllowed;
		}

		return result_code;
	}

	const bool Manager::DirectConnectTo(ConnectParameters&& params, ConnectCallback&& function) noexcept
	{
		auto success = false;

		auto peerths = Create(params.PeerIPEndpoint.GetIPAddress().GetFamily(), SOCK_STREAM, IPPROTO_TCP,
							  PeerConnectionType::Outbound, std::move(params.GlobalSharedSecret));
		if (peerths != nullptr)
		{
			peerths->WithUniqueLock([&](Peer& peer)
			{
				if (function) peer.AddConnectCallback(std::move(function));

				if (peer.BeginConnect(params.PeerIPEndpoint))
				{
					if (Add(peerths))
					{
						success = true;
					}
					else peer.Close();
				}
			});
		}

		if (!success)
		{
			LogErr(L"Could not create connection to peer %s", params.PeerIPEndpoint.GetString().c_str());
		}

		return success;
	}

	Result<std::pair<PeerLUID, bool>> Manager::RelayConnectTo(ConnectParameters&& params,
															  ConnectCallback&& function) noexcept
	{
		assert(params.Relay.Hops > 0);

		auto reused = false;
		PeerLUID pluid{ 0 };
		auto result_code = ResultCode::Failed;
		String error_details;
		std::optional<PeerLUID> out_peer;
		auto out_reused = false;

		const auto rport = m_RelayManager.MakeRelayPort();
		if (rport)
		{
			try
			{
				if (params.Relay.Hops == 1)
				{
					const auto excl_addr = GetLocalIPAddresses();
					if (excl_addr != nullptr)
					{
						// Don't include addresses/network of local instance
						const auto result = AreRelayIPsInSameNetwork(params.PeerIPEndpoint.GetIPAddress().GetBinary(),
																	 *excl_addr);
						if (result.Succeeded())
						{
							if (!result.GetValue())
							{
								if (params.Relay.ViaPeer)
								{
									LogWarn(L"Ignoring via_peer parameter (LUID %llu) for relay link to peer %s because of single hop",
											params.Relay.ViaPeer.value(), params.PeerIPEndpoint.GetString().c_str());
								}

								// Connect to specific endpoint for final hop 0; if we're
								// already connected we'll use the existing connection; note that we specify
								// the same global shared secret since the destination is the same
								const auto retval = ConnectTo({ params.PeerIPEndpoint, params.GlobalSharedSecret }, nullptr);
								if (retval.Succeeded())
								{
									out_peer = retval->first;
									out_reused = retval->second;
								}
								else result_code = static_cast<ResultCode>(retval.GetErrorValue());
							}
							else error_details = L"the destination endpoint is on the same network as the local instance";
						}
						else error_details = L"couldn't check if the destination endpoint is on the same network as the local instance";
					}
					else error_details = L"couldn't get IP addresses of local instance";
				}
				else
				{
					const auto excl_addr1 = GetLocalIPAddresses();
					if (excl_addr1 != nullptr)
					{
						Vector<BinaryIPAddress> excl_addr2{ params.PeerIPEndpoint.GetIPAddress().GetBinary() };

						if (params.Relay.ViaPeer)
						{
							if (const auto via_peerths = Get(*params.Relay.ViaPeer); via_peerths != nullptr)
							{
								const auto via_peer_ip =
									via_peerths->WithSharedLock()->GetPeerEndpoint().GetIPAddress().GetBinary();

								// Don't include addresses/network of local instance
								const auto result1 = AreRelayIPsInSameNetwork(via_peer_ip, *excl_addr1);

								// Don't include the final endpoint/network
								const auto result2 = AreRelayIPsInSameNetwork(via_peer_ip, excl_addr2);

								if (result1.Succeeded() && result2.Succeeded())
								{
									if (!result1.GetValue() && !result2.GetValue())
									{
										out_peer = params.Relay.ViaPeer;
									}
									else error_details = Util::FormatString(L"cannot go through peer LUID %llu because it's on the same network as the local or destination endpoint",
																			params.Relay.ViaPeer.value());
								}
								else error_details = Util::FormatString(L"couldn't check if peer LUID %llu is on the same network as the local or destination endpoint",
																		params.Relay.ViaPeer.value());
							}
							else error_details = Util::FormatString(L"a peer with LUID %llu wasn't found", params.Relay.ViaPeer.value());
						}
						else
						{
							// Try to get a (random) peer for the hop in between
							// and don't include endpoints on excluded networks
							const auto result = GetRelayPeer(*excl_addr1, excl_addr2);
							if (result.Succeeded())
							{
								out_peer = result.GetValue();
							}
							else
							{
								if (result == ResultCode::PeerNotFound)
								{
									result_code = ResultCode::NoPeersForRelay;
									error_details = L"no peers available to create relay link";
								}
								else
								{
									error_details = L"failed to get a peer to create relay link";
								}
							}
						}
					}
					else error_details = L"couldn't get IP addresses of local instance";
				}
			}
			catch (...)
			{
				error_details = L"an exception was thrown";
			}

			if (out_peer)
			{
				if (auto in_peerths = CreateRelay(PeerConnectionType::Outbound,
												  std::move(params.GlobalSharedSecret)); in_peerths != nullptr)
				{
					in_peerths->WithUniqueLock([&](Peer& in_peer)
					{
						if (function) in_peer.AddConnectCallback(std::move(function));

						const auto out_endpoint = IPEndpoint(params.PeerIPEndpoint.GetIPAddress(),
															 params.PeerIPEndpoint.GetPort(), *rport, params.Relay.Hops);
						if (in_peer.BeginConnect(out_endpoint))
						{
							if (Add(in_peerths))
							{
								if (m_RelayManager.Connect(in_peer.GetLUID(), *out_peer,
														   out_endpoint, *rport, params.Relay.Hops))
								{
									pluid = in_peer.GetLUID();
									result_code = ResultCode::Succeeded;
								}
							}

							if (result_code != ResultCode::Succeeded) in_peer.Close();
						}
					});
				}

				// If creating relay failed and we made a new connection specifically
				// for this relay then we should close it since it's not needed
				if (result_code != ResultCode::Succeeded && params.Relay.Hops == 1 && !out_reused)
				{
					DiscardReturnValue(DisconnectFrom(*out_peer, nullptr));
				}
			}
		}
		else error_details = L"couldn't get relay port (relays may not be enabled)";

		if (result_code != ResultCode::Succeeded)
		{
			if (!error_details.empty()) error_details = L" - " + error_details;

			LogErr(L"Couldn't create relay link to peer %s%s",
				   params.PeerIPEndpoint.GetString().c_str(), error_details.c_str());
		}

		if (result_code == ResultCode::Succeeded) return { std::make_pair(pluid, reused) };

		return result_code;
	}

	Result<Vector<PeerLUID>> Manager::QueryPeers(const PeerQueryParameters& params) const noexcept
	{
		return m_LookupMaps.WithSharedLock()->QueryPeers(params);
	}

	Result<PeerDetails> Manager::GetPeerDetails(const PeerLUID pluid) const noexcept
	{
		return m_LookupMaps.WithSharedLock()->GetPeerDetails(pluid);
	}

	Result<> Manager::Broadcast(const MessageType msgtype, const Buffer& buffer, BroadcastCallback&& callback)
	{
		// If there are connections
		if (m_AllPeers.Count > 0)
		{
			m_AllPeers.Map.WithSharedLock([&](const PeerMap& peers)
			{
				for (const auto it : peers)
				{
					it.second->WithUniqueLock([&](Peer& peer)
					{
						auto broadcast_result = BroadcastResult::Succeeded;

						if (peer.IsReady())
						{
							// Note the copy
							auto bbuffer = buffer;
							if (!peer.Send(msgtype, std::move(bbuffer)))
							{
								broadcast_result = BroadcastResult::SendFailure;
							}
						}
						else broadcast_result = BroadcastResult::PeerNotReady;

						if (callback) callback(peer, broadcast_result);
					});
				}
			});
		}

		return ResultCode::Succeeded;
	}

	const Vector<BinaryIPAddress>* Manager::GetLocalIPAddresses() const noexcept
	{
		return m_LocalEnvironment.WithSharedLock()->GetCachedIPAddresses();
	}

	Result<> Manager::SendTo(const ExtenderUUID& extuuid, const std::atomic_bool& running,
							 PeerLUID pluid, Buffer&& buffer, const bool compress)
	{
		auto result_code = ResultCode::Failed;

		if (auto peerths = Get(pluid); peerths != nullptr)
		{
			peerths->WithUniqueLock([&](Peer& peer)
			{
				// Only if peer status is ready (handshake succeeded, etc.)
				if (peer.IsReady())
				{
					// If peer has extender installed and active
					if (peer.GetPeerExtenderUUIDs().HasExtender(extuuid))
					{
						// If local extender is still running
						if (running)
						{
							if (peer.Send(Message(MessageOptions(MessageType::ExtenderCommunication,
																 extuuid, std::move(buffer), compress))))
							{
								result_code = ResultCode::Succeeded;
							}
						}
						else result_code = ResultCode::NotRunning;
					}
					else result_code = ResultCode::PeerNoExtender;
				}
				else result_code = ResultCode::PeerNotReady;
			});
		}
		else result_code = ResultCode::PeerNotFound;

		return result_code;
	}

	Result<Buffer> Manager::GetExtenderUpdateData() const noexcept
	{
		const auto& lsextlist = m_ExtenderManager.GetActiveExtenderUUIDs().SerializedUUIDs;

		Memory::BufferWriter wrt(true);
		if (wrt.WriteWithPreallocation(Memory::WithSize(lsextlist, Memory::MaxSize::_65KB)))
		{
			return Buffer(wrt.MoveWrittenBytes());
		}

		return ResultCode::Failed;
	}

	const bool Manager::BroadcastExtenderUpdate()
	{
		// If there are no connections, don't bother
		if (m_AllPeers.Count == 0) return true;

		if (const auto result = GetExtenderUpdateData(); result.Succeeded())
		{
			const auto result2 = Broadcast(MessageType::ExtenderUpdate, *result,
										   [](Peer& peer, const BroadcastResult broadcast_result)
			{
				switch (broadcast_result)
				{
					case BroadcastResult::PeerNotReady:
						if (peer.IsInSessionInit())
						{
							// We'll need to send an extender update to the peer
							// when it gets in the ready state
							peer.SetNeedsExtenderUpdate();

							LogDbg(L"Couldn't broadcast ExtenderUpdate message to peer LUID %llu; will send update when it gets in ready state", peer.GetLUID());
						}
						break;
					default:
						break;
				}
			});

			if (result2.Succeeded())
			{
				LogInfo(L"Broadcasted ExtenderUpdate to peers");
				return true;
			}
			else LogErr(L"Couldn't broadcast ExtenderUpdate message to peers");
		}
		else LogErr(L"Couldn't prepare ExtenderUpdate message data for peers");

		return false;
	}

	void Manager::OnAccessUpdate() noexcept
	{
		assert(m_Running);

		// This function should not update peers directly since
		// it can get called by all kinds of outside threads and
		// could cause deadlocks. We use a shared update flag instead.
		// Increment the update flag; the change is simply used
		// as an indication that we need to check all peers
		// again for access rights in the main worker threads.
		++m_AllPeers.AccessUpdateFlag;
	}

	void Manager::OnLocalExtenderUpdate(const Vector<ExtenderUUID>& extuuids, const bool added)
	{
		assert(m_Running);

		// If there are no connections, don't bother
		if (m_AllPeers.Count == 0) return;

		if (added)
		{
			// If an extender was added, update it with all existing connections
			// in case the peers also support this extender
			m_AllPeers.Map.WithSharedLock([&](const PeerMap& peers)
			{
				for (auto& it : peers)
				{
					it.second->WithUniqueLock([&](Peer& peer)
					{
						peer.ProcessLocalExtenderUpdate(extuuids);
					});
				}
			});
		}

		// Let connected peers know we added or removed an extender
		BroadcastExtenderUpdate();
	}

	void Manager::OnUnhandledExtenderMessage(const ExtenderUUID& extuuid, const PeerLUID pluid,
											 const std::pair<bool, bool>& result) noexcept
	{
		assert(m_Running);

		// If the peer is still connected
		if (auto peerths = Get(pluid); peerths != nullptr)
		{
			peerths->WithUniqueLock([&](Peer& peer)
			{
				peer.OnUnhandledExtenderMessage(extuuid, result);
			});
		}
	}

	void Manager::OnPeerEvent(const Peer& peer, const Event&& event) noexcept
	{
		switch (event.GetType())
		{
			case PeerEventType::Connected:
			{
				// Add new peer to lookup maps
				if (!m_LookupMaps.WithUniqueLock()->AddPeerData(peer.GetPeerData()))
				{
					LogErr(L"Couldn't add peer with UUID %s, LUID %llu to peer lookup maps",
						   event.GetPeerUUID().GetString().c_str(), event.GetPeerLUID());
				}

				break;
			}
			case PeerEventType::Disconnected:
			{
				// Remove peer from lookup maps
				if (!m_LookupMaps.WithUniqueLock()->RemovePeerData(peer.GetPeerData()))
				{
					LogErr(L"Couldn't remove peer with UUID %s, LUID %llu from peer lookup maps",
						   event.GetPeerUUID().GetString().c_str(), event.GetPeerLUID());
				}

				break;
			}
			default:
			{
				break;
			}
		}
	}

	void Manager::AddReportedPublicIPEndpoint(const IPEndpoint& pub_endpoint, const IPEndpoint& rep_peer,
											  const PeerConnectionType rep_con_type, const bool trusted) noexcept
	{
		DiscardReturnValue(m_LocalEnvironment.WithUniqueLock()->AddPublicIPEndpoint(pub_endpoint, rep_peer,
																					rep_con_type, trusted));
	}
}