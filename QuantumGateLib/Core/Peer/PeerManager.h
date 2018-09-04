// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\LocalEnvironment.h"
#include "..\..\Settings.h"
#include "..\..\Concurrency\Queue.h"
#include "..\..\Concurrency\ThreadPool.h"
#include "..\KeyGeneration\KeyGenerationManager.h"
#include "..\Relay\RelayManager.h"
#include "PeerLookupMaps.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Manager
	{
		friend class Peer;
		friend class Relay::Manager;

		using PeerMap = std::unordered_map<PeerLUID, std::shared_ptr<Peer_ThS>>;
		using PeerMap_ThS = Concurrency::ThreadSafe<PeerMap, Concurrency::RecursiveSharedMutex>;
		using PeerQueue = Concurrency::Queue<std::shared_ptr<Peer_ThS>>;
		using PeerQueue_ThS = Concurrency::ThreadSafe<PeerQueue, Concurrency::SpinMutex>;

		struct PeerCollection
		{
			PeerMap_ThS Map;
			std::atomic<Size> Count{ 0 };
			std::atomic<UInt> AccessUpdateFlag;
		};

		struct ThreadData
		{
			ThreadData(const bool primary) noexcept : IsPrimary(primary) {}

			const bool IsPrimary{ false };
		};

		struct ThreadPoolData
		{
			ThreadPoolData(Manager& mgr) noexcept : PeerManager(mgr) {}

			Manager& PeerManager;
			PeerCollection PeerCollection;
			PeerQueue_ThS Queue;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData, ThreadData>;
		using ThreadPoolMap = std::unordered_map<UInt64, std::unique_ptr<ThreadPool>>;

	public:
		Manager() = delete;
		Manager(const Settings_CThS& settings, const LocalEnvironment& environment,
				KeyGeneration::Manager& keymgr, Access::Manager& accessmgr,
				Extender::Manager& extenders) noexcept;
		Manager(const Manager&) = delete;
		Manager(Manager&&) = default;
		virtual ~Manager() = default;
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = default;

		const Settings& GetSettings() const noexcept;

		const bool Startup() noexcept;
		void Shutdown() noexcept;

		const bool StartupRelays() noexcept;
		const void ShutdownRelays() noexcept;
		const bool AreRelaysRunning() const noexcept { return m_RelayManager.IsRunning(); }

		std::shared_ptr<Peer_ThS> Get(const PeerLUID pluid) const noexcept;

		Result<std::vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;
		Result<PeerDetails> GetPeerDetails(const PeerLUID pluid) const noexcept;

		std::shared_ptr<Peer_ThS> Create(const PeerConnectionType pctype,
										 std::optional<ProtectedBuffer>&& shared_secret) noexcept;
		std::shared_ptr<Peer_ThS> CreateRelay(const PeerConnectionType pctype,
											  std::optional<ProtectedBuffer>&& shared_secret) noexcept;

		const bool Accept(std::shared_ptr<Peer_ThS>& peerths) noexcept;

		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params,
													ConnectCallback&& function) noexcept;

		Result<> DisconnectFrom(const PeerLUID pluid,
								DisconnectCallback&& function) noexcept;

		Result<> SendTo(const ExtenderUUID& extuuid, const std::atomic_bool& running,
						const PeerLUID pluid, Buffer&& buffer, const bool compress);

		Result<> Broadcast(const MessageType msgtype, Buffer&& buffer);

		const std::vector<IPAddress>& GetLocalIPAddresses() const noexcept;

	private:
		void PreStartupThreadPools() noexcept;
		void ResetState() noexcept;

		const bool StartupThreadPools() noexcept;
		void ShutdownThreadPools() noexcept;
		const bool AddCallbacks() noexcept;
		void RemoveCallbacks() noexcept;

		inline Access::Manager& GetAccessManager() const noexcept { return m_AccessManager; }
		inline KeyGeneration::Manager& GetKeyGenerationManager() const noexcept { return m_KeyGenerationManager; }
		inline Extender::Manager& GetExtenderManager() const noexcept { return m_ExtenderManager; }

		std::shared_ptr<Peer_ThS> Create(const IPAddressFamily af, const Int32 type,
										 const Int32 protocol, const PeerConnectionType pctype,
										 std::optional<ProtectedBuffer>&& shared_secret) noexcept;

		inline Relay::Manager& GetRelayManager() noexcept { return m_RelayManager; }
		Result<PeerLUID> GetRelayPeer(const std::vector<BinaryIPAddress>& excl_addr) const noexcept;
		Result<bool> AreRelayIPsInSameNetwork(const BinaryIPAddress& ip1, const BinaryIPAddress& ip2) const noexcept;
		Result<bool> AreRelayIPsInSameNetwork(const BinaryIPAddress& ip,
											  const std::vector<BinaryIPAddress>& addresses) noexcept;

		const bool Add(std::shared_ptr<Peer_ThS>& peerths) noexcept;
		void Remove(const Peer& peer) noexcept;
		void Remove(const std::list<std::shared_ptr<Peer_ThS>>& peerlist) noexcept;
		void RemoveAll() noexcept;

		const bool DirectConnectTo(ConnectParameters&& params,
								   ConnectCallback&& function) noexcept;

		Result<std::pair<PeerLUID, bool>> RelayConnectTo(ConnectParameters&& params,
														 ConnectCallback&& function) noexcept;

		void Disconnect(Peer& peer, const bool graceful) noexcept;
		void DisconnectAndRemoveAll() noexcept;

		const bool BroadcastExtenderUpdate();

		void OnAccessUpdate() noexcept;
		void OnLocalExtenderUpdate(const std::vector<ExtenderUUID>& extuuids, const bool added);
		void OnUnhandledExtenderMessage(const ExtenderUUID& extuuid, const PeerLUID pluid,
										const std::pair<bool, bool>& result) noexcept;
		void OnPeerEvent(const Peer& peer, const Event&& event) noexcept;

		static const std::pair<bool, bool> PrimaryThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata,
																  const Concurrency::EventCondition& shutdown_event);
		static const std::pair<bool, bool> WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata,
																 const Concurrency::EventCondition& shutdown_event);

	private:
		std::atomic_bool m_Running{ false };
		const Settings_CThS& m_Settings;
		const LocalEnvironment& m_LocalEnvironment;
		KeyGeneration::Manager& m_KeyGenerationManager;
		Access::Manager& m_AccessManager;
		Extender::Manager& m_ExtenderManager;

		LookupMaps_ThS m_LookupMaps;
		PeerCollection m_AllPeers;
		ThreadPoolMap m_ThreadPools;

		Relay::Manager m_RelayManager{ *this };

		Access::Manager::AccessUpdateCallbackHandle m_AccessUpdateCallbackHandle;
		Extender::Manager::ExtenderUpdateCallbackHandle m_ExtenderUpdateCallbackHandle;
		Extender::Manager::UnhandledExtenderMessageCallbackHandle m_UnhandledExtenderMessageCallbackHandle;
	};
}