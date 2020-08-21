// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPSocket.h"
#include "..\..\Concurrency\ThreadPool.h"
#include "..\..\Concurrency\EventGroup.h"
#include "..\..\Concurrency\Queue.h"
#include "..\..\Network\Socket.h"

namespace QuantumGate::Implementation::Core::UDP::Listener
{
	class Manager;
}

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Manager final
	{
		friend class Listener::Manager;

		using ConnectionID = UInt64;

		struct Connection final
		{
			using ListenerDataQueue = Concurrency::Queue<Buffer>;

			PeerConnectionType Type{ PeerConnectionType::Unknown };
			ConnectionID ID{ 0 };
			Network::Socket Socket;
			IPEndpoint LocalEndpoint;
			IPEndpoint PeerEndpoint;
			std::shared_ptr<Socket::Buffers_ThS> Buffers;
			std::unique_ptr<ListenerDataQueue> ListenerData;
		};

		using ConnectionMap = Containers::UnorderedMap<ConnectionID, Connection>;
		using ConnectionMap_ThS = Concurrency::ThreadSafe<ConnectionMap, std::shared_mutex>;

		using ThreadKey = UInt64;

		using ThreadKeyToConnectionTotalMap = Containers::UnorderedMap<ThreadKey, Size>;
		using ThreadKeyToConnectionTotalMap_ThS = Concurrency::ThreadSafe<ThreadKeyToConnectionTotalMap, Concurrency::SharedSpinMutex>;

		struct ThreadData final
		{
			ThreadData(const ThreadKey thread_key) noexcept :
				ThreadKey(thread_key),
				WorkEvents(std::make_unique<Concurrency::EventGroup>()),
				Connections(std::make_unique<ConnectionMap_ThS>())
			{}

			ThreadKey ThreadKey{ 0 };
			std::unique_ptr<Concurrency::EventGroup> WorkEvents;
			std::unique_ptr<ConnectionMap_ThS> Connections;
		};

		struct ThreadPoolData final
		{
			ThreadKeyToConnectionTotalMap_ThS ThreadKeyToConnectionTotals;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData, ThreadData>;

	public:
		Manager() = delete;
		Manager(const Settings_CThS& settings) noexcept : m_Settings(settings) {}
		Manager(const Manager&) = delete;
		Manager(Manager&&) noexcept = default;
		~Manager() { if (IsRunning()) Shutdown(); }
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) noexcept = default;

		[[nodiscard]] bool Startup() noexcept;
		void Shutdown() noexcept;
		[[nodiscard]] inline bool IsRunning() const noexcept { return m_Running; }

		[[nodiscard]] bool AddConnection(const Network::IP::AddressFamily af, const PeerConnectionType type,
										 Socket& socket) noexcept;

	private:
		[[nodiscard]] inline const Settings& GetSettings() const noexcept { return m_Settings.GetCache(); }

		void PreStartup() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] bool StartupThreadPool() noexcept;
		void ShutdownThreadPool() noexcept;

		std::optional<ConnectionID> MakeConnectionID() const noexcept;
		std::optional<ThreadKey> GetThreadKeyWithLeastConnections() const noexcept;

		void ProcessIncomingListenerData(const IPEndpoint endpoint, Buffer&& buffer) noexcept;

		void WorkerThreadWait(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);
		void WorkerThreadWaitInterrupt(ThreadPoolData& thpdata, ThreadData& thdata);
		void WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);

	private:
		std::atomic_bool m_Running{ false };
		const Settings_CThS& m_Settings;

		ThreadPool m_ThreadPool;
	};
}