// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "ExtenderControl.h"
#include "ExtenderManager.h"
#include "..\..\API\PeerEvent.h"

namespace QuantumGate::Implementation::Core::Extender
{
	Control::Control(Manager & mgr, const std::shared_ptr<QuantumGate::API::Extender>& extender,
					 const ExtenderModuleID moduleid) noexcept :
		m_ExtenderManager(mgr), m_Extender(extender), m_ExtenderModuleID(moduleid)
	{
		m_SteadyTimeAdded = Util::GetCurrentSteadyTime();
	}

	void Control::SetStatus(const Status status) noexcept
	{
		switch (status)
		{
			case Status::Startup:
			case Status::Running:
				break;
			case Status::Shutdown:
			case Status::Stopped:
				m_SteadyTimeRemoved = Util::GetCurrentSteadyTime();
				break;
			default:
				assert(false);
				break;
		}

		m_Status = status;
	}

	void Control::PreStartupExtenderThreadPools() noexcept
	{
		ResetState();
	}

	void Control::ResetState() noexcept
	{
		m_Peers.clear();
		m_ThreadPools.clear();
	}

	const bool Control::StartupExtenderThreadPools() noexcept
	{
		PreStartupExtenderThreadPools();

		const auto extname = GetExtenderName();

		Size numthreadpools = 1u;
		Size numthreadsperpool = 1u;

		const auto& settings = m_ExtenderManager.GetSettings();

		const auto cth = std::thread::hardware_concurrency();
		numthreadpools = (cth > settings.Local.MinThreadPools) ? cth : settings.Local.MinThreadPools;

		if (numthreadsperpool < settings.Local.MinThreadsPerPool)
		{
			numthreadsperpool = settings.Local.MinThreadsPerPool;
		}

		// Must have at least one thread pool, and at least one thread per pool 
		assert(numthreadpools > 0 && numthreadsperpool > 0);

		LogSys(L"Creating %u extender %s with %u worker %s %s",
			   numthreadpools, numthreadpools > 1 ? L"threadpools" : L"threadpool",
			   numthreadsperpool, numthreadsperpool > 1 ? L"threads" : L"thread",
			   numthreadpools > 1 ? L"each" : L"");

		auto error = false;

		// Create the threadpools
		for (auto i = 0u; i < numthreadpools; i++)
		{
			try
			{
				auto thpool = std::make_unique<ThreadPool>(m_ExtenderManager, GetExtender());

				thpool->SetWorkerThreadsMaxBurst(settings.Local.WorkerThreadsMaxBurst);
				thpool->SetWorkerThreadsMaxSleep(settings.Local.WorkerThreadsMaxSleep);

				// Create the worker threads
				for (auto x = 0u; x < numthreadsperpool; x++)
				{
					if (!thpool->AddThread(extname + L" Thread", &Control::WorkerThreadProcessor,
										   ThreadData(), &thpool->Data().Queue.WithUniqueLock()->Event()))
					{
						error = true;
						break;
					}
				}

				if (!error && thpool->Startup())
				{
					m_ThreadPools[i] = std::move(thpool);
				}
				else
				{
					LogErr(L"Couldn't start an extender threadpool");
					error = true;
				}
			}
			catch (...) { error = true; }

			if (error) break;
		}

		return !error;
	}

	void Control::ShutdownExtenderThreadPools() noexcept
	{
		for (const auto& thpool : m_ThreadPools)
		{
			thpool.second->Shutdown();
			thpool.second->Clear();
			thpool.second->Data().Queue.WithUniqueLock()->Clear();
		}

		ResetState();
	}

