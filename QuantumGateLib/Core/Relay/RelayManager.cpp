// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "RelayManager.h"
#include "..\Peer\PeerManager.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::Relay
{
	Peer::Manager& Manager::GetPeerManager() const noexcept
	{
		return m_PeerManager;
	}

	Access::Manager& Manager::GetAccessManager() const noexcept
	{
		return GetPeerManager().GetAccessManager();
	}

	const Settings& Manager::GetSettings() const noexcept
	{
		return GetPeerManager().GetSettings();
	}

	bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"Relaymanager starting...");

		PreStartup();

		if (!StartupThreadPool())
		{
			BeginShutdownThreadPool();
			EndShutdownThreadPool();

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

		BeginShutdownThreadPool();

		// Disconnect and remove all relays
		DisconnectAndRemoveAll();

		EndShutdownThreadPool();

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

		auto error = !m_ThreadPool.GetData().WorkEvents.Initialize();

		// Create the worker threads
		for (Size x = 0; x < numthreadsperpool && !error; ++x)
		{
			// First thread is primary worker thread
			if (x == 0)
			{
				if (!m_ThreadPool.AddThread(L"QuantumGate Relay Thread (Main)", ThreadData(x, nullptr),
											MakeCallback(this, &Manager::PrimaryThreadProcessor),
											MakeCallback(this, &Manager::PrimaryThreadWait)))
				{
					error = true;
				}
			}
			else
			{
				try
				{
					m_ThreadPool.GetData().RelayEventQueues[x] = std::make_unique<EventQueueMap_ThS>();
					m_ThreadPool.GetData().RelayEventQueues[x]->Insert(DefaultQueueRelayPort);

					if (m_ThreadPool.AddThread(L"QuantumGate Relay Thread (Event Processor)",
											   ThreadData(x, m_ThreadPool.GetData().RelayEventQueues[x].get()),
											   MakeCallback(this, &Manager::WorkerThreadProcessor),
											   MakeCallback(this, &Manager::WorkerThreadWait),
											   MakeCallback(this, &Manager::WorkerThreadWaitInterrupt)))
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
		}

		if (!error && m_ThreadPool.Startup())
		{
			return true;
		}

		return false;
	}

	void Manager::BeginShutdownThreadPool() noexcept
	{
		m_ThreadPool.Shutdown();
		m_ThreadPool.Clear();
	}

	void Manager::EndShutdownThreadPool() noexcept
	{
		m_ThreadPool.GetData().WorkEvents.Deinitialize();
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
						  const Endpoint& endpoint, const RelayPort rport, const RelayHop hops) noexcept
	{
		assert(IsRunning());

		try
		{
			auto success = false;

			auto rlths = std::make_unique<Link_ThS>(in_peer, out_peer, endpoint,
													rport, hops, Position::Beginning);

			{
				auto rl = rlths->WithUniqueLock();

				Peer::Peer_ThS::UniqueLockedType in_peer;
				Peer::Peer_ThS::UniqueLockedType out_peer;

				// Get the peers and lock them
				GetUniqueLocks(rl->GetIncomingPeer(), in_peer, rl->GetOutgoingPeer(), out_peer);

				success = UpdateRelayStatus(*rl, in_peer, out_peer, Status::Connect);
			}

			if (success && Add(rport, std::move(rlths)))
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

			auto rlths = std::make_unique<Link_ThS>(rcevent.Origin.PeerLUID, out_peer,
													rcevent.ConnectEndpoint, rcevent.Port, rcevent.Hop, position);

			{
				auto rl = rlths->WithUniqueLock();

				Peer::Peer_ThS::UniqueLockedType in_peer;
				Peer::Peer_ThS::UniqueLockedType out_peer;

				// Get the peers and lock them
				GetUniqueLocks(rl->GetIncomingPeer(), in_peer, rl->GetOutgoingPeer(), out_peer);

				success = UpdateRelayStatus(*rl, in_peer, out_peer, Status::Connect);
			}
			
			if (success && Add(rcevent.Port, std::move(rlths)))
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
				m_ThreadPool.GetData().RelayPortToThreadKeys.WithUniqueLock([&](RelayPortToThreadKeyMap& ports) noexcept
				{
					// Add a relationship between RelayPort and ThreadKey so we can
					// lookup which thread handles events for a certain port
					if (const auto ret_pair = ports.insert({ rport, *thkey }); ret_pair.second)
					{
						auto sg = MakeScopeGuard([&] { ports.erase(ret_pair.first); });

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
							}
						});

						if (success)
						{
							// Add eventqueue for port
							m_ThreadPool.GetData().RelayEventQueues[*thkey]->Insert(rport);

							sg.Deactivate();
						}
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
				m_ThreadPool.GetData().ThreadKeyToLinkTotals.WithUniqueLock([&](ThreadKeyToLinkTotalMap& link_totals) noexcept
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

				// Remove eventqueue for port
				m_ThreadPool.GetData().RelayEventQueues[it->second]->Erase(rport);

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

	bool Manager::AddRelayEvent(RelayPort rport, Event&& event) noexcept
	{
		if (!IsRunning()) return false;

		auto success = false;

		try
		{
			// TODO: Need to check if we're receiving a data or datack event
			// for existing relay link that is suspended, and if so, send 
			// a suspended status update to the originating peer.

			auto use_default_queue = false;
			std::optional<ThreadKey> thkey;

			if (std::holds_alternative<Events::StatusUpdate>(event))
			{
				const auto status = std::get<Events::StatusUpdate>(event).Status;

				// Suspend and resume updates are handled out of order in the default queue
				// to prevent data events from blocking them in front during suspended state
				use_default_queue = (status == RelayStatusUpdate::Suspended || status == RelayStatusUpdate::Resumed);
			}
			else if (std::holds_alternative<Events::Connect>(event))
			{
				use_default_queue = true;
			}

			if (!use_default_queue)
			{
				// Check if the relay port is already mapped to a specific thread
				thkey = GetThreadKey(rport);
			}
			
			if (!thkey)
			{
				// Get the thread with the least amount of relay links
				thkey = GetThreadKeyWithLeastLinks();
				rport = DefaultQueueRelayPort;
			}

			m_ThreadPool.GetData().RelayEventQueues[*thkey]->PushBack(rport, std::move(event));
			success = true;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Couldn't add event on relay port %llu due to exception - %s",
				   rport, Util::ToStringW(e.what()).c_str());
		}

		return success;
	}

	bool Manager::Add(const RelayPort rport, std::unique_ptr<Link_ThS>&& rl) noexcept
	{
		auto success = false;

		try
		{
			m_RelayLinks.WithUniqueLock([&](LinkMap& relays) noexcept
			{
				const auto result = relays.insert({ rport, std::move(rl) });
				if (result.second)
				{
					auto sg = MakeScopeGuard([&] { relays.erase(result.first); });

					if (MapRelayPortToThreadKey(rport))
					{
						success = true;

						sg.Deactivate();
					}
					else LogErr(L"Failed to map relay port %llu to worker thread!", rport);
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
			m_RelayLinks.WithUniqueLock([&](LinkMap& relays) noexcept
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
			std::optional<Containers::List<RelayPort>> remove_list;

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
							GetUniqueLocks(rl.GetIncomingPeer(), in_peer, rl.GetOutgoingPeer(), out_peer);

							if (rl.GetStatus() != Status::Closed)
							{
								UpdateRelayStatus(rl, in_peer, out_peer, Status::Disconnected);

								ProcessRelayDisconnect(rl, in_peer, out_peer);
							}
						}

						// Collect the relay for removal
						if (!remove_list.has_value()) remove_list.emplace();

						remove_list->emplace_back(rl.GetPort());
					});
				}
			});

			// Remove all relays that were collected for removal
			if (remove_list.has_value() && !remove_list->empty())
			{
				Remove(*remove_list);
				remove_list->clear();
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

		if (ipeer.Peer == nullptr) ipeer.Peer = GetPeerManager().Get(ipeer.PeerLUID);
		if (opeer.Peer == nullptr) opeer.Peer = GetPeerManager().Get(opeer.PeerLUID);

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

		// If the peers are disconnected remove them
		{
			if (in_peer && in_peer->GetStatus() == Peer::Status::Disconnected)
			{
				in_peer.Reset();
			}

			if (out_peer && out_peer->GetStatus() == Peer::Status::Disconnected)
			{
				out_peer.Reset();
			}
		}
	}

	void Manager::GetUniqueLock(PeerDetails& rpeer, Peer::Peer_ThS::UniqueLockedType& peer) const noexcept
	{
		if (rpeer.Peer == nullptr) rpeer.Peer = GetPeerManager().Get(rpeer.PeerLUID);

		if (rpeer.Peer != nullptr) peer = rpeer.Peer->WithUniqueLock();

		// If the peer is disconnected remove it
		{
			if (peer && peer->GetStatus() == Peer::Status::Disconnected)
			{
				peer.Reset();
			}
		}
	}

	void Manager::DeterioratePeerReputation(const PeerLUID pluid,
											const Access::AddressReputationUpdate rep_update) const noexcept
	{
		if (auto orig_peer = GetPeerManager().Get(pluid); orig_peer != nullptr)
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

	void Manager::PrimaryThreadWait(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		const auto result = thpdata.WorkEvents.Wait(1ms);
		if (!result.Waited)
		{
			shutdown_event.Wait(1ms);
		}
	}

	void Manager::PrimaryThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		std::optional<Containers::List<RelayPort>> remove_list;

		m_RelayLinks.WithSharedLock([&](const LinkMap& relays)
		{
			if (relays.empty()) return;

			const auto& settings = GetSettings();
			const auto max_connect_duration = settings.Relay.ConnectTimeout;
			const auto closed_grace_period = settings.Relay.GracePeriod;
			const auto max_suspend_duration = settings.Relay.MaxSuspendDuration;

			for (auto it = relays.begin(); it != relays.end() && !shutdown_event.IsSet(); ++it)
			{
				it->second->IfUniqueLock([&](Link& rl)
				{
					const auto current_steadytime = Util::GetCurrentSteadyTime();

					if (rl.GetStatus() != Status::Closed)
					{
						Peer::Peer_ThS::UniqueLockedType in_peer;
						Peer::Peer_ThS::UniqueLockedType out_peer;

						// Get the peers and lock them
						GetUniqueLocks(rl.GetIncomingPeer(), in_peer, rl.GetOutgoingPeer(), out_peer);

						if (!in_peer)
						{
							LogDbg(L"No incoming peer for relay link on port %llu", rl.GetPort());

							auto exception = Exception::Unknown;

							if (rl.GetPosition() != Position::Beginning)
							{
								if (rl.GetStatus() == Status::Connected || rl.GetStatus() == Status::Suspended)
								{
									// If we were connected and the peer went away
									exception = Exception::ConnectionReset;
								}
							}

							UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, exception);
						}
						else if (!out_peer)
						{
							LogDbg(L"No outgoing peer for relay link on port %llu", rl.GetPort());

							auto exception = Exception::Unknown;

							if (rl.GetPosition() != Position::End)
							{
								if (rl.GetStatus() == Status::Connect)
								{
									// Peer went away or connection failed
									exception = Exception::HostUnreachable;
								}
								else if (rl.GetStatus() == Status::Connecting || rl.GetStatus() == Status::Connected ||
										 rl.GetStatus() == Status::Suspended)
								{
									// If we were connecting/ed and the peer went away
									exception = Exception::ConnectionReset;
								}
							}

							UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, exception);
						}
						else // Both peers are present
						{
							// Check for timeout
							if (rl.GetStatus() < Status::Connected &&
								((current_steadytime - rl.GetLastStatusChangeSteadyTime()) > max_connect_duration))
							{
								LogErr(L"Relay link on port %llu timed out; will remove", rl.GetPort());

								UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, Exception::TimedOut);
							}
							else if (rl.GetStatus() == Status::Connect)
							{
								if ((rl.GetPosition() == Position::Beginning || rl.GetPosition() == Position::Between) &&
									out_peer->GetStatus() != Peer::Status::Ready)
								{
									// Outgoing peer may still be connecting;
									// we'll try again later
								}
								else
								{
									DiscardReturnValue(ProcessRelayConnect(rl, in_peer, out_peer));
								}
							}
							else if (rl.GetStatus() == Status::Connected)
							{
								DiscardReturnValue(ProcessRelayConnected(rl, in_peer, out_peer));
							}
							else if (rl.GetStatus() == Status::Suspended)
							{
								const auto suspend_duration =
									std::chrono::duration_cast<std::chrono::seconds>(current_steadytime - rl.GetLastStatusChangeSteadyTime());
								if (suspend_duration > max_suspend_duration)
								{
									LogErr(L"Relay link on port %llu has been suspended for too long (%jds; maximum is %jds); will remove",
										   rl.GetPort(), suspend_duration.count(), max_suspend_duration.count());

									UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, Exception::TimedOut);
								}
								else
								{
									DiscardReturnValue(ProcessRelaySuspended(rl, in_peer, out_peer));
								}
							}
						}

						if (rl.GetStatus() == Status::Disconnected || rl.GetStatus() == Status::Exception)
						{
							ProcessRelayDisconnect(rl, in_peer, out_peer);
						}
					}
					else if (rl.GetStatus() == Status::Closed &&
							 ((current_steadytime - rl.GetLastStatusChangeSteadyTime()) > closed_grace_period))
					{
						// Collect the relay for removal
						if (!remove_list.has_value()) remove_list.emplace();

						remove_list->emplace_back(rl.GetPort());
					}
				});
			}
		});

		// Remove all relays that were collected for removal
		if (remove_list.has_value() && !remove_list->empty())
		{
			LogDbg(L"Removing relays");
			Remove(*remove_list);

			remove_list->clear();
		}
	}

	void Manager::WorkerThreadWait(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		thdata.EventQueueMap->Wait(shutdown_event);
	}

	void Manager::WorkerThreadWaitInterrupt(ThreadPoolData& thpdata, ThreadData& thdata)
	{
		thdata.EventQueueMap->InterruptWait();
	}

	void Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		ProcessEvents(*thdata.EventQueueMap, shutdown_event);
	}

	void Manager::ProcessEvents(EventQueueMap_ThS& queue_map, const Concurrency::Event& shutdown_event)
	{
		std::optional<Event> event;

		queue_map.PopFrontIf([&](auto& fevent) noexcept -> bool
		{
			event = std::move(fevent);
			return true;
		});

		if (event.has_value())
		{
			std::visit(Util::Overloaded{
				[&](auto& revent) noexcept
				{
					ProcessRelayEvent(revent);
				},
				[&](Events::RelayData& revent) noexcept
				{
					if (ProcessRelayEvent(revent) == RelayEventProcessResult::Retry)
					{
						const auto rport = revent.Port;

						try
						{
							queue_map.PushFront(rport, std::move(revent));

							if (queue_map.GetKeyCount() == 1)
							{
								// Prevent from spinning if there's only one queue
								std::this_thread::sleep_for(1ms);
							}
						}
						catch (const std::exception& e)
						{
							LogErr(L"Couldn't add event on relay port %llu back to queue due to exception - %s",
								   rport, Util::ToStringW(e.what()).c_str());
						}
					}
				}
			}, *event);
		}
	}

	bool Manager::UpdateRelayStatus(Link& rl,
									Peer::Peer_ThS::UniqueLockedType& in_peer,
									Peer::Peer_ThS::UniqueLockedType& out_peer,
									const Status status, const Exception exception) noexcept
	{
		const auto prev_status = rl.GetStatus();

		if (rl.UpdateStatus(status, exception))
		{
			return OnRelayStatusUpdate(rl, in_peer, out_peer, prev_status);
		}

		return false;
	}

	bool Manager::UpdateRelayStatus(Link& rl,
									Peer::Peer_ThS::UniqueLockedType& in_peer,
									Peer::Peer_ThS::UniqueLockedType& out_peer,
									const PeerLUID from_pluid, const RelayStatusUpdate status) noexcept
	{
		const auto prev_status = rl.GetStatus();

		if ((status == RelayStatusUpdate::Suspended && prev_status == Status::Suspended) ||
			(status == RelayStatusUpdate::Resumed && prev_status == Status::Connected))
		{
			return true;
		}

		if (rl.UpdateStatus(from_pluid, status))
		{
			return OnRelayStatusUpdate(rl, in_peer, out_peer, prev_status);
		}

		return false;
	}

	bool Manager::OnRelayStatusUpdate(const Link& rl,
									  Peer::Peer_ThS::UniqueLockedType& in_peer,
									  Peer::Peer_ThS::UniqueLockedType& out_peer,
									  const Status prev_status) noexcept
	{
		if (rl.GetStatus() == Status::Connected && (prev_status == Status::Connect || prev_status == Status::Connecting))
		{
			// We went to the connected state while we were connecting;
			// the socket is now writable
			if (rl.GetPosition() == Position::Beginning) in_peer->GetSocket<Socket>().SetSocketWrite();
			else if (rl.GetPosition() == Position::End) out_peer->GetSocket<Socket>().SetSocketWrite();
		}
		else if (rl.GetStatus() == Status::Suspended && prev_status == Status::Connected)
		{
			// We went into suspended state while we were connected
			if (rl.GetPosition() == Position::Beginning) in_peer->GetSocket<Socket>().SetSocketSuspended(true);
			else if (rl.GetPosition() == Position::End) out_peer->GetSocket<Socket>().SetSocketSuspended(true);
		}
		else if (rl.GetStatus() == Status::Connected && prev_status == Status::Suspended)
		{
			// We went into connected state while we were suspended
			if (rl.GetPosition() == Position::Beginning) in_peer->GetSocket<Socket>().SetSocketSuspended(false);
			else if (rl.GetPosition() == Position::End) out_peer->GetSocket<Socket>().SetSocketSuspended(false);
		}

		return true;
	}

	bool Manager::ProcessRelayConnect(Link& rl,
									  Peer::Peer_ThS::UniqueLockedType& in_peer,
									  Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept
	{
		assert(rl.GetStatus() == Status::Connect);

		auto success = false;
		Peer::Peer_ThS::UniqueLockedType* peer{ nullptr };

		switch (rl.GetPosition())
		{
			case Position::Beginning:
			{
				peer = &in_peer;

				LogDbg(L"Connecting relay to peer %s on port %llu for hop %u (beginning); outgoing peer %s",
					   rl.GetEndpoint().GetString().c_str(), rl.GetPort(), rl.GetHop(), out_peer->GetPeerName().c_str());

				if (out_peer->GetMessageProcessor().SendBeginRelay(rl.GetPort(), rl.GetEndpoint(), rl.GetHop() - 1))
				{
					in_peer->GetSocket<Socket>().SetLocalEndpoint(out_peer->GetLocalEndpoint(), rl.GetPort(), rl.GetHop());
					success = UpdateRelayStatus(rl, in_peer, out_peer, Status::Connecting);
				}

				break;
			}
			case Position::End:
			{
				peer = &out_peer;

				LogDbg(L"Connecting relay to peer %s on port %llu for hop %u (end); incoming peer %s",
					   rl.GetEndpoint().GetString().c_str(), rl.GetPort(), rl.GetHop(), in_peer->GetPeerName().c_str());

				if (rl.SendRelayStatus(*in_peer, std::nullopt, RelayStatusUpdate::Connected))
				{
					if (UpdateRelayStatus(rl, in_peer, out_peer, Status::Connected))
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
					success = UpdateRelayStatus(rl, in_peer, out_peer, Status::Connecting);
				}

				break;
			}
			default:
			{
				// Shouldn't get here
				assert(false);
				break;
			}
		}

		if (peer != nullptr)
		{
			success = m_ThreadPool.GetData().WorkEvents.AddEvent((*peer)->GetSocket<Socket>().GetSendEvent().GetHandle());
		}

		if (!success) UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, Exception::GeneralFailure);

		return success;
	}

	bool Manager::ProcessRelayConnected(Link& rl,
										Peer::Peer_ThS::UniqueLockedType& in_peer,
										Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept
	{
		assert(rl.GetStatus() == Status::Connected);

		const auto process_suspend = [&]() noexcept
		{
			switch (rl.GetPosition())
			{
				case Position::Beginning:
				{
					if (out_peer->GetStatus() == Peer::Status::Suspended)
					{
						if (UpdateRelayStatus(rl, in_peer, out_peer, Status::Suspended))
						{
							rl.GetOutgoingPeer().IsSuspended = true;
							return true;
						}
					}
					break;
				}
				case Position::End:
				{
					if (in_peer->GetStatus() == Peer::Status::Suspended)
					{
						if (UpdateRelayStatus(rl, in_peer, out_peer, Status::Suspended))
						{
							rl.GetIncomingPeer().IsSuspended = true;
							return true;
						}
					}
					break;
				}
				case Position::Between:
				{
					if (in_peer->GetStatus() == Peer::Status::Suspended || out_peer->GetStatus() == Peer::Status::Suspended)
					{
						auto success = UpdateRelayStatus(rl, in_peer, out_peer, Status::Suspended);
						if (success)
						{
							rl.GetIncomingPeer().IsSuspended = (in_peer->GetStatus() == Peer::Status::Suspended);
							rl.GetOutgoingPeer().IsSuspended = (out_peer->GetStatus() == Peer::Status::Suspended);

							if (success && in_peer->GetStatus() != Peer::Status::Suspended)
							{
								success = rl.SendRelayStatus(*in_peer, rl.GetOutgoingPeer().PeerLUID, RelayStatusUpdate::Suspended);
								rl.GetIncomingPeer().NeedsResumeUpdate = success;

								LogDbg(L"Sent suspend status update to peer %llu for relay link on port %llu",
									   rl.GetIncomingPeer().PeerLUID, rl.GetPort());
							}
							if (success && out_peer->GetStatus() != Peer::Status::Suspended)
							{
								success = rl.SendRelayStatus(*out_peer, rl.GetIncomingPeer().PeerLUID, RelayStatusUpdate::Suspended);
								rl.GetOutgoingPeer().NeedsResumeUpdate = success;

								LogDbg(L"Sent suspend status update to peer %llu for relay link on port %llu",
									   rl.GetOutgoingPeer().PeerLUID, rl.GetPort());
							}
						}

						return success;
					}
					break;
				}
				default:
				{
					// Shouldn't get here
					assert(false);
					break;
				}
			}

			return false;
		};

		const auto process_send = [&](const PeerLUID orig_luid, Peer::Peer_ThS::UniqueLockedType& orig_peer) noexcept
		{
			auto success = true;

			auto send_buffer = orig_peer->GetSocket<Socket>().GetSendBuffer();
			auto& rdrl = rl.GetDataRateLimiter();

			while (success && rdrl.CanAddMTU())
			{
				const auto send_size = std::invoke([&]()
				{
					// Shouldn't send more than available MTU size
					auto size = std::min(send_buffer->GetSize(), rdrl.GetMTUSize());
					// Shouldn't send more than maximum data a relay data message can handle
					size = std::min(size, RelayDataMessage::MaxMessageDataSize);
					return size;
				});

				if (send_size > 0)
				{
					try
					{
						const auto msg_id = rdrl.GetNewMessageID();

						Events::RelayData red;
						red.Port = rl.GetPort();
						red.MessageID = msg_id;
						red.Data = BufferView(*send_buffer).GetFirst(send_size);
						red.Origin.PeerLUID = orig_luid;

						if (AddRelayEvent(rl.GetPort(), std::move(red)))
						{
							send_buffer->RemoveFirst(send_size);

							success = rdrl.AddMTU(msg_id, send_size, Util::GetCurrentSteadyTime());
						}
						else success = false;
					}
					catch (const std::exception& e)
					{
						LogErr(L"Couldn't send data from peer %s (LUID %llu) on relay port %llu due to exception - %s",
							   orig_peer->GetPeerName().c_str(), orig_luid, rl.GetPort(), Util::ToStringW(e.what()).c_str());
						success = false;
					}
				}
				else break;
			}

			// Update socket send event
			orig_peer->GetSocket<Socket>().SetRelayWrite(rdrl.CanAddMTU());

			return success;
		};

		auto success = false;

		if (in_peer->GetStatus() == Peer::Status::Suspended || out_peer->GetStatus() == Peer::Status::Suspended)
		{
			success = process_suspend();
		}
		else
		{
			switch (rl.GetPosition())
			{
				case Position::Beginning:
					success = process_send(rl.GetIncomingPeer().PeerLUID, in_peer);
					break;
				case Position::End:
					success = process_send(rl.GetOutgoingPeer().PeerLUID, out_peer);
					break;
				case Position::Between:
					success = true;
					break;
				default:
					// Shouldn't get here
					assert(false);
					break;
			}
		}

		if (!success)
		{
			UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, Exception::GeneralFailure);
		}

		return success;
	}

	bool Manager::ProcessRelaySuspended(Link& rl,
										Peer::Peer_ThS::UniqueLockedType& in_peer,
										Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept
	{
		auto success = true;

		switch (rl.GetPosition())
		{
			case Position::Beginning:
			{
				if (rl.GetOutgoingPeer().IsSuspended && out_peer->GetStatus() == Peer::Status::Ready)
				{
					success = UpdateRelayStatus(rl, in_peer, out_peer, Status::Connected);
					if (success) rl.GetOutgoingPeer().IsSuspended = false;
				}
				break;
			}
			case Position::End:
			{
				if (rl.GetIncomingPeer().IsSuspended && in_peer->GetStatus() == Peer::Status::Ready)
				{
					success = UpdateRelayStatus(rl, in_peer, out_peer, Status::Connected);
					if (success) rl.GetIncomingPeer().IsSuspended = false;
				}
				break;
			}
			case Position::Between:
			{
				if ((in_peer->GetStatus() == Peer::Status::Ready && out_peer->GetStatus() == Peer::Status::Ready) &&
					(rl.GetIncomingPeer().IsSuspended || rl.GetOutgoingPeer().IsSuspended))
				{
					success = UpdateRelayStatus(rl, in_peer, out_peer, Status::Connected);
					if (success)
					{
						rl.GetIncomingPeer().IsSuspended = false;
						rl.GetOutgoingPeer().IsSuspended = false;
					}

					if (success && rl.GetIncomingPeer().NeedsResumeUpdate)
					{
						success = rl.SendRelayStatus(*in_peer, rl.GetOutgoingPeer().PeerLUID, RelayStatusUpdate::Resumed);
						rl.GetIncomingPeer().NeedsResumeUpdate = false;

						LogDbg(L"Sent resume status update to peer %llu for relay link on port %llu",
							   rl.GetIncomingPeer().PeerLUID, rl.GetPort());
					}

					if (success && rl.GetOutgoingPeer().NeedsResumeUpdate)
					{
						success = rl.SendRelayStatus(*out_peer, rl.GetIncomingPeer().PeerLUID, RelayStatusUpdate::Resumed);
						rl.GetOutgoingPeer().NeedsResumeUpdate = false;

						LogDbg(L"Sent resume status update to peer %llu for relay link on port %llu",
							   rl.GetOutgoingPeer().PeerLUID, rl.GetPort());
					}
				}
				break;
			}
			default:
			{
				// Shouldn't get here
				assert(false);
				break;
			}
		}

		if (!success)
		{
			UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, Exception::GeneralFailure);
		}

		return success;
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

		Peer::Peer_ThS::UniqueLockedType temp_peer;
		Peer::Peer_ThS::UniqueLockedType* peer{ nullptr };

		switch (rl.GetPosition())
		{
			case Position::Beginning:
			{
				if (in_peer)
				{
					peer = &in_peer;

					// In case the connection was closed properly we just enable read
					// on the socket so that it will receive 0 bytes indicating the connection closed
					if (wsaerror != -1) in_peer->GetSocket<Socket>().SetException(wsaerror);
					else in_peer->GetSocket<Socket>().SetSocketRead();
				}
				else
				{
					temp_peer = rl.GetIncomingPeer().Peer->WithUniqueLock();
					peer = &temp_peer;
				}

				if (out_peer) DiscardReturnValue(rl.SendRelayStatus(*out_peer, std::nullopt, status_update));

				break;
			}
			case Position::End:
			{
				if (out_peer)
				{
					peer = &out_peer;

					// In case the connection was closed properly we just enable read
					// on the socket so that it will receive 0 bytes indicating the connection closed
					if (wsaerror != -1) out_peer->GetSocket<Socket>().SetException(wsaerror);
					else out_peer->GetSocket<Socket>().SetSocketRead();
				}
				else
				{
					temp_peer = rl.GetOutgoingPeer().Peer->WithUniqueLock();
					peer = &temp_peer;
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

		if (peer != nullptr)
		{
			// Event may not have been added if the link never got to the Connecting state
			const auto handle = (*peer)->GetSocket<Socket>().GetSendEvent().GetHandle();
			if (m_ThreadPool.GetData().WorkEvents.HasEvent(handle))
			{
				m_ThreadPool.GetData().WorkEvents.RemoveEvent(handle);
			}
		}

		UpdateRelayStatus(rl, in_peer, out_peer, Status::Closed);
	}

	Manager::RelayEventProcessResult Manager::ProcessRelayEvent(const Events::Connect& connect_event) noexcept
	{
		// Increase relay connection attempts for this address; if attempts get too high
		// for a given interval the address will get a bad reputation and this will fail
		if (!GetAccessManager().AddRelayConnectionAttempt(connect_event.Origin.PeerEndpoint))
		{
			LogWarn(L"Relay link from peer %s (LUID %llu) was rejected; maximum number of allowed attempts exceeded",
					connect_event.Origin.PeerEndpoint.GetString().c_str(), connect_event.Origin.PeerLUID);
			return RelayEventProcessResult::Failed;
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
			auto peerths = m_PeerManager.CreateRelay(PeerConnectionType::Inbound, std::nullopt);
			if (peerths != nullptr)
			{
				peerths->WithUniqueLock([&](Peer::Peer& peer) noexcept
				{
					if (peer.GetSocket<Socket>().BeginAccept(connect_event.Port, connect_event.Hop,
															 connect_event.Origin.LocalEndpoint,
															 connect_event.Origin.PeerEndpoint))
					{
						if (m_PeerManager.Add(peerths))
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
				const auto excl_addr1 = m_PeerManager.GetLocalAddresses();
				if (excl_addr1 != nullptr)
				{
					Vector<Network::Address> excl_addr2{ connect_event.Origin.PeerEndpoint };

					// Don't include addresses/network of local instance
					const auto result1 = m_PeerManager.AreRelayAddressesInSameNetwork(connect_event.ConnectEndpoint, *excl_addr1);
					// Don't include origin address/network
					const auto result2 = m_PeerManager.AreRelayAddressesInSameNetwork(connect_event.ConnectEndpoint, excl_addr2);

					if (result1.Succeeded() && result2.Succeeded())
					{
						if (!result1.GetValue() && !result2.GetValue())
						{
							// Connect to a specific endpoint for final hop 0
							const auto result2 = m_PeerManager.ConnectTo({ connect_event.ConnectEndpoint }, nullptr);
							if (result2.Succeeded())
							{
								out_peer = result2->first;
								reused = result2->second;
							}
							else
							{
								LogErr(L"Couldn't connect to final endpoint %s for relay port %llu",
									   connect_event.ConnectEndpoint.GetString().c_str(), connect_event.Port);

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
				const auto excl_addr1 = m_PeerManager.GetLocalAddresses();
				if (excl_addr1 != nullptr)
				{
					Vector<Network::Address> excl_addr2
					{
						// Don't include origin address/network
						connect_event.Origin.PeerEndpoint,
						// Don't include the final endpoint/network
						connect_event.ConnectEndpoint
					};

					const auto result = m_PeerManager.GetRelayPeer(*excl_addr1, excl_addr2);
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
					DiscardReturnValue(m_PeerManager.DisconnectFrom(*out_peer, nullptr));
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
			auto peerths = m_PeerManager.Get(connect_event.Origin.PeerLUID);
			if (peerths != nullptr)
			{
				peerths->WithUniqueLock()->GetMessageProcessor().SendRelayStatus(connect_event.Port, rstatus);
			}
		}

		return RelayEventProcessResult::Succeeded;
	}

	Manager::RelayEventProcessResult Manager::ProcessRelayEvent(const Events::StatusUpdate& event) noexcept
	{
		auto retval = RelayEventProcessResult::Failed;

		if (auto relayths = Get(event.Port); relayths != nullptr)
		{
			relayths->WithUniqueLock([&](Link& rl) noexcept
			{
				// Event should come from expected origin
				if (!ValidateEventOrigin(event, rl)) return;

				// If relay is already closed don't bother
				if (rl.GetStatus() == Status::Closed) return;

				Peer::Peer_ThS::UniqueLockedType in_peer;
				Peer::Peer_ThS::UniqueLockedType out_peer;

				// Get the peers and lock them
				GetUniqueLocks(rl.GetIncomingPeer(), in_peer, rl.GetOutgoingPeer(), out_peer);

				if (in_peer && out_peer) // Both peers are present
				{
					if (UpdateRelayStatus(rl, in_peer, out_peer, event.Origin.PeerLUID, event.Status))
					{
						if (rl.GetPosition() == Position::Between)
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
								retval = RelayEventProcessResult::Succeeded;
							}
						}
						else retval = RelayEventProcessResult::Succeeded;
					}
				}

				if (retval == RelayEventProcessResult::Failed)
				{
					UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, Exception::GeneralFailure);
				}
			});
		}
		else
		{
			// Received event for invalid relay link; this could be an attack
			LogWarn(L"Peer LUID %llu sent relay status update for an unknown port %llu",
					event.Origin.PeerLUID, event.Port);

			DeterioratePeerReputation(event.Origin.PeerLUID);
		}

		return retval;
	}

	Manager::RelayEventProcessResult Manager::ProcessRelayEvent(Events::RelayData& event) noexcept
	{
		auto retval = RelayEventProcessResult::Failed;

		if (auto relayths = Get(event.Port); relayths != nullptr)
		{
			relayths->WithUniqueLock([&](Link& rl) noexcept
			{
				// Event should come from expected origin
				if (!ValidateEventOrigin(event, rl)) return;

				// If relay is not (yet) connected (anymore) don't bother
				if (rl.GetStatus() != Status::Connected && rl.GetStatus() != Status::Suspended)
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
										retval = RelayEventProcessResult::Succeeded;
									}
									else if (result == ResultCode::PeerSendBufferFull)
									{
										retval = RelayEventProcessResult::Retry;
									}
								}
								else
								{
									try
									{
										data_ack_needed = true;

										auto rcv_buffer = dest_peer->GetSocket<Socket>().GetReceiveBuffer();
										*rcv_buffer += event.Data;

										retval = RelayEventProcessResult::Succeeded;
									}
									catch (...) {}
								}
								break;
							}
							case Position::End:
							{
								if (event.Origin.PeerLUID == rl.GetIncomingPeer().PeerLUID)
								{
									try
									{
										data_ack_needed = true;

										auto rcv_buffer = dest_peer->GetSocket<Socket>().GetReceiveBuffer();
										*rcv_buffer += event.Data;

										retval = RelayEventProcessResult::Succeeded;
									}
									catch (...) {}
								}
								else
								{
									if (const auto result = dest_peer->GetMessageProcessor().SendRelayData(
										RelayDataMessage{ rl.GetPort(), event.MessageID, event.Data }); result.Succeeded())
									{
										retval = RelayEventProcessResult::Succeeded;
									}
									else if (result == ResultCode::PeerSendBufferFull)
									{
										retval = RelayEventProcessResult::Retry;
									}
								}
								break;
							}
							case Position::Between:
							{
								if (const auto result = dest_peer->GetMessageProcessor().SendRelayData(
									RelayDataMessage{ rl.GetPort(), event.MessageID, event.Data }); result.Succeeded())
								{
									retval = RelayEventProcessResult::Succeeded;
								}
								else if (result == ResultCode::PeerSendBufferFull)
								{
									retval = RelayEventProcessResult::Retry;
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

				if (data_ack_needed && retval == RelayEventProcessResult::Succeeded)
				{
					Peer::Peer_ThS::UniqueLockedType orig_peer;

					// Get the peer and lock it
					GetUniqueLock(*orig_rpeer, orig_peer);

					if (orig_peer) // If peer is present
					{
						// Send RelayDataAck to the origin
						if (!orig_peer->GetMessageProcessor().SendRelayDataAck({ rl.GetPort(), event.MessageID }))
						{
							retval = RelayEventProcessResult::Failed;
						}
					}
				}

				if (retval == RelayEventProcessResult::Failed)
				{
					Peer::Peer_ThS::UniqueLockedType in_peer;
					Peer::Peer_ThS::UniqueLockedType out_peer;

					// Get the peers and lock them
					GetUniqueLocks(rl.GetIncomingPeer(), in_peer, rl.GetOutgoingPeer(), out_peer);

					UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, Exception::GeneralFailure);
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

	Manager::RelayEventProcessResult Manager::ProcessRelayEvent(const Events::RelayDataAck& event) noexcept
	{
		auto retval = RelayEventProcessResult::Failed;

		if (auto relayths = Get(event.Port); relayths != nullptr)
		{
			relayths->WithUniqueLock([&](Link& rl) noexcept
			{
				// Event should come from expected origin
				if (!ValidateEventOrigin(event, rl)) return;

				// If relay is not (yet) connected (anymore) don't bother
				if (rl.GetStatus() != Status::Connected && rl.GetStatus() != Status::Suspended)
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
						auto& rdrl = rl.GetDataRateLimiter();

						if (rdrl.AckMTU(event.MessageID, Util::GetCurrentSteadyTime()))
						{
							retval = RelayEventProcessResult::Succeeded;

							// Get the peer and lock it
							GetUniqueLock(*dest_rpeer, dest_peer);

							if (dest_peer) // If peer is present
							{
								// Update socket send event
								dest_peer->GetSocket<Socket>().SetRelayWrite(rdrl.CanAddMTU());
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
							if (dest_peer->GetMessageProcessor().SendRelayDataAck({ event.Port, event.MessageID }))
							{
								retval = RelayEventProcessResult::Succeeded;
							}
						}

						break;
					}
					default:
					{
						assert(false);
						break;
					}
				}

				if (retval == RelayEventProcessResult::Failed)
				{
					Peer::Peer_ThS::UniqueLockedType in_peer;
					Peer::Peer_ThS::UniqueLockedType out_peer;

					// Get the peers and lock them
					GetUniqueLocks(rl.GetIncomingPeer(), in_peer, rl.GetOutgoingPeer(), out_peer);

					UpdateRelayStatus(rl, in_peer, out_peer, Status::Exception, Exception::GeneralFailure);
				}
			});
		}
		else
		{
			// Received event for invalid relay link; this could be an attack
			LogWarn(L"Peer LUID %llu sent relay data ack for an unknown port %llu",
					event.Origin.PeerLUID, event.Port);

			DeterioratePeerReputation(event.Origin.PeerLUID);
		}

		return retval;
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

			DeterioratePeerReputation(event.Origin.PeerLUID, Access::AddressReputationUpdate::DeteriorateSevere);
			return false;
		}

		return true;
	}
}