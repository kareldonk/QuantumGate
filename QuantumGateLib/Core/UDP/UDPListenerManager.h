// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPConnectionManager.h"
#include "UDPListenerSocket.h"
#include "..\..\Concurrency\ThreadPool.h"
#include "..\Peer\PeerManager.h"
#include "..\Access\AccessManager.h"

namespace QuantumGate::Implementation::Core::UDP::Listener
{
	class Manager final
	{
		using ReceiveBuffer = Memory::StackBuffer<Connection::UDPMessageSizes::Max>;

		struct CookieInfo final
		{
			SteadyTime CreationTime;
			ConnectionID ConnectionID{ 0 };
			IPEndpoint Endpoint;
		};

		using CookieMap = Containers::UnorderedMap<CookieID, CookieInfo>;
		using CookieMap_ThS = Concurrency::ThreadSafe<CookieMap, std::shared_mutex>;

		struct ThreadData final
		{
			ThreadData(const ProtectedBuffer& shared_secret, const bool primary = false) :
				IsPrimary(primary), SymmetricKeys(shared_secret)
			{}

			ThreadData(const ThreadData&) = delete;
			ThreadData(ThreadData&&) noexcept = default;

			~ThreadData()
			{
				if (Socket.GetIOStatus().IsOpen()) Socket.Close();
			}

			ThreadData& operator=(const ThreadData&) = delete;
			ThreadData& operator=(ThreadData&&) noexcept = default;

			bool IsPrimary{ false };
			const SymmetricKeys SymmetricKeys;
			Socket Socket;
			std::shared_ptr<SendQueue_ThS> SendQueue;
		};

		struct ThreadPoolData final
		{
			CookieMap_ThS Cookies;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData, ThreadData>;

	public:
		Manager() = delete;
		Manager(const Settings_CThS& settings, Access::Manager& accessmgr, UDP::Connection::Manager& udpmgr,
				Peer::Manager& peermgr) noexcept;
		Manager(const Manager&) = delete;
		Manager(Manager&&) noexcept = default;
		~Manager() { if (IsRunning()) Shutdown(); }
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) noexcept = default;

		[[nodiscard]] bool Startup() noexcept;
		[[nodiscard]] bool Startup(const Vector<API::Local::Environment::EthernetInterface>& interfaces) noexcept;
		void Shutdown() noexcept;
		[[nodiscard]] inline bool IsRunning() const noexcept { return m_Running; }

		[[nodiscard]] bool AddPrimaryListenerThread(const ProtectedBuffer& shared_secret) noexcept;
		[[nodiscard]] bool AddWorkerListenerThreads(const IPAddress& address, const Vector<UInt16> ports,
													const bool nat_traversal, const ProtectedBuffer& shared_secret) noexcept;
		std::optional<ThreadPool::ThreadType> RemoveListenerThread(ThreadPool::ThreadType&& thread) noexcept;
		[[nodiscard]] bool Update(const Vector<API::Local::Environment::EthernetInterface>& interfaces) noexcept;

	private:
		void PreStartup() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] ReceiveBuffer& GetReceiveBuffer() const noexcept;

		void PrimaryThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);
		void WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);

		[[nodiscard]] bool CanAcceptConnection(const IPAddress& ipaddr) const noexcept;
		[[nodiscard]] std::pair<bool, Access::IPReputationUpdate> AcceptConnection(const std::shared_ptr<SendQueue_ThS>& send_queue,
																				   const IPEndpoint& lendpoint, const IPEndpoint& pendpoint,
																				   BufferSpan& buffer, const SymmetricKeys& symkeys) noexcept;

		void SendCookie(const std::shared_ptr<SendQueue_ThS>& send_queue, const IPEndpoint& pendpoint,
						const ConnectionID connectionid, const SymmetricKeys& symkeys) noexcept;

		[[nodiscard]] std::optional<Message::CookieData> GetCookie(const ConnectionID connectionid,
																   const IPEndpoint& pendpoint) noexcept;
		[[nodiscard]] bool VerifyCookie(const Message::CookieData& cookie, const ConnectionID connectionid,
										const IPEndpoint& pendpoint) noexcept;
		void CheckCookieExpiration(const std::chrono::seconds expiration_time) noexcept;

	private:
		std::atomic_bool m_Running{ false };
		std::atomic_bool m_ListeningOnAnyAddresses{ false };
		const Settings_CThS& m_Settings;
		Access::Manager& m_AccessManager;
		UDP::Connection::Manager& m_UDPConnectionManager;
		Peer::Manager& m_PeerManager;

		ThreadPool m_ThreadPool;
	};
}