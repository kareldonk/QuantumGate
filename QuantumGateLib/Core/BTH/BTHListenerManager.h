// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Concurrency\ThreadPool.h"
#include "..\Peer\PeerManager.h"
#include "..\Access\AccessManager.h"

namespace QuantumGate::Implementation::Core::BTH::Listener
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
		[[nodiscard]] bool Startup(const Vector<API::Local::Environment::BluetoothRadio>& radios) noexcept;
		void Shutdown() noexcept;
		[[nodiscard]] inline bool IsRunning() const noexcept { return m_Running; }

		[[nodiscard]] bool AddListenerThreads(const BTHAddress& address, const Vector<UInt16> ports, const bool require_auth,
											  const BluetoothServiceDetails& service_details) noexcept;
		std::optional<ThreadPool::ThreadType> RemoveListenerThread(ThreadPool::ThreadType&& thread,
																   const BluetoothServiceDetails& service_details) noexcept;
		[[nodiscard]] bool Update(const Vector<API::Local::Environment::BluetoothRadio>& radios) noexcept;

	private:
		void PreStartup() noexcept;
		void ResetState() noexcept;

		void WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);

		void AcceptConnection(Network::Socket& listener_socket) noexcept;

		[[nodiscard]] bool CanAcceptConnection(const Address& addr) const noexcept;

	public:
		static inline constexpr const WChar* DefaultServiceName{ L"QuantumGate" };
		static inline constexpr const WChar* DefaultServiceComment{ L"QuantumGate Bluetooth Service" };

	private:
		std::atomic_bool m_Running{ false };
		std::atomic_bool m_ListeningOnAnyAddresses{ false };
		const Settings_CThS& m_Settings;
		Access::Manager& m_AccessManager;
		Peer::Manager& m_PeerManager;

		ThreadPool m_ThreadPool;
	};
}