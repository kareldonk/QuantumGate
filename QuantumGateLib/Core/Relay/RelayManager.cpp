// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "RelayManager.h"
#include "..\Peer\PeerManager.h"
#include "..\Peer\PeerLookupMaps.h"

#include <array>

using namespace std::literals;

namespace QuantumGate::Implementation::Core::Relay
{
	Peer::Manager& Manager::GetPeers() const noexcept
	{
		return m_Peers;
	}

	Access::Manager& Manager::GetAccessManager() const noexcept
	{
		return GetPeers().GetAccessManager();
	}

	const Settings& Manager::GetSettings() const noexcept
	{
		return GetPeers().GetSettings();
	}

	bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"Relaymanager starting...");

		PreStartup();

		if (!StartupThreadPool())
		{
			ShutdownThreadPool();

			LogErr(L"Relaymanager startup failed");

			return false;
		}

		m_Running = true;

		LogSys(L"Relaymanager startup successful");

		return true;
	}

	void Manager::Shutdown() noexcept
	{
		if (!m_Running) return;

		m_Running = false;

		LogSys(L"Relaymanager shutting down...");

		ShutdownThreadPool();

		// Disconnect and remove all relays
		DisconnectAndRemoveAll();

		// If all relays were disconnected and our bookkeeping
		// was done right then the below should be true
		assert(m_RelayLinks.WithUniqueLock()->empty());

		ResetState();

		LogSys(L"Relaymanager shut down");
	}

	void Manager::PreStartup() noexcept
	{
		ResetState();
	}

	void Manager::ResetState() noexcept
	{
		m_ThreadPool.GetData().RelayEventQueues.clear();
		m_ThreadPool.GetData().RelayPortToThreadKeys.WithUniqueLock()->clear();
		m_ThreadPool.GetData().ThreadKeyToLinkTotals.WithUniqueLock()->clear();

		m_RelayLinks.WithUniqueLock()->clear();
	}

	bool Manager::StartupThreadPool() noexcept
	{
		const auto& settings = GetSettings();

		const auto numthreadsperpool = Util::GetNumThreadsPerPool(settings.Local.Concurrency.RelayManager.MinThreads,
																  settings.Local.Concurrency.RelayManager.MaxThreads, 2u);

		// Must have at least two threads in pool 
		// one of which will be the primary thread
		assert(numthreadsperpool > 1);

		LogSys(L"Creating relay threadpool with %zu worker %s",
			   numthreadsperpool, numthreadsperpool > 1 ? L"threads" : L"thread");

		m_ThreadPool.SetWorkerThreadsMaxBurst(settings.Local.Concurrency.WorkerThreadsMaxBurst);
		m_ThreadPool.SetWorkerThreadsMaxSleep(settings.Local.Concurrency.WorkerThreadsMaxSleep);

		auto error = false;

		// Create the worker threads
		for (Size x = 0; x < numthreadsperpool; ++x)
		{
			// First thread is primary worker thread
			if (x == 0)
			{
				if (!m_ThreadPool.AddThread(L"QuantumGate Relay Thread (Main)",
											MakeCallback(this, &Manager::PrimaryThreadProcessor), ThreadData(x)))
				{
					error = true;
				}
			}
			else
			{
				try
				{
					m_ThreadPool.GetData().RelayEventQueues[x] = std::make_unique<EventQueue_ThS>();

					if (m_ThreadPool.AddThread(L"QuantumGate Relay Thread (Event Processor)",
											   MakeCallback(this, &Manager::WorkerThreadProcessor), ThreadData(x),
											   &m_ThreadPool.GetData().RelayEventQueues[x]->WithUniqueLock()->Event()))
					{
						// Add entry for the total number of relay links this thread is handling
						m_ThreadPool.GetData().ThreadKeyToLinkTotals.WithUniqueLock([&](ThreadKeyToLinkTotalMap& link_totals)
						{
							[[maybe_unused]] const auto [it, inserted] = link_totals.insert({ x, 0 });
							if (!inserted)
							{
								error = true;
							}
						});
					}
					else error = true;
				}
				catch (...) { error = true; }
			}

			if (error) break;
		}

		if (!error && m_ThreadPool.Startup())
		{
			return true;
		}

		return false;
	}

	void Manager::ShutdownThreadPool() noexcept
	{
		m_ThreadPool.Shutdown();
		m_ThreadPool.Clear();
	}

	std::optional<RelayPort> Manager::MakeRelayPort() const noexcept
	{
		if (IsRunning())
		{
			if (const auto rport = Crypto::GetCryptoRandomNumber(); rport.has_value())
			{
				return { *rport };
			}
		}

		return std::nullopt;
	}

	bool Manager::Connect(const PeerLUID in_peer, const PeerLUID out_peer,
						  const IPEndpoint& endpoint, const RelayPort rport, const RelayHop hops) noexcept
	{
		assert(IsRunning());

		try
		{
			auto success = false;

			auto rcths = std::make_unique<Link_ThS>(in_peer, out_peer, endpoint,
													rport, hops, Position::Beginning);

			success = rcths->WithUniqueLock()->UpdateStatus(Status::Connect);
			if (success && Add(rport, std::move(rcths)))
			{
				return true;
			}
		}
		catch (...) {}

		return false;
	}

	bool Manager::Accept(const Events::Connect& rcevent, const PeerLUID out_peer) noexcept
	{
		assert(IsRunning());

		try
		{
			auto success = false;

			const auto position = (rcevent.Hop == 0) ? Position::End : Position::Between;

			auto rcths = std::make_unique<Link_ThS>(rcevent.Origin.PeerLUID, out_peer,
													rcevent.Endpoint, rcevent.Port, rcevent.Hop, position);

			success = rcths->WithUniqueLock()->UpdateStatus(Status::Connect);
			if (success && Add(rcevent.Port, std::move(rcths)))
			{
				return true;
			}
		}
		catch (...) {}

		return false;
	}

	std::optional<Manager::ThreadKey> Manager::GetThreadKey(const RelayPort rport) const noexcept
	{
		std::optional<Manager::ThreadKey> thkey;
		m_ThreadPool.GetData().RelayPortToThreadKeys.WithSharedLock([&](const RelayPortToThreadKeyMap& ports)
		{
			if (const auto it = ports.find(rport); it != ports.end())
			{
				thkey = it->second;
			}
		});

		return thkey;
	}

	bool Manager::MapRelayPortToThreadKey(const RelayPort rport) noexcept
	{
		auto success = false;

		const auto thkey = GetThreadKeyWithLeastLinks();
		if (thkey)
		{
			try
			{
				m_ThreadPool.GetData().RelayPortToThreadKeys.WithUniqueLock([&](RelayPortToThreadKeyMap& ports)
				{
					// Add a relationship between RelayPort and ThreadKey so we can
					// lookup which thread handles events for a certain port
					if (const auto ret_pair = ports.insert({ rport, *thkey }); ret_pair.second)
					{
						// Update the total amount of relay links the thread is handling
						m_ThreadPool.GetData().ThreadKeyToLinkTotals.WithUniqueLock(
							[&](ThreadKeyToLinkTotalMap& link_totals)
						{
							if (const auto ltit = link_totals.find(*thkey); ltit != link_totals.end())
							{
								++ltit->second;
								success = true;
							}
							else
							{
								// Shouldn't get here
								assert(false);

								ports.erase(ret_pair.first);
							}
						});
					}
					else
					{
						// Shouldn't get here
						assert(false);
					}
				});
			}
			catch (...) {}
		}

		return success;
	}

	void Manager::UnMapRelayPortFromThreadKey(const RelayPort rport) noexcept
	{
		m_ThreadPool.GetData().RelayPortToThreadKeys.WithUniqueLock([&](RelayPortToThreadKeyMap& ports)
		{
			if (const auto it = ports.find(rport); it != ports.end())
			{
				m_ThreadPool.GetData().ThreadKeyToLinkTotals.WithUniqueLock([&](ThreadKeyToLinkTotalMap& link_totals)
				{
					if (const auto ltit = link_totals.find(it->second); ltit != link_totals.end())
					{
						if (ltit->second > 0) --ltit->second;
						else
						{
							// Shouldn't get here
							assert(false);
						}
					}
					else
					{
						// Shouldn't get here
						assert(false);
					}
				});

				ports.erase(it);
			}
			else
			{
				// Shouldn't get here
				assert(false);
			}
		});
	}

	std::optional<Manager::ThreadKey> Manager::GetThreadKeyWithLeastLinks() const noexcept
	{
		std::optional<ThreadKey> thkey;

		// Get the threadpool with the least amount of relay links
		m_ThreadPool.GetData().ThreadKeyToLinkTotals.WithSharedLock([&](const ThreadKeyToLinkTotalMap& link_totals)
		{
			// Should have at least one item (at least
			// one event worker thread running)
			assert(link_totals.size() > 0);

			const auto it = std::min_element(link_totals.begin(), link_totals.end(),
											 [](const auto& a, const auto& b)
			{
				return (a.second < b.second);
			});

			assert(it != link_totals.end());

			thkey = it->first;
		});

		return thkey;
	}

	bool Manager::AddRelayEvent(const RelayPort rport, Event&& event) noexcept
	{
		if (!IsRunning()) return false;

		auto success = false;

		try
		{
			// Check if the relay port is already mapped to a specific thread
			std::optional<ThreadKey> thkey = GetThreadKey(rport);
			if (!thkey)
			{
				// Get the thread with the least amount of relay links
				thkey = GetThreadKeyWithLeastLinks();
			}

			if (thkey)
			{
				m_ThreadPool.GetData().RelayEventQueues[*thkey]->WithUniqueLock([&](EventQueue& queue)
				{
					queue.Push(std::move(event));
				});

				success = true;
			}
		}
		catch (...) {}

		return success;
	}

	bool Manager::Add(const RelayPort rport, std::unique_ptr<Link_ThS>&& rl) noexcept
	{
		auto success = false;

		try
		{
			m_RelayLinks.WithUniqueLock([&](LinkMap& relays)
			{
				[[maybe_unused]] const auto [it, inserted] = relays.insert({ rport, std::move(rl) });

				if (inserted)
				{
					if (MapRelayPortToThreadKey(rport)) success = true;
					else
					{
						LogErr(L"Failed to map relay port %llu to worker thread!", rport);

						relays.erase(it);
					}
				}
				else LogErr(L"Attempt to add relay port %llu which already exists; this could mean relay loop!", rport);
			});
		}
		catch (...) {}

		return success;
	}

	void Manager::Remove(const Containers::List<RelayPort>& rlist) noexcept
	{
		try
		{
			m_RelayLinks.WithUniqueLock([&](LinkMap& relays)
			{
				for (auto rport : rlist)
				{
					if (relays.erase(rport) == 0)
					{
						LogErr(L"Attempt to remove relay port %llu which doesn't exists!", rport);
					}

					UnMapRelayPortFromThreadKey(rport);
				}
			});
		}
		catch (...) {}
	}

	void Manager::DisconnectAndRemoveAll() noexcept
	{
		try
		{
			Containers::List<RelayPort> remove_list;

			m_RelayLinks.WithUniqueLock([&](LinkMap& relays)
			{
				for (auto& it : relays)
				{
					it.second->WithUniqueLock([&](Link& rl)
					{
						{
							Peer::Peer_ThS::UniqueLockedType in_peer;
							Peer::Peer_ThS::UniqueLockedType out_peer;

							// Get the peers and lock them
							GetUniqueLocks(rl.GetIncomingPeer(), in_peer,
										   rl.GetOutgoingPeer(), out_peer);

							if (rl.GetStatus() != Status::Closed)
							{
								rl.UpdateStatus(Status::Disconnected);

								ProcessRelayDisconnect(rl, in_peer, out_peer);
							}
						}

						// Collect the relay for removal
						remove_list.emplace_back(rl.GetPort());
					});
				}
			});

			// Remove all relays that were collected for removal
			if (!remove_list.empty())
			{
				Remove(remove_list);
				remove_list.clear();
			}
		}
		catch (...) {}
	}

	void Manager::GetUniqueLocks(PeerDetails& ipeer, Peer::Peer_ThS::UniqueLockedType& in_peer,
								 PeerDetails& opeer, Peer::Peer_ThS::UniqueLockedType& out_peer) const noexcept
	{
		// Important to keep a copy of the shared_ptr
		// to the peers while we do work, in case they go
		// away in the mean time and are removed in the Peers
		// class, otherwise we're going to get memory faults

		if (ipeer.Peer == nullptr) ipeer.Peer = GetPeers().Get(ipeer.PeerLUID);

		if (opeer.Peer == nullptr) opeer.Peer = GetPeers().Get(opeer.PeerLUID);

		// Ensure deterministic lock order/direction to prevent possible deadlock
		// situations; smaller PeerLUID always gets locked first
		if (ipeer.PeerLUID < opeer.PeerLUID)
		{
			if (ipeer.Peer != nullptr) in_peer = ipeer.Peer->WithUniqueLock();
			if (opeer.Peer != nullptr) out_peer = opeer.Peer->WithUniqueLock();
		}
		else
		{
			if (opeer.Peer != nullptr) out_peer = opeer.Peer->WithUniqueLock();
			if (ipeer.Peer != nullptr) in_peer = ipeer.Peer->WithUniqueLock();
		}

		// If the peers are disconnected remove them and release the shared_ptr
		{
			if (in_peer && in_peer->GetStatus() == Peer::Status::Disconnected)
			{
				in_peer.Reset();
				ipeer.Peer.reset();
			}

			if (out_peer && out_peer->GetStatus() == Peer::Status::Disconnected)
			{
				out_peer.Reset();
				opeer.Peer.reset();
			}
		}
	}

	void Manager::GetUniqueLock(PeerDetails& rpeer, Peer::Peer_ThS::UniqueLockedType& peer) const noexcept
	{
		if (rpeer.Peer == nullptr) rpeer.Peer = GetPeers().Get(rpeer.PeerLUID);

		if (rpeer.Peer != nullptr) peer = rpeer.Peer->WithUniqueLock();

		// If the peer is disconnected remove it and release the shared_ptr
		{
			if (peer && peer->GetStatus() == Peer::Status::Disconnected)
			{
				peer.Reset();
				rpeer.Peer.reset();
			}
		}
	}

	void Manager::DeterioratePeerReputation(const PeerLUID pluid,
											const Access::IPReputationUpdate rep_update) const noexcept
	{
		if (auto orig_peer = GetPeers().Get(pluid); orig_peer != nullptr)
		{
			orig_peer->WithUniqueLock([&](Peer::Peer& peer) noexcept
			{
				peer.UpdateReputation(rep_update);
			});
		}
	}

	const Link_ThS* Manager::Get(const RelayPort rport) const noexcept
	{
		Link_ThS* rcths{ nullptr };

		m_RelayLinks.WithSharedLock([&](const LinkMap& relays)
		{
			const auto it = relays.find(rport);
			if (it != relays.end())
			{
				rcths = it->second.get();
			}
		});

		return rcths;
	}

	Link_ThS* Manager::Get(const RelayPort rport) noexcept
	{
		return const_cast<Link_ThS*>(const_cast<const Manager*>(this)->Get(rport));
	}

	Manager::ThreadPool::ThreadCallbackResult Manager::PrimaryThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata,
																			  const Concurrency::EventCondition& shutdown_event)
	{
		ThreadPool::ThreadCallbackResult result{ .Success = true };

		Containers::List<RelayPort> remove_list;

		const auto& settings = GetSettings();

		const auto max_connect_duration = settings.Relay.ConnectTimeout;
		const auto closed_grace_period = settings.Relay.GracePeriod;

		m_RelayLinks.WithSharedLock([&](const LinkMap& relays)
		{
			for (auto it = relays.begin(); it != relays.end() && !shutdown_event.IsSet(); ++it)
			{
				it->second->IfUniqueLock([&](Link& rc)
				{
					if (rc.GetStatus() != Status::Closed)
					{
						Peer::Peer_ThS::UniqueLockedType in_peer;
						Peer::Peer_ThS::UniqueLockedType out_peer;

						// Get the peers and lock them
						GetUniqueLocks(rc.GetIncomingPeer(), in_peer, rc.GetOutgoingPeer(), out_peer);

						if (!in_peer)
						{
							LogDbg(L"No incoming peer for relay link on port %llu", rc.GetPort());

							auto exception = Exception::Unknown;

							if (rc.GetPosition() != Position::Beginning)
							{
								if (rc.GetStatus() == Status::Connected)
								{
									// If we were connected and the peer went away
									exception = Exception::ConnectionReset;
								}
							}

							rc.UpdateStatus(Status::Exception, exception);
							result.DidWork = true;
						}
						else if (!out_peer)
						{
							LogDbg(L"No outgoing peer for relay link on port %llu", rc.GetPort());

							auto exception = Exception::Unknown;

							if (rc.GetPosition() != Position::End)
							{
								if (rc.GetStatus() == Status::Connect)
								{
									// Peer went away or connection failed
									exception = Exception::HostUnreachable;
								}
								else if (rc.GetStatus() == Status::Connecting || rc.GetStatus() == Status::Connected)
								{
									// If we were connecting/ed and the peer went away
									exception = Exception::ConnectionReset;
								}
							}

							rc.UpdateStatus(Status::Exception, exception);
							result.DidWork = true;
						}
						else // Both peers are present
						{
							// Check for timeout
							if (rc.GetStatus() < Status::Connected &&
								((Util::GetCurrentSteadyTime() - rc.GetLastStatusChangeSteadyTime()) > max_connect_duration))
							{
								LogErr(L"Relay link on port %llu timed out; will remove", rc.GetPort());

								rc.UpdateStatus(Status::Exception, Exception::TimedOut);
								result.DidWork = true;
							}
							else if (rc.GetStatus() == Status::Connect)
							{
								if ((rc.GetPosition() == Position::Beginning || rc.GetPosition() == Position::Between) &&
									out_peer->GetStatus() != Peer::Status::Ready)
								{
									// Outgoing peer may still be connecting;
									// we'll try again later
								}
								else
								{
									DiscardReturnValue(ProcessRelayConnect(rc, in_peer, out_peer));
									result.DidWork = true;
								}
							}
							else if (rc.GetStatus() == Status::Connected)
							{
								[[maybe_unused]] const auto [success, did_work] = ProcessRelayConnected(rc, in_peer, out_peer);
								if (did_work) result.DidWork = true;
							}
						}

						if (rc.GetStatus() == Status::Disconnected || rc.GetStatus() == Status::Exception)
						{
							ProcessRelayDisconnect(rc, in_peer, out_peer);
							result.DidWork = true;
						}
					}
					else if (rc.GetStatus() == Status::Closed &&
						((Util::GetCurrentSteadyTime() - rc.GetLastStatusChangeSteadyTime()) > closed_grace_period))
					{
						// Collect the relay for removal
						remove_list.emplace_back(rc.GetPort());
					}
				});
			}
		});

		// Remove all relays that were collected for removal
		if (!remove_list.empty())
		{
			LogDbg(L"Removing relays");
			Remove(remove_list);

			remove_list.clear();
			result.DidWork = true;
		}

		return result;
	}

	Manager::ThreadPool::ThreadCallbackResult Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata,
																			 const Concurrency::EventCondition& shutdown_event)
	{
		ThreadPool::ThreadCallbackResult result{ .Success = true };

		std::optional<Event> event;

		m_ThreadPool.GetData().RelayEventQueues[thdata.ThreadKey]->IfUniqueLock([&](auto& queue)
		{
			if (!queue.Empty())
			{
				event = std::move(queue.Front());
				queue.Pop();

				// We had events in the queue
				// so we did work
				result.DidWork = true;
			}
		});

		if (event.has_value())
		{
			std::visit(Util::Overloaded{
				[&](auto& revent)
			{
				ProcessRelayEvent(revent);
			},
					   [&](Events::RelayData& revent)
			{
				while (ProcessRelayEvent(revent) == RelayDataProcessResult::Retry && !shutdown_event.IsSet())
				{
					std::this_thread::sleep_for(1ms);
				}
			}
					   }, *event);
		}

		return result;
	}

	bool Manager::ProcessRelayConnect(Link& rl,
									  Peer::Peer_ThS::UniqueLockedType& in_peer,
									  Peer::Peer_ThS::UniqueLockedType& out_peer)
	{
		assert(rl.GetStatus() == Status::Connect);

		auto success = false;

		switch (rl.GetPosition())
		{
			case Position::Beginning:
			{
				LogDbg(L"Connecting relay to peer %s on port %llu for hop %u (beginning); outgoing peer %s",
					   rl.GetEndpoint().GetString().c_str(), rl.GetPort(), rl.GetHop(), out_peer->GetPeerName().c_str());

				if (out_peer->GetMessageProcessor().SendBeginRelay(rl.GetPort(), rl.GetEndpoint(), rl.GetHop() - 1))
				{
					in_peer->GetSocket<Socket>().SetLocalEndpoint(out_peer->GetLocalEndpoint(), rl.GetPort(), rl.GetHop());
					success = rl.UpdateStatus(Status::Connecting);
				}

				break;
			}
			case Position::End:
			{
				LogDbg(L"Connecting relay to peer %s on port %llu for hop %u (end); incoming peer %s",
					   rl.GetEndpoint().GetString().c_str(), rl.GetPort(), rl.GetHop(), in_peer->GetPeerName().c_str());

				if (rl.SendRelayStatus(*in_peer, std::nullopt, RelayStatusUpdate::Connected))
				{
					if (rl.UpdateStatus(Status::Connected))
					{
						success = out_peer->GetSocket<Socket>().CompleteAccept();
					}
				}

				break;
			}
			case Position::Between:
			{
				LogDbg(L"Connecting relay to peer %s on port %llu for hop %u (between); incoming peer %s, outgoing peer %s",
					   rl.GetEndpoint().GetString().c_str(), rl.GetPort(), rl.GetHop(),
					   in_peer->GetPeerName().c_str(), out_peer->GetPeerName().c_str());

				if (out_peer->GetMessageProcessor().SendBeginRelay(rl.GetPort(), rl.GetEndpoint(), rl.GetHop() - 1))
				{
					success = rl.UpdateStatus(Status::Connecting);
				}

				break;
			}
			default:
			{
				assert(false);
				break;
			}
		}

		if (!success) rl.UpdateStatus(Status::Exception, Exception::GeneralFailure);

		return success;
	}

	std::pair<bool, bool> Manager::ProcessRelayConnected(Link& rl,
														 Peer::Peer_ThS::UniqueLockedType& in_peer,
														 Peer::Peer_ThS::UniqueLockedType& out_peer)
	{
		assert(rl.GetStatus() == Status::Connected);

		bool success = true;
		bool did_work = false;

		PeerLUID orig_luid{ 0 };
		Peer::Peer_ThS::UniqueLockedType* orig_peer{ nullptr };

		switch (rl.GetPosition())
		{
			case Position::Beginning:
				orig_luid = rl.GetIncomingPeer().PeerLUID;
				orig_peer = &in_peer;
				break;
			case Position::End:
				orig_luid = rl.GetOutgoingPeer().PeerLUID;
				orig_peer = &out_peer;
				break;
			default:
				break;
		}

		if (orig_peer != nullptr)
		{
			auto& send_buffer = (*orig_peer)->GetSocket<Socket>().GetSendBuffer();

			const auto send_size = std::invoke([&]()
			{
				// Shouldn't send more than available window size
				auto size = std::min(send_buffer.GetSize(), rl.GetDataRateLimiter().GetAvailableWindowSize());
				// Shouldn't send more than maximum data a relay data message can handle
				size = std::min(size, RelayDataMessage::MaxMessageDataSize);
				return size;
			});

			if (send_size > 0)
			{
				did_work = true;

				const auto msg_id = rl.GetDataRateLimiter().GetNewDataMessageID();

				Events::RelayData red;
				red.Port = rl.GetPort();
				red.MessageID = msg_id;
				red.Data = BufferView(send_buffer).GetFirst(send_size);
				red.Origin.PeerLUID = orig_luid;

				if (AddRelayEvent(rl.GetPort(), std::move(red)))
				{
					send_buffer.RemoveFirst(send_size);

					success = rl.GetDataRateLimiter().AddDataMessage(msg_id, send_size, Util::GetCurrentSteadyTime());
				}
				else success = false;

				if (!success)
				{
					rl.UpdateStatus(Status::Exception, Exception::GeneralFailure);
				}
			}
		}

		return std::make_pair(success, did_work);
	}

	void Manager::ProcessRelayDisconnect(Link& rl,
										 Peer::Peer_ThS::UniqueLockedType& in_peer,
										 Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept
	{
		assert(rl.GetStatus() == Status::Disconnected || rl.GetStatus() == Status::Exception);

		auto status_update{ RelayStatusUpdate::Disconnected };
		auto wsaerror{ -1 };

		switch (rl.GetException())
		{
			case Exception::Unknown:
				break;
			case Exception::GeneralFailure:
				status_update = RelayStatusUpdate::GeneralFailure;
				wsaerror = WSAECONNABORTED;
				break;
			case Exception::ConnectionReset:
				status_update = RelayStatusUpdate::ConnectionReset;
				wsaerror = WSAECONNRESET;
				break;
			case Exception::NoPeersAvailable:
				status_update = RelayStatusUpdate::NoPeersAvailable;
				wsaerror = WSAENETUNREACH;
				break;
			case Exception::HostUnreachable:
				status_update = RelayStatusUpdate::HostUnreachable;
				wsaerror = WSAEHOSTUNREACH;
				break;
			case Exception::ConnectionRefused:
				status_update = RelayStatusUpdate::ConnectionRefused;
				wsaerror = WSAECONNREFUSED;
				break;
			case Exception::TimedOut:
				status_update = RelayStatusUpdate::TimedOut;
				wsaerror = WSAETIMEDOUT;
				break;
			default:
				assert(false);
				break;
		}

		switch (rl.GetPosition())
		{
			case Position::Beginning:
			{
				if (in_peer)
				{
					// In case the connection was closed properly we just enable read
					// on the socket so that it will receive 0 bytes indicating the connection closed
					if (wsaerror != -1) in_peer->GetSocket<Socket>().SetException(wsaerror);
					else in_peer->GetSocket<Socket>().SetRead();
				}

				if (out_peer) DiscardReturnValue(rl.SendRelayStatus(*out_peer, std::nullopt, status_update));

				break;
			}
			case Position::End:
			{
				if (out_peer)
				{
					// In case the connection was closed properly we just enable read
					// on the socket so that it will receive 0 bytes indicating the connection closed
					if (wsaerror != -1) out_peer->GetSocket<Socket>().SetException(wsaerror);
					else out_peer->GetSocket<Socket>().SetRead();
				}

				if (in_peer) DiscardReturnValue(rl.SendRelayStatus(*in_peer, std::nullopt, status_update));

				break;
			}
			case Position::Between:
			{
				if (in_peer) DiscardReturnValue(rl.SendRelayStatus(*in_peer, std::nullopt, status_update));

				if (out_peer) DiscardReturnValue(rl.SendRelayStatus(*out_peer, std::nullopt, status_update));

				break;
			}
			default:
			{
				assert(false);
				break;
			}
		}

		rl.UpdateStatus(Status::Closed);
	}

	bool Manager::ProcessRelayEvent(const Events::Connect& connect_event) noexcept
	{
		// Increase relay connection attempts for this IP; if attempts get too high
		// for a given interval the IP will get a bad reputation and this will fail
		if (!GetAccessManager().AddIPRelayConnectionAttempt(connect_event.Origin.PeerEndpoint.GetIPAddress()))
		{
			LogWarn(L"Relay link from peer %s (LUID %llu) was rejected; maximum number of allowed attempts exceeded",
					connect_event.Origin.PeerEndpoint.GetString().c_str(), connect_event.Origin.PeerLUID);
			return false;
		}

		auto rstatus = RelayStatusUpdate::GeneralFailure;
		String error_details;

		LogInfo(L"Accepting new relay link on endpoint %s for port %llu (hop %u)",
				connect_event.Origin.LocalEndpoint.GetString().c_str(),
				connect_event.Port, connect_event.Hop);

		std::optional<PeerLUID> out_peer;
		auto reused = false;

		if (connect_event.Hop == 0) // Final hop
		{
			auto peerths = m_Peers.CreateRelay(PeerConnectionType::Inbound, std::nullopt);
			if (peerths != nullptr)
			{
				peerths->WithUniqueLock([&](Peer::Peer& peer) noexcept
				{
					if (peer.GetSocket<Socket>().BeginAccept(connect_event.Port, connect_event.Hop,
															 connect_event.Origin.LocalEndpoint,
															 connect_event.Origin.PeerEndpoint))
					{
						if (m_Peers.Add(peerths))
						{
							out_peer = peer.GetLUID();
						}
						else peer.Close();
					}
				});
			}
		}
		else if (connect_event.Hop == 1)
		{
			try
			{
				const auto excl_addr1 = m_Peers.GetLocalIPAddresses();
				if (excl_addr1 != nullptr)
				{
					Vector<BinaryIPAddress> excl_addr2{ connect_event.Origin.PeerEndpoint.GetIPAddress().GetBinary() };

					// Don't include addresses/network of local instance
					const auto result1 = m_Peers.AreRelayIPsInSameNetwork(connect_event.Endpoint.GetIPAddress().GetBinary(),
																		  *excl_addr1);
					// Don't include origin address/network
					const auto result2 = m_Peers.AreRelayIPsInSameNetwork(connect_event.Endpoint.GetIPAddress().GetBinary(),
																		  excl_addr2);

					if (result1.Succeeded() && result2.Succeeded())
					{
						if (!result1.GetValue() && !result2.GetValue())
						{
							// Connect to a specific endpoint for final hop 0
							const auto result2 = m_Peers.ConnectTo({ connect_event.Endpoint }, nullptr);
							if (result2.Succeeded())
							{
								out_peer = result2->first;
								reused = result2->second;
							}
							else
							{
								LogErr(L"Couldn't connect to final endpoint %s for relay port %llu",
									   connect_event.Endpoint.GetString().c_str(), connect_event.Port);

								if (result2 == ResultCode::NotAllowed)
								{
									rstatus = RelayStatusUpdate::ConnectionRefused;
									error_details = L"connection to final endpoint is not allowed by access configuration";
								}
							}
						}
						else
						{
							rstatus = RelayStatusUpdate::ConnectionRefused;
							error_details = L"connection to final endpoint is not allowed because it's on the same network as the origin or local instance";
						}
					}
					else error_details = L"couldn't check if endpoint is on excluded networks";
				}
				else error_details = L"couldn't get IP addresses of local instance";
			}
			catch (...)
			{
				error_details = L"an exception was thrown";
			}
		}
		else // Hop in between
		{
			try
			{
				// Don't include addresses/network of local instance
				const auto excl_addr1 = m_Peers.GetLocalIPAddresses();
				if (excl_addr1 != nullptr)
				{
					Vector<BinaryIPAddress> excl_addr2
					{
						// Don't include origin address/network
						connect_event.Origin.PeerEndpoint.GetIPAddress().GetBinary(),
						// Don't include the final endpoint/network
						connect_event.Endpoint.GetIPAddress().GetBinary()
					};

					const auto result = m_Peers.GetRelayPeer(*excl_addr1, excl_addr2);
					if (result.Succeeded())
					{
						out_peer = result.GetValue();
					}
					else
					{
						if (result == ResultCode::PeerNotFound)
						{
							rstatus = RelayStatusUpdate::NoPeersAvailable;
							error_details = L"no peers available to create relay connection";
						}
						else
						{
							error_details = L"failed to get a peer to create relay connection";
						}
					}
				}
				else error_details = L"couldn't get IP addresses of local instance";
			}
			catch (...)
			{
				error_details = L"an exception was thrown";
			}
		}

		if (out_peer)
		{
			if (!Accept(connect_event, *out_peer))
			{
				// Failed to accept, so cancel connection
				// we made for this relay link
				if (connect_event.Hop == 0 ||
					(connect_event.Hop == 1 && !reused))
				{
					DiscardReturnValue(m_Peers.DisconnectFrom(*out_peer, nullptr));
				}

				out_peer.reset();
			}
		}

		if (!out_peer)
		{
			if (!error_details.empty()) error_details = L" - " + error_details;

			LogErr(L"Failed to accept relay link on endpoint %s for relay port %llu (hop %u)%s",
				   connect_event.Origin.LocalEndpoint.GetString().c_str(),
				   connect_event.Port, connect_event.Hop, error_details.c_str());

			// Couldn't accept; let the incoming peer know
			auto peerths = m_Peers.Get(connect_event.Origin.PeerLUID);
			if (peerths != nullptr)
			{
				peerths->WithUniqueLock()->GetMessageProcessor().SendRelayStatus(connect_event.Port, rstatus);
			}
		}

		return true;
	}

	bool Manager::ProcessRelayEvent(const Events::StatusUpdate& event) noexcept
	{
		auto success = false;

		if (auto relayths = Get(event.Port); relayths != nullptr)
		{
			relayths->WithUniqueLock([&](Link& rl) noexcept
			{
				// Event should come from expected origin
				if (!ValidateEventOrigin(event, rl)) return;

				// If relay is already closed don't bother
				if (rl.GetStatus() == Status::Closed) return;

				{
					Peer::Peer_ThS::UniqueLockedType in_peer;
					Peer::Peer_ThS::UniqueLockedType out_peer;

					// Get the peers and lock them
					GetUniqueLocks(rl.GetIncomingPeer(), in_peer, rl.GetOutgoingPeer(), out_peer);

					if (in_peer && out_peer) // Both peers are present
					{
						const auto prev_status = rl.GetStatus();

						if (rl.UpdateStatus(event.Origin.PeerLUID, event.Status))
						{
							if (rl.GetPosition() == Position::Beginning &&
								rl.GetStatus() == Status::Connected &&
								prev_status != rl.GetStatus())
							{
								// We went to the connected state while we were connecting;
								// the socket is now writable
								in_peer->GetSocket<Socket>().SetWrite();
								success = true;
							}
							else if (rl.GetPosition() == Position::Between)
							{
								Peer::Peer_ThS::UniqueLockedType* peer1 = &in_peer;
								Peer::Peer_ThS::UniqueLockedType* peer2 = &out_peer;
								if (event.Origin.PeerLUID == rl.GetOutgoingPeer().PeerLUID)
								{
									peer1 = &out_peer;
									peer2 = &in_peer;
								}

								// Forward status update to the other peer
								if (rl.SendRelayStatus(*(*peer2), (*peer1)->GetLUID(), event.Status))
								{
									success = true;
								}
							}
							else success = true;
						}
					}
				}

				if (!success) rl.UpdateStatus(Status::Exception, Exception::GeneralFailure);
			});
		}
		else
		{
			// Received event for invalid relay link; this could be an attack
			LogWarn(L"Peer LUID %llu sent relay status update for an unknown port %llu",
					event.Origin.PeerLUID, event.Port);

			DeterioratePeerReputation(event.Origin.PeerLUID);
		}

		return success;
	}

	Manager::RelayDataProcessResult Manager::ProcessRelayEvent(Events::RelayData& event) noexcept
	{
		auto retval = RelayDataProcessResult::Failed;

		if (auto relayths = Get(event.Port); relayths != nullptr)
		{
			relayths->WithUniqueLock([&](Link& rl) noexcept
			{
				// Event should come from expected origin
				if (!ValidateEventOrigin(event, rl)) return;

				// If relay is not (yet) connected (anymore) don't bother
				if (rl.GetStatus() != Status::Connected)
				{
					DbgInvoke([&]()
					{
						LogErr(L"Received relay data event from peer LUID %llu on port %llu that's not connected",
							   event.Origin.PeerLUID, event.Port);
					});

					return;
				}

				bool data_ack_needed{ false };

				auto orig_rpeer = &rl.GetOutgoingPeer();
				auto dest_rpeer = &rl.GetIncomingPeer();
				if (event.Origin.PeerLUID == rl.GetIncomingPeer().PeerLUID)
				{
					orig_rpeer = &rl.GetIncomingPeer();
					dest_rpeer = &rl.GetOutgoingPeer();
				}

				{
					Peer::Peer_ThS::UniqueLockedType dest_peer;

					// Get the peer and lock it
					GetUniqueLock(*dest_rpeer, dest_peer);

					if (dest_peer) // If peer is present
					{
						switch (rl.GetPosition())
						{
							case Position::Beginning:
							{
								if (event.Origin.PeerLUID == rl.GetIncomingPeer().PeerLUID)
								{
									if (const auto result = dest_peer->GetMessageProcessor().SendRelayData(
										RelayDataMessage{ rl.GetPort(), event.MessageID, event.Data }); result.Succeeded())
									{
										retval = RelayDataProcessResult::Succeeded;
									}
									else if (result == ResultCode::PeerSendBufferFull)
									{
										retval = RelayDataProcessResult::Retry;
									}
								}
								else
								{
									data_ack_needed = true;

									if (dest_peer->GetSocket<Socket>().AddToReceiveQueue(std::move(event.Data)))
									{
										retval = RelayDataProcessResult::Succeeded;
									}
								}
								break;
							}
							case Position::End:
							{
								if (event.Origin.PeerLUID == rl.GetIncomingPeer().PeerLUID)
								{
									data_ack_needed = true;

									if (dest_peer->GetSocket<Socket>().AddToReceiveQueue(std::move(event.Data)))
									{
										retval = RelayDataProcessResult::Succeeded;
									}
								}
								else
								{
									if (const auto result = dest_peer->GetMessageProcessor().SendRelayData(
										RelayDataMessage{ rl.GetPort(), event.MessageID, event.Data }); result.Succeeded())
									{
										retval = RelayDataProcessResult::Succeeded;
									}
									else if (result == ResultCode::PeerSendBufferFull)
									{
										retval = RelayDataProcessResult::Retry;
									}
								}
								break;
							}
							case Position::Between:
							{
								if (const auto result = dest_peer->GetMessageProcessor().SendRelayData(
									RelayDataMessage{ rl.GetPort(), event.MessageID, event.Data }); result.Succeeded())
								{
									retval = RelayDataProcessResult::Succeeded;
								}
								else if (result == ResultCode::PeerSendBufferFull)
								{
									retval = RelayDataProcessResult::Retry;
								}
								break;
							}
							default:
							{
								assert(false);
								break;
							}
						}
					}
				}

				if (data_ack_needed && retval == RelayDataProcessResult::Succeeded)
				{
					Peer::Peer_ThS::UniqueLockedType orig_peer;

					// Get the peer and lock it
					GetUniqueLock(*orig_rpeer, orig_peer);

					if (orig_peer) // If peer is present
					{
						// Send RelayDataAck to the origin
						if (!orig_peer->GetMessageProcessor().SendRelayDataAck({ rl.GetPort(), event.MessageID }))
						{
							retval = RelayDataProcessResult::Failed;
						}
					}
				}

				if (retval == RelayDataProcessResult::Failed)
				{
					rl.UpdateStatus(Status::Exception, Exception::GeneralFailure);
				}
			});
		}
		else
		{
			// Received event for invalid relay link; this could be an attack
			LogWarn(L"Peer LUID %llu sent relay data for an unknown port %llu",
					event.Origin.PeerLUID, event.Port);

			DeterioratePeerReputation(event.Origin.PeerLUID);
		}

		return retval;
	}

	bool Manager::ProcessRelayEvent(const Events::RelayDataAck& event) noexcept
	{
		auto success = false;

		if (auto relayths = Get(event.Port); relayths != nullptr)
		{
			relayths->WithUniqueLock([&](Link& rl) noexcept
			{
				// Event should come from expected origin
				if (!ValidateEventOrigin(event, rl)) return;

				// If relay is not (yet) connected (anymore) don't bother
				if (rl.GetStatus() != Status::Connected)
				{
					DbgInvoke([&]()
					{
						LogErr(L"Received relay data ack from peer LUID %llu on port %llu that's not connected",
							   event.Origin.PeerLUID, event.Port);
					});

					return;
				}

				auto dest_rpeer = &rl.GetIncomingPeer();
				if (event.Origin.PeerLUID == rl.GetIncomingPeer().PeerLUID)
				{
					dest_rpeer = &rl.GetOutgoingPeer();
				}

				Peer::Peer_ThS::UniqueLockedType dest_peer;

				switch (rl.GetPosition())
				{
					case Position::Beginning:
					case Position::End:
					{
						if (rl.GetDataRateLimiter().UpdateDataMessage(event.MessageID, Util::GetCurrentSteadyTime()))
						{
							success = true;

							// Get the peer and lock it
							GetUniqueLock(*dest_rpeer, dest_peer);

							if (dest_peer) // If peer is present
							{
								// Update the send buffer size of the relay socket to prevent too much data
								// getting queued up while the connection can't handle it
								dest_peer->GetSocket<Socket>().SetMaxSendBufferSize(rl.GetDataRateLimiter().GetMaxWindowSize());
							}
						}

						break;
					}
					case Position::Between:
					{
						// Get the peer and lock it
						GetUniqueLock(*dest_rpeer, dest_peer);

						if (dest_peer) // If peer is present
						{
							// Forward RelayDataAck to the destination
							success = dest_peer->GetMessageProcessor().SendRelayDataAck({ event.Port, event.MessageID });
						}

						break;
					}
					default:
					{
						assert(false);
						break;
					}
				}

				if (!success) rl.UpdateStatus(Status::Exception, Exception::GeneralFailure);
			});
		}
		else
		{
			// Received event for invalid relay link; this could be an attack
			LogWarn(L"Peer LUID %llu sent relay data ack for an unknown port %llu",
					event.Origin.PeerLUID, event.Port);

			DeterioratePeerReputation(event.Origin.PeerLUID);
		}

		return success;
	}

	template<typename T>
	bool Manager::ValidateEventOrigin(const T& event, const Link& rl) const noexcept
	{
		if (event.Origin.PeerLUID != rl.GetIncomingPeer().PeerLUID &&
			event.Origin.PeerLUID != rl.GetOutgoingPeer().PeerLUID)
		{
			// Received event from a peer not related to this relay
			// link locally; this could be an attack
			LogErr(L"Peer LUID %llu sent relay data for unrelated port %llu",
				   event.Origin.PeerLUID, event.Port);

			DeterioratePeerReputation(event.Origin.PeerLUID, Access::IPReputationUpdate::DeteriorateSevere);
			return false;
		}

		return true;
	}
}