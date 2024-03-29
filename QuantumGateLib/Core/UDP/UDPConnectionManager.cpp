// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnectionManager.h"
#include "..\..\Crypto\Crypto.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"UDP connectionmanager starting...");

		PreStartup();

		if (!StartupThreadPool())
		{
			ShutdownThreadPool();

			LogErr(L"UDP connectionmanager startup failed");

			return false;
		}

		m_Running = true;

		LogSys(L"UDP connectionmanager startup successful");

		return true;
	}

	void Manager::Shutdown() noexcept
	{
		if (!m_Running) return;

		m_Running = false;

		LogSys(L"UDP connectionmanager shutting down...");

		ShutdownThreadPool();

		ResetState();

		LogSys(L"UDP connectionmanager shut down");
	}

	void Manager::PreStartup() noexcept
	{
		ResetState();
	}

	void Manager::ResetState() noexcept
	{
		m_ThreadPool.GetData().NumIncomingHandshakesInProgress = 0;
		m_ThreadPool.GetData().ThreadKeyToConnectionTotals.WithUniqueLock()->clear();
	}

	bool Manager::StartupThreadPool() noexcept
	{
		const auto& settings = GetSettings();

		const auto numthreadsperpool = Util::GetNumThreadsPerPool(settings.Local.Concurrency.UDPConnectionManager.MinThreads,
																  settings.Local.Concurrency.UDPConnectionManager.MaxThreads, 1u);

		// Must have at least one thread in pool
		assert(numthreadsperpool > 0);

		LogSys(L"Creating UDP connection threadpool with %zu worker %s",
			   numthreadsperpool, numthreadsperpool > 1 ? L"threads" : L"thread");

		auto error = false;

		// Create the worker threads
		for (Size x = 0; x < numthreadsperpool && !error; ++x)
		{
			try
			{
				auto thdata = ThreadData(x);
				if (thdata.WorkEvents->Initialize())
				{
					if (m_ThreadPool.AddThread(L"QuantumGate UDP connectionmanager Thread", std::move(thdata),
											   MakeCallback(this, &Manager::WorkerThreadProcessor),
											   MakeCallback(this, &Manager::WorkerThreadWait)))
					{
						// Add entry for the total number of relay links this thread is handling
						m_ThreadPool.GetData().ThreadKeyToConnectionTotals.WithUniqueLock([&](auto& con_totals)
						{
							[[maybe_unused]] const auto [it, inserted] = con_totals.insert({ x, 0 });
							if (!inserted)
							{
								error = true;
							}
						});
					}
					else error = true;
				}
				else error = true;
			}
			catch (...) { error = true; }
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

		auto thread = m_ThreadPool.GetFirstThread();
		while (thread)
		{
			thread->GetData().WorkEvents->Deinitialize();

			thread = m_ThreadPool.GetNextThread(*thread);
		}

		m_ThreadPool.Clear();

		DbgInvoke([&]() noexcept
		{
			// If all threads are shut down, and connections
			// are cleared the count should be zero
			assert(m_ThreadPool.GetData().NumIncomingHandshakesInProgress == 0);
		});
	}

	std::optional<Manager::ThreadKey> Manager::GetThreadKeyWithLeastConnections() const noexcept
	{
		std::optional<ThreadKey> thkey;

		// Get the thread with the least amount of connections
		m_ThreadPool.GetData().ThreadKeyToConnectionTotals.WithSharedLock([&](const auto& con_totals)
		{
			// Should have at least one item (at least one worker thread running)
			assert(con_totals.size() > 0);

			const auto it = std::min_element(con_totals.begin(), con_totals.end(),
											 [](const auto& a, const auto& b)
			{
				return (a.second < b.second);
			});

			assert(it != con_totals.end());

			thkey = it->first;
		});

		return thkey;
	}

	std::optional<Manager::ThreadPool::ThreadType> Manager::GetThreadWithLeastConnections() noexcept
	{
		const auto thkey = GetThreadKeyWithLeastConnections();
		if (thkey)
		{
			auto thread = m_ThreadPool.GetFirstThread();
			while (thread)
			{
				if (thread->GetData().ThreadKey == *thkey)
				{
					return thread;
				}
				else thread = m_ThreadPool.GetNextThread(*thread);
			}

			LogErr(L"Couldn't find UDP connectionmanager thread with key %llu", *thkey);
		}

		return std::nullopt;
	}

	void Manager::WorkerThreadWait(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		const auto result = thdata.WorkEvents->Wait(1ms);
		if (!result.Waited)
		{
			shutdown_event.Wait(1ms);
		}
	}

	void Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		std::optional<Containers::List<ConnectionID>> remove_list;

		auto connections = thdata.Connections->WithUniqueLock();
		for (auto it = connections->begin(); it != connections->end() && !shutdown_event.IsSet(); ++it)
		{
			// Placed in the loop to have the latest time for each connection
			const auto current_steadytime = Util::GetCurrentSteadyTime();
			const auto current_systemtime = Util::GetCurrentSystemTime();

			auto& connection = it->second;

			connection.ProcessEvents(current_steadytime, current_systemtime);

			if (connection.ShouldClose())
			{
				// Collect the connection for removal
				if (!remove_list.has_value()) remove_list.emplace();

				remove_list->emplace_back(connection.GetID());
			}
		}

		// Remove all connections that were collected for removal
		if (remove_list.has_value() && !remove_list->empty())
		{
			LogDbg(L"Removing UDP connections");
			RemoveConnections(*remove_list, *connections, thdata);

			remove_list->clear();
		}
	}

	bool Manager::AddConnection(const Network::AddressFamily af, const PeerConnectionType type,
								const ConnectionID id, const Message::SequenceNumber seqnum, ProtectedBuffer&& handshake_data,
								Socket& socket, std::optional<ProtectedBuffer>&& shared_secret) noexcept
	{
		assert(m_Running);

		try
		{
			const auto& settings = m_Settings.GetCache();
			const auto nat_traversal = settings.Local.Listeners.UDP.NATTraversal;

			auto thread = GetThreadWithLeastConnections();
			if (thread)
			{
				ConnectionMap::iterator cit;

				{
					std::unique_ptr<Connection::HandshakeTracker> handshake_tracker;
					if (type == PeerConnectionType::Inbound)
					{
						handshake_tracker = std::make_unique<Connection::HandshakeTracker>(
							m_ThreadPool.GetData().NumIncomingHandshakesInProgress);
					}

					auto connections = thread->GetData().Connections->WithUniqueLock();

					[[maybe_unused]] const auto [it, inserted] = connections->try_emplace(id, m_Settings, m_KeyManager, m_AccessManager,
																						  type, id, seqnum, std::move(handshake_data), 
																						  std::move(shared_secret),
																						  std::move(handshake_tracker));

					assert(inserted);
					if (!inserted)
					{
						LogErr(L"Couldn't add new UDP connection; a connection with ID %llu already exists", id);
						return false;
					}

					if (!it->second.Open(af, nat_traversal, socket))
					{
						LogErr(L"Couldn't open new UDP connection");
						connections->erase(it);
						return false;
					}

					cit = it;
				}

				auto sg1 = MakeScopeGuard([&]
				{
					cit->second.Close();
					thread->GetData().Connections->WithUniqueLock()->erase(cit);
				});

				if (!IncrementThreadConnectionTotal(thread->GetData().ThreadKey))
				{
					LogErr(L"Couldn't add new UDP connection; failed to increment thread connection total");
					return false;
				}

				auto sg2 = MakeScopeGuard([&]
				{
					if (!DecrementThreadConnectionTotal(thread->GetData().ThreadKey))
					{
						LogErr(L"UDP connectionmanager failed to decrement thread connection total");
					}
				});

				if (!thread->GetData().WorkEvents->AddEvent(cit->second.GetReadEvent()))
				{
					LogErr(L"Couldn't add new UDP connection; failed to add read event");
					return false;
				}

				sg1.Deactivate();
				sg2.Deactivate();

				return true;
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Exception while adding connection to UDP connectionmanager - %s", Util::ToStringW(e.what()).c_str());
		}

		return false;
	}

	void Manager::RemoveConnection(const ConnectionID id, ConnectionMap& connections, const ThreadData& thdata) noexcept
	{
		const auto it = connections.find(id);
		if (it != connections.end())
		{
			thdata.WorkEvents->RemoveEvent(it->second.GetReadEvent());

			it->second.Close();

			connections.erase(it);

			if (!DecrementThreadConnectionTotal(thdata.ThreadKey))
			{
				LogErr(L"UDP connectionmanager failed to decrement thread connection total");
			}
		}
		else
		{
			LogErr(L"UDP connectionmanager failed to remove connection %llu; the connection wasn't found", id);
		}
	}

	void Manager::RemoveConnections(const Containers::List<ConnectionID>& list, ConnectionMap& connections,
									const ThreadData& thdata) noexcept
	{
		for (const auto id : list)
		{
			RemoveConnection(id, connections, thdata);
		}
	}

	bool Manager::IncrementThreadConnectionTotal(const ThreadKey key) noexcept
	{
		auto success = false;

		m_ThreadPool.GetData().ThreadKeyToConnectionTotals.WithUniqueLock([&](auto& con_totals)
		{
			if (const auto it = con_totals.find(key); it != con_totals.end())
			{
				++it->second;
				success = true;
			}
		});

		return success;
	}

	bool Manager::DecrementThreadConnectionTotal(const ThreadKey key) noexcept
	{
		auto success = false;

		m_ThreadPool.GetData().ThreadKeyToConnectionTotals.WithUniqueLock([&](auto& con_totals)
		{
			if (const auto it = con_totals.find(key); it != con_totals.end())
			{
				assert(it->second > 0);
				--it->second;
				success = true;
			}
		});

		return success;
	}

	void Manager::OnLocalIPInterfaceChanged() noexcept
	{
		auto thread = m_ThreadPool.GetFirstThread();
		while (thread)
		{
			auto connections = thread->GetData().Connections->WithUniqueLock();

			for (auto& connection : *connections)
			{
				connection.second.OnLocalIPInterfaceChanged();
			}

			thread = m_ThreadPool.GetNextThread(*thread);
		}
	}

	Manager::AddQueryCode Manager::QueryAddConnection(const ConnectionID id, const IPEndpoint& pendpoint,
													  const PeerConnectionType type) const noexcept
	{
		const auto& settings = m_Settings.GetCache();

		using int_type = decltype(m_ThreadPool.GetData().NumIncomingHandshakesInProgress.load());

		if (type == PeerConnectionType::Inbound && m_ThreadPool.GetData().NumIncomingHandshakesInProgress >=
			static_cast<int_type>(settings.UDP.ConnectCookieRequirementThreshold))
		{
			return AddQueryCode::RequireSynCookie;
		}

		auto thread = m_ThreadPool.GetFirstThread();
		while (thread)
		{
			auto connections = thread->GetData().Connections->WithSharedLock();

			const auto it = connections->find(id);
			if (it != connections->end())
			{
				if (it->second.GetType() == type)
				{
					if (it->second.GetPeerEndpoint().GetIPAddress() == pendpoint.GetIPAddress())
					{
						return AddQueryCode::ConnectionAlreadyExists;
					}
					else return AddQueryCode::ConnectionIDInUse;
				}
			}

			thread = m_ThreadPool.GetNextThread(*thread);
		}

		return AddQueryCode::OK;
	}
}