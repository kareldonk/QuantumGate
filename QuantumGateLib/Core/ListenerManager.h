// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Concurrency\ThreadPool.h"
#include "Peer\PeerManager.h"
#include "Access\AccessManager.h"

#include <variant>

namespace QuantumGate::Implementation::Core::Listener
{
	class Manager final
	{
		struct TCPListenerData
		{
			Network::Socket Socket;
			bool UseConditionalAcceptFunction{ true };
		};

		struct UDPListenerData
		{
			Network::Socket Socket;
		};

		using ListenerDataType = std::variant<TCPListenerData, UDPListenerData>;

		struct ThreadData final
		{
			ThreadData(ListenerDataType&& data) noexcept : ListenerData(std::move(data)) {}
			ThreadData(const ThreadData&) = delete;
			ThreadData(ThreadData&&) noexcept = default;

			~ThreadData()
			{
				std::visit(Util::Overloaded{
					[](TCPListenerData& data)
					{
						if (data.Socket.GetIOStatus().IsOpen()) data.Socket.Close();
					},
				    [](UDPListenerData& data)
					{
						if (data.Socket.GetIOStatus().IsOpen()) data.Socket.Close();
					}
				}, ListenerData);
			}

			ThreadData& operator=(const ThreadData&) = delete;
			ThreadData& operator=(ThreadData&&) noexcept = default;

			ListenerDataType ListenerData;
		};

		struct ThreadPoolData final
		{};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData, ThreadData>;

	public:
		Manager() = delete;
		Manager(const Settings_CThS& settings, Access::Manager& accessmgr, Peer::Manager& peers) noexcept;
		Manager(const Manager&) = delete;
		Manager(Manager&&) noexcept = default;
		~Manager() { if (IsRunning()) Shutdown(); }
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) noexcept = default;

		[[nodiscard]] bool Startup() noexcept;
		[[nodiscard]] bool Startup(const Vector<API::Local::Environment::EthernetInterface>& interfaces) noexcept;
		void Shutdown() noexcept;
		[[nodiscard]] inline bool IsRunning() const noexcept { return m_Running; }

		[[nodiscard]] bool AddTCPListenerThreads(const IPAddress& address, const Vector<UInt16> ports,
											  const bool cond_accept, const bool nat_traversal) noexcept;
		[[nodiscard]] bool AddUDPListenerThreads(const IPAddress& address, const Vector<UInt16> ports,
												 const bool nat_traversal) noexcept;
		std::optional<ThreadPool::Thread> RemoveListenerThread(ThreadPool::Thread&& thread) noexcept;
		[[nodiscard]] bool Update(const Vector<API::Local::Environment::EthernetInterface>& interfaces) noexcept;

	private:
		void PreStartup() noexcept;
		void ResetState() noexcept;

		void WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);

		void AcceptTCPConnection(Network::Socket& listener_socket, const bool cond_accept) noexcept;
		void AcceptUDPConnection(Network::Socket& listener_socket) noexcept;

		[[nodiscard]] bool CanAcceptConnection(const IPAddress& ipaddr) const noexcept;

		static int CALLBACK TCPAcceptConditionFunction(LPWSABUF lpCallerId, LPWSABUF lpCallerData, LPQOS lpSQOS,
													   LPQOS lpGQOS, LPWSABUF lpCalleeId, LPWSABUF lpCalleeData,
													   GROUP FAR* g, DWORD_PTR dwCallbackData) noexcept;

	private:
		std::atomic_bool m_Running{ false };
		std::atomic_bool m_ListeningOnAnyAddresses{ false };
		const Settings_CThS& m_Settings;
		Access::Manager& m_AccessManager;
		Peer::Manager& m_PeerManager;

		ThreadPool m_ListenerThreadPool;
	};
}