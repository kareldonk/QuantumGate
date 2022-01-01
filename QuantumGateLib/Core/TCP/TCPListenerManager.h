// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Concurrency\ThreadPool.h"
#include "..\Peer\PeerManager.h"
#include "..\Access\AccessManager.h"

namespace QuantumGate::Implementation::Core::TCP::Listener
{
	class Manager final
	{
		struct ThreadData final
		{
			ThreadData() noexcept = default;
			ThreadData(const ThreadData&) = delete;
			ThreadData(ThreadData&&) noexcept = default;

			~ThreadData()
			{
				if (Socket.GetIOStatus().IsOpen()) Socket.Close();
			}

			ThreadData& operator=(const ThreadData&) = delete;
			ThreadData& operator=(ThreadData&&) noexcept = default;

			Network::Socket Socket;
			bool UseConditionalAcceptFunction{ true };
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

		[[nodiscard]] bool AddListenerThreads(const IPAddress& address, const Vector<UInt16> ports,
											  const bool cond_accept, const bool nat_traversal) noexcept;
		std::optional<ThreadPool::ThreadType> RemoveListenerThread(ThreadPool::ThreadType&& thread) noexcept;
		[[nodiscard]] bool Update(const Vector<API::Local::Environment::EthernetInterface>& interfaces) noexcept;

	private:
		void PreStartup() noexcept;
		void ResetState() noexcept;

		void WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);

		void AcceptConnection(Network::Socket& listener_socket, const bool cond_accept) noexcept;

		[[nodiscard]] bool CanAcceptConnection(const Address& ipaddr) const noexcept;

		static int CALLBACK AcceptConditionFunction(LPWSABUF lpCallerId, LPWSABUF lpCallerData, LPQOS lpSQOS,
													LPQOS lpGQOS, LPWSABUF lpCalleeId, LPWSABUF lpCalleeData,
													GROUP FAR* g, DWORD_PTR dwCallbackData) noexcept;

	private:
		std::atomic_bool m_Running{ false };
		std::atomic_bool m_ListeningOnAnyAddresses{ false };
		const Settings_CThS& m_Settings;
		Access::Manager& m_AccessManager;
		Peer::Manager& m_PeerManager;

		ThreadPool m_ThreadPool;
	};
}