// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "TCP\TCPListenerManager.h"
#include "UDP\UDPListenerManager.h"
#include "KeyGeneration\KeyGenerationManager.h"

namespace QuantumGate::Implementation::Core
{
	class Local final
	{
		friend class Extender::Extender;

		using ExtenderModuleMap = Containers::UnorderedMap<Extender::ExtenderModuleID, Extender::Module>;

		struct Events final
		{
			struct LocalEnvironmentChange final
			{};

			struct UnhandledExtenderException final
			{
				ExtenderUUID UUID;
			};
		};

		using Event = std::variant<Events::LocalEnvironmentChange, Events::UnhandledExtenderException>;
		using EventQueue_ThS = Concurrency::Queue<Event>;

		struct ThreadPoolData final
		{
			EventQueue_ThS EventQueue;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData>;

	public:
		Local();
		Local(const Local&) = delete;
		Local(Local&&) noexcept = default;
		~Local();
		Local& operator=(const Local&) = delete;
		Local& operator=(Local&&) noexcept = default;

		inline const Settings_CThS& GetSettings() const noexcept { return m_Settings; }

		Result<> Startup(const StartupParameters& params) noexcept;
		Result<> Shutdown() noexcept;
		inline bool IsRunning() const noexcept { return (m_Running && !m_ShutdownEvent.IsSet()); }

		Result<> EnableListeners(const API::Local::ListenerType type) noexcept;
		Result<> DisableListeners(const API::Local::ListenerType type) noexcept;
		bool AreListenersEnabled(const API::Local::ListenerType type) const noexcept;
		
		Result<> UpdateListeners() noexcept;

		Result<> EnableExtenders() noexcept;
		Result<> DisableExtenders() noexcept;
		bool AreExtendersEnabled() const noexcept;

		Result<> EnableRelays() noexcept;
		Result<> DisableRelays() noexcept;
		bool AreRelaysEnabled() const noexcept;

		const LocalEnvironment_ThS& GetEnvironment() noexcept;

		inline Access::Manager& GetAccessManager() noexcept { return m_AccessManager; }
		inline KeyGeneration::Manager& GetKeyGenerationManager() noexcept { return m_KeyGenerationManager; }
		inline Extender::Manager& GetExtenderManager() noexcept { return m_ExtenderManager; }

		Result<bool> AddExtender(const std::shared_ptr<API::Extender>& extender) noexcept;
		Result<> RemoveExtender(const std::shared_ptr<API::Extender>& extender) noexcept;

		Result<> AddExtenderModule(const Path& module_path) noexcept;
		Result<> RemoveExtenderModule(const Path& module_path) noexcept;

		bool HasExtender(const ExtenderUUID& extuuid) const noexcept;
		std::weak_ptr<API::Extender> GetExtender(const ExtenderUUID& extuuid) const noexcept;

		Result<API::Peer> ConnectTo(ConnectParameters&& params) noexcept;
		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params,
													ConnectCallback&& function) noexcept;

		Result<> DisconnectFrom(const PeerLUID pluid) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept;
		Result<> DisconnectFrom(API::Peer& peer) noexcept;
		Result<> DisconnectFrom(API::Peer& peer, DisconnectCallback&& function) noexcept;

		std::tuple<UInt, UInt, UInt, UInt> GetVersion() const noexcept;
		String GetVersionString() const noexcept;
		std::pair<UInt, UInt> GetProtocolVersion() const noexcept;
		String GetProtocolVersionString() const noexcept;

		Result<PeerUUID> GetUUID() const noexcept;

		Result<API::Peer> GetPeer(const PeerLUID pluid) const noexcept;

		Result<Vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;
		Result<> QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept;

		Result<> SetSecurityLevel(const SecurityLevel level,
								  const std::optional<SecurityParameters>& params = std::nullopt,
								  const bool silent = false) noexcept;
		[[nodiscard]] SecurityLevel GetSecurityLevel() const noexcept;
		[[nodiscard]] SecurityParameters GetSecurityParameters() const noexcept;
		void SetDefaultSecuritySettings(Settings& settings) noexcept;

		void FreeUnusedMemory() noexcept;

	private:
		[[nodiscard]] bool StartupThreadPool() noexcept;
		void ShutdownThreadPool() noexcept;

		void OnLocalEnvironmentChanged() noexcept;
		void OnUnhandledExtenderException(const ExtenderUUID extuuid) noexcept;

		Result<bool> AddExtenderImpl(const std::shared_ptr<API::Extender>& extender,
									 const Extender::ExtenderModuleID moduleid = 0) noexcept;
		Result<> RemoveExtenderImpl(const std::shared_ptr<API::Extender>& extender,
									const Extender::ExtenderModuleID moduleid = 0) noexcept;

		bool ValidateInitParameters(const StartupParameters& params) const noexcept;
		bool ValidateSupportedAlgorithms(const Algorithms& algorithms) const noexcept;
		bool ValidateSecurityParameters(const SecurityParameters& params) const noexcept;

		Result<Size> Send(const ExtenderUUID& uuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						  const PeerLUID id, const BufferView& buffer, const SendParameters& params,
						  SendCallback&& callback) noexcept;
		Result<Size> Send(const ExtenderUUID& uuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						  API::Peer& peer, const BufferView& buffer, const SendParameters& params,
						  SendCallback&& callback) noexcept;
		Result<> SendTo(const ExtenderUUID& uuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						const PeerLUID id, Buffer&& buffer, const SendParameters& params, SendCallback&& callback) noexcept;
		Result<> SendTo(const ExtenderUUID& uuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						API::Peer& peer, Buffer&& buffer, const SendParameters& params, SendCallback&& callback) noexcept;

		Result<> DisconnectFromImpl(API::Peer& peer) noexcept;

		void ProcessEvent(const Events::LocalEnvironmentChange& event) noexcept;
		void ProcessEvent(const Events::UnhandledExtenderException& event) noexcept;

		void WorkerThreadWait(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event);
		void WorkerThreadWaitInterrupt(ThreadPoolData& thpdata);
		void WorkerThreadProcessor(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event);

	private:
		std::atomic_bool m_Running{ false };
		Concurrency::Event m_ShutdownEvent;

		Settings_CThS m_Settings;
		SecurityLevel m_SecurityLevel{ SecurityLevel::One };

		LocalEnvironment_ThS m_LocalEnvironment{ m_Settings };

		ExtenderModuleMap m_ExtenderModules;

		Access::Manager m_AccessManager{ m_Settings };
		Extender::Manager m_ExtenderManager{ m_Settings };
		KeyGeneration::Manager m_KeyGenerationManager{ m_Settings };
		Peer::Manager m_PeerManager{ m_Settings, m_LocalEnvironment, m_KeyGenerationManager,
			m_AccessManager, m_ExtenderManager };
		TCP::Listener::Manager m_TCPListenerManager{ m_Settings, m_AccessManager, m_PeerManager };
		UDP::Listener::Manager m_UDPListenerManager{ m_Settings, m_AccessManager, m_PeerManager };

		std::shared_mutex m_Mutex;

		ThreadPool m_ThreadPool;
	};
}