	const std::pair<bool, bool> Control::WorkerThreadProcessor(ThreadPoolData& thpdata,
															   ThreadData& thdata,
															   const Concurrency::EventCondition& shutdown_event)
	{
		auto didwork = false;
		auto addtoqueue = false;

		std::shared_ptr<Peer_ThS> peerctrl = nullptr;

		thpdata.Queue.IfUniqueLock([&](auto& queue)
		{
			if (!queue.Empty())
			{
				// Make copy of shared_ptr (instead of reference) to take shared ownership 
				// in case peer gets removed while we do work
				peerctrl = queue.Front();
				queue.Pop();

				// We had peers in the queue so we did work
				didwork = true;
			}
		});

		if (peerctrl != nullptr)
		{
			// Peer events have priority; process as many as we can from the queue,
			// then move on to message events if the peer is still connected

			const auto maxnum = thpdata.ExtenderManager.GetSettings().Local.WorkerThreadsMaxBurst;
			auto num = 0u;

			while (num < maxnum && !shutdown_event.IsSet())
			{
				Core::Peer::Event event;
				peerctrl->WithUniqueLock([&](Peer& peer)
				{
					if (!peer.EventQueue.Empty())
					{
						event = std::move(peer.EventQueue.Front());
						peer.EventQueue.Pop();

						num++;
					}
				});

				if (event)
				{
					thpdata.Extender.OnPeerEvent(QuantumGate::API::PeerEvent(std::move(event)));
				}
				else break;
			}

			while (num < maxnum && !shutdown_event.IsSet())
			{
				Core::Peer::Event event;
				peerctrl->WithUniqueLock([&](Peer& peer)
				{
					if (!peer.MessageQueue.Empty() && peer.Status == Peer::Status::Connected)
					{
						event = std::move(peer.MessageQueue.Front());
						peer.MessageQueue.Pop();

						num++;
					}
				});

				if (event)
				{
					const auto pluid = event.GetPeerLUID();
					const auto retval = thpdata.Extender.OnPeerMessage(QuantumGate::API::PeerEvent(std::move(event)));
					if ((!retval.first || !retval.second) &&
						!thpdata.Extender.HadException())
					{
						thpdata.ExtenderManager.m_UnhandledExtenderMessageCallbacks.WithSharedLock()(thpdata.Extender.GetUUID(),
																									 pluid, retval);
						break;
					}
				}
				else break;
			}

			peerctrl->WithUniqueLock([&](Peer& peer)
			{
				// If we still have peer events or, messages while the peer is
				// still connected, then add it back into the queue and we'll come 
				// back later to continue processing
				if (!peer.EventQueue.Empty() ||
					(!peer.MessageQueue.Empty() && peer.Status == Peer::Status::Connected))
				{
					addtoqueue = true;
				}

				if (!addtoqueue) peer.IsInQueue = false;
			});
		}

		if (addtoqueue) thpdata.Queue.WithUniqueLock()->Push(peerctrl);

		return std::make_pair(true, didwork);
	}

	const bool Control::AddPeerEvent(Core::Peer::Event&& event) noexcept
	{
		assert(event.GetType() != PeerEventType::Unknown);

		try
		{
			std::shared_ptr<Peer_ThS> peerctrl = nullptr;

			if (event.GetType() == PeerEventType::Connected)
			{
				// Connect event means we should add a new peer;
				// get the threadpool with the least amount of peers so
				// that there's an even distribution among all available pools
				const auto thpit = std::min_element(m_ThreadPools.begin(), m_ThreadPools.end(),
													[](const auto& a, const auto& b)
				{
					return (a.second->Data().PeerCount < b.second->Data().PeerCount);
				});

				assert(thpit != m_ThreadPools.end());

				peerctrl = std::make_shared<Peer_ThS>(thpit->first,
													  thpit->second->Data().PeerCount, Peer::Status::Connected);

				// If this fails there was already a peer in the map; this should not happen
				[[maybe_unused]] const auto[it, success] = m_Peers.insert({ event.GetPeerLUID(), peerctrl });

				assert(success);

				if (!success)
				{
					LogErr(L"Couldn't add peer to extender; a peer with LUID %llu already exists", event.GetPeerLUID());
					return false;
				}
			}
			else
			{
				// Peer should already exist if we get here
				if (const auto it = m_Peers.find(event.GetPeerLUID()); it != m_Peers.end())
				{
					// Making a copy of the shared_ptr to take shared
					// ownership; important for when peer gets removed
					// when disconnected and we still work on it
					peerctrl = it->second;
				}
				else
				{
					// Should never get here
					assert(false);

					LogErr(L"Couldn't find peer with LUID %llu in extender peer map", event.GetPeerLUID());
					return false;
				}

				assert(peerctrl != nullptr);

				if (event.GetType() == PeerEventType::Disconnected)
				{
					peerctrl->WithUniqueLock()->Status = Peer::Status::Disconnected;
					m_Peers.erase(event.GetPeerLUID());
				}
			}

			auto addtoqueue = false;
			UInt thpoolkey = 0u;

			peerctrl->WithUniqueLock([&](Peer& peer)
			{
				thpoolkey = peer.ThreadPoolKey;

				if (event.GetType() == PeerEventType::Message)
				{
					peer.MessageQueue.Push(std::move(event));
				}
				else peer.EventQueue.Push(std::move(event));

				if (!peer.IsInQueue)
				{
					addtoqueue = true;
					peer.IsInQueue = true;
				}
			});

			if (addtoqueue) m_ThreadPools[thpoolkey]->Data().Queue.WithUniqueLock()->Push(peerctrl);

			return true;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Exception while adding peer event to extender with UUID %llu - %s",
				   event.GetExtenderUUID(), Util::ToStringW(e.what()).c_str());
		}

		return false;
	}
}
