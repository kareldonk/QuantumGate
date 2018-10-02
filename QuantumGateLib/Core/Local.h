// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "ListenerManager.h"
#include "KeyGeneration\KeyGenerationManager.h"

namespace QuantumGate::Implementation::Core
{
	namespace Events
	{
		struct LocalEnvironmentChange
		{};
	}

	class Local
	{
		friend class Extender::Extender;

		using ExtenderModuleMap = std::unordered_map<Extender::ExtenderModuleID, Extender::Module>;

		using Event = std::variant<Events::LocalEnvironmentChange>;
		using EventQueue = Concurrency::Queue<Event>;
		using EventQueue_ThS = Concurrency::ThreadSafe<EventQueue, std::shared_mutex>;

		struct ThreadPoolData
		{
			EventQueue_ThS EventQueue;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData>;

	public:
		Local() noexcept;
		Local(const Local&) = delete;
		Local(Local&&) = default;
		virtual ~Local();
		Local& operator=(const Local&) = delete;
		Local& operator=(Local&&) = default;

		inline const Settings_CThS& GetSettings() const noexcept { return m_Settings; }

		Result<> Startup(const StartupParameters& params) noexcept;
		Result<> Shutdown() noexcept;
		inline const bool IsRunning() const noexcept { return (m_Running && !m_ShutdownEvent.IsSet()); }

		Result<> EnableListeners() noexcept;
		Result<> DisableListeners() noexcept;
		const bool AreListenersEnabled() const noexcept;

		Result<> EnableExtenders() noexcept;
		Result<> DisableExtenders() noexcept;
		const bool AreExtendersEnabled() const noexcept;

		Result<> EnableRelays() noexcept;
		Result<> DisableRelays() noexcept;
		const bool AreRelaysEnabled() const noexcept;

		const LocalEnvironment_ThS& GetEnvironment() noexcept;

		inline Access::Manager& GetAccessManager() noexcept { return m_AccessManager; }
		inline KeyGeneration::Manager& GetKeyGenerationManager() noexcept { return m_KeyGenerationManager; }
		inline Extender::Manager& GetExtenderManager() noexcept { return m_ExtenderManager; }

		Result<bool> AddExtender(const std::shared_ptr<QuantumGate::API::Extender>& extender) noexcept;
		Result<> RemoveExtender(const std::shared_ptr<QuantumGate::API::Extender>& extender) noexcept;

		Result<> AddExtenderModule(const Path& module_path) noexcept;
		Result<> RemoveExtenderModule(const Path& module_path) noexcept;

		const bool HasExtender(const ExtenderUUID& extuuid) const noexcept;
		std::weak_ptr<QuantumGate::API::Extender> GetExtender(const ExtenderUUID& extuuid) const noexcept;

		Result<ConnectDetails> ConnectTo(ConnectParameters&& params) noexcept;
		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params,
													ConnectCallback&& function) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept;

		std::tuple<UInt, UInt, UInt, UInt> GetVersion() const noexcept;
		String GetVersionString() const noexcept;
		std::pair<UInt, UInt> GetProtocolVersion() const noexcept;
		String GetProtocolVersionString() const noexcept;

		Result<PeerUUID> GetUUID() const noexcept;
		Result<PeerDetails> GetPeerDetails(const PeerLUID pluid) const noexcept;
		Result<Vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;

		Result<> SetSecurityLevel(const SecurityLevel level,
								  const std::optional<SecurityParameters>& params = std::nullopt,
								  const bool silent = false) noexcept;
		[[nodiscard]] const SecurityLevel GetSecurityLevel() const noexcept;
		[[nodiscard]] SecurityParameters GetSecurityParameters() const noexcept;
		void SetDefaultSecuritySettings(Settings& settings) noexcept;

	private:
		[[nodiscard]] const bool StartupThreadPool() noexcept;
		void ShutdownThreadPool() noexcept;

		void LocalEnvironmentChangedCallback() noexcept;

		Result<bool> AddExtenderImpl(const std::shared_ptr<QuantumGate::API::Extender>& extender,
									 const Extender::ExtenderModuleID moduleid = 0) noexcept;
		Result<> RemoveExtenderImpl(const std::shared_ptr<QuantumGate::API::Extender>& extender,
									const Extender::ExtenderModuleID moduleid = 0) noexcept;

		const bool ValidateInitParameters(const StartupParameters& params) const noexcept;
		const bool ValidateSupportedAlgorithms(const Algorithms& algorithms) const noexcept;
		const bool ValidateSecurityParameters(const SecurityParameters& params) const noexcept;

		Result<> SendTo(const ExtenderUUID& uuid, const std::atomic_bool& running,
						const PeerLUID id, Buffer&& buffer, const bool compress);

		void ProcessEvent(const Events::LocalEnvironmentChange& event) noexcept;

		const std::pair<bool, bool> WorkerThreadProcessor(ThreadPoolData& thpdata,
														  const Concurrency::EventCondition& shutdown_event);

	private:
		std::atomic_bool m_Running{ false };
		Concurrency::EventCondition m_ShutdownEvent{ false };

		Settings_CThS m_Settings;
		SecurityLevel m_SecurityLevel{ SecurityLevel::One };

		LocalEnvironment_ThS m_LocalEnvironment;

		ExtenderModuleMap m_ExtenderModules;

		Access::Manager m_AccessManager{ m_Settings };
		Extender::Manager m_ExtenderManager{ m_Settings };
		KeyGeneration::Manager m_KeyGenerationManager{ m_Settings };
		Peer::Manager m_PeerManager{ m_Settings, m_LocalEnvironment, m_KeyGenerationManager,
			m_AccessManager, m_ExtenderManager };
		Listener::Manager m_ListenerManager{ m_Settings, m_AccessManager, m_PeerManager };

		std::shared_mutex m_Mutex;

		ThreadPool m_ThreadPool;
	};
}
