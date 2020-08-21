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

	std::optional<Manager::ConnectionID> Manager::MakeConnectionID() const noexcept
	{
		if (IsRunning())
		{
			if (const auto cid = Crypto::GetCryptoRandomNumber(); cid.has_value())
			{
				return { *cid };
			}
		}

		return std::nullopt;
	}

	bool Manager::AddConnection(const Network::IP::AddressFamily af, const PeerConnectionType type, Socket& socket) noexcept
	{
		try
		{
			const auto cid = MakeConnectionID();
			if (cid)
			{
				Connection connection;
				connection.Type = type;
				connection.ID = *cid;
				connection.Socket = Network::Socket(af, Network::Socket::Type::Datagram, Network::IP::Protocol::UDP);
				connection.Buffers = std::make_shared<Socket::Buffers_ThS>(connection.Socket.GetEvent());
				connection.LocalEndpoint = IPEndpoint(IPEndpoint::Protocol::UDP,
													  (af == BinaryIPAddress::Family::IPv4) ?
													  IPAddress::AnyIPv4() : IPAddress::AnyIPv6(), 0);

				const auto& settings = m_Settings.GetCache();
				const auto nat_traversal = settings.Local.Listeners.NATTraversal;

				if (connection.Socket.Bind(connection.LocalEndpoint, nat_traversal))
				{
					socket.SetBuffers(connection.Buffers);

					const auto thkey = GetThreadKeyWithLeastConnections();
					if (thkey)
					{
						auto thread = m_ThreadPool.GetFirstThread();
						while (thread)
						{
							if (thread->GetData().ThreadKey == thkey)
							{
								thread->GetData().Connections->WithUniqueLock()->emplace(*cid, std::move(connection));
								return true;
							}

							thread = m_ThreadPool.GetNextThread(*thread);
						}

						LogErr(L"Couldn't find UDP connectionmanager thread with key %llu", thkey);
					}
				}
			}
			else
			{
				LogErr(L"Couldn't create UDP connection ID; UDP connectionmanager may not be running");
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Exception while adding connection to UDP connectionmanager - %s", Util::ToStringW(e.what()).c_str());
		}

		return false;
	}

	void Manager::PreStartup() noexcept
	{
		ResetState();
	}

	void Manager::ResetState() noexcept
	{
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
		
		//auto error = !m_ThreadPool.GetData().WorkEvents.Initialize();
		auto error = false;

		// Create the worker threads
		for (Size x = 0; x < numthreadsperpool && !error; ++x)
		{
			try
			{
				if (m_ThreadPool.AddThread(L"QuantumGate UDP connectionmanager Thread", ThreadData(x),
										   MakeCallback(this, &Manager::WorkerThreadProcessor),
										   MakeCallback(this, &Manager::WorkerThreadWait),
										   MakeCallback(this, &Manager::WorkerThreadWaitInterrupt)))
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
		m_ThreadPool.Clear();
	}

	std::optional<Manager::ThreadKey> Manager::GetThreadKeyWithLeastConnections() const noexcept
	{
		std::optional<ThreadKey> thkey;

		// Get the threadpool with the least amount of relay links
		m_ThreadPool.GetData().ThreadKeyToConnectionTotals.WithSharedLock([&](const auto& con_totals)
		{
			// Should have at least one item (at least
			// one event worker thread running)
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

	void Manager::ProcessIncomingListenerData(const IPEndpoint endpoint, Buffer&& buffer) noexcept
	{
	}

	void Manager::WorkerThreadWait(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		auto result = thdata.WorkEvents->Wait(1ms);
		if (!result.Waited)
		{
			shutdown_event.Wait(1ms);
		}
	}

	void Manager::WorkerThreadWaitInterrupt(ThreadPoolData& thpdata, ThreadData& thdata)
	{}

	void Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{}
}