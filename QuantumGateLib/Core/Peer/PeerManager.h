// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\API\Peer.h"
#include "..\LocalEnvironment.h"
#include "..\..\Settings.h"
#include "..\..\Concurrency\Queue.h"
#include "..\..\Concurrency\ThreadPool.h"
#include "..\..\Concurrency\EventGroup.h"
#include "..\KeyGeneration\KeyGenerationManager.h"
#include "..\Relay\RelayManager.h"
#include "..\UDP\UDPConnectionManager.h"
#include "PeerLookupMaps.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Manager final
	{
		friend class Peer;
		friend class Relay::Manager;

		using PeerMap = Containers::UnorderedMap<PeerLUID, PeerSharedPointer>;
		using PeerMap_ThS = Concurrency::ThreadSafe<PeerMap, std::shared_mutex>;

		struct Tasks final
		{
			struct PeerAccessCheck final {};
			struct PeerCallback final { Callback<void()> Callback; };
		};

		using ThreadPoolTask = std::variant<Tasks::PeerAccessCheck, Tasks::PeerCallback>;
		using ThreadPoolTaskQueue_ThS = Concurrency::Queue<ThreadPoolTask>;
		
		enum class BroadcastResult { Succeeded, PeerNotReady, SendFailure };

		using BroadcastCallback = Callback<void(Peer& peer, const BroadcastResult result)>;

		struct ThreadPoolData final
		{
		public:
			PeerMap_ThS PeerMap;
			ThreadPoolTaskQueue_ThS TaskQueue;

		private:
			Concurrency::EventGroup WorkEvents;

		public:
			[[nodiscard]] inline bool InitializeWorkEvents() noexcept { return WorkEvents.Initialize(); }
			inline void DeinitializeWorkEvents() noexcept { WorkEvents.Deinitialize(); }
			inline void ClearWorkEvents() noexcept { WorkEvents.RemoveAllEvents(); }
			inline auto WaitForWorkEvent(const std::chrono::milliseconds time) noexcept { return WorkEvents.Wait(time); }
			[[nodiscard]] bool AddWorkEvent(const Peer& peer) noexcept;
			void RemoveWorkEvent(const Peer& peer) noexcept;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData>;
		using ThreadPoolMap = Containers::UnorderedMap<UInt64, std::unique_ptr<ThreadPool>>;

	public:
		Manager() = delete;
		Manager(const Settings_CThS& settings, LocalEnvironment_ThS& environment, UDP::Connection::Manager& udpmgr,
				KeyGeneration::Manager& keymgr, Access::Manager& accessmgr,
				Extender::Manager& extenders) noexcept;
		Manager(const Manager&) = delete;
		Manager(Manager&&) noexcept = default;
		~Manager() { if (IsRunning()) Shutdown(); }
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) noexcept = default;

		const Settings& GetSettings() const noexcept;

		bool Startup() noexcept;
		void Shutdown() noexcept;
		[[nodiscard]] inline bool IsRunning() const noexcept { return m_Running; }

		bool StartupRelays() noexcept;
		void ShutdownRelays() noexcept;
		[[nodiscard]] inline bool AreRelaysRunning() const noexcept { return m_RelayManager.IsRunning(); }

		PeerSharedPointer Get(const PeerLUID pluid) const noexcept;
		Result<API::Peer> GetPeer(const PeerLUID pluid) const noexcept;

		Result<> QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept;

		PeerSharedPointer Create(const IP::AddressFamily af, const IP::Protocol protocol, const PeerConnectionType pctype,
								 std::optional<ProtectedBuffer>&& shared_secret) noexcept;
		PeerSharedPointer CreateRelay(const PeerConnectionType pctype,
									  std::optional<ProtectedBuffer>&& shared_secret) noexcept;

		bool Accept(PeerSharedPointer& peerths) noexcept;

		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params, ConnectCallback&& function) noexcept;

		Result<> DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept;
		Result<> DisconnectFrom(API::Peer& peer, DisconnectCallback&& function) noexcept;

		Result<Size> Send(const ExtenderUUID& extuuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						  const PeerLUID pluid, const BufferView& buffer, const SendParameters& params, SendCallback&& callback) noexcept;
		Result<Size> Send(const ExtenderUUID& extuuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						  API::Peer& api_peer, const BufferView& buffer, const SendParameters& params, SendCallback&& callback) noexcept;

		Result<> SendTo(const ExtenderUUID& extuuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						const PeerLUID pluid, Buffer&& buffer, const SendParameters& params, SendCallback&& callback) noexcept;
		Result<> SendTo(const ExtenderUUID& extuuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						API::Peer& api_peer, Buffer&& buffer, const SendParameters& params, SendCallback&& callback) noexcept;

		Result<> Broadcast(const MessageType msgtype, const Buffer& buffer, BroadcastCallback&& callback);

		const Vector<BinaryIPAddress>* GetLocalIPAddresses() const noexcept;

	private:
		void PreStartupThreadPools() noexcept;
		void ResetState() noexcept;

		bool StartupThreadPools() noexcept;
		void ShutdownThreadPools() noexcept;
		bool AddCallbacks() noexcept;
		void RemoveCallbacks() noexcept;

		inline Access::Manager& GetAccessManager() const noexcept { return m_AccessManager; }
		inline KeyGeneration::Manager& GetKeyGenerationManager() const noexcept { return m_KeyGenerationManager; }
		inline Extender::Manager& GetExtenderManager() const noexcept { return m_ExtenderManager; }

		inline PeerSharedPointer& GetPeerFromPeerStorage(API::Peer& peer) noexcept
		{
			return *static_cast<PeerSharedPointer*>(peer.GetPeerSharedPtrStorage());
		}

		inline Relay::Manager& GetRelayManager() noexcept { return m_RelayManager; }

		Result<std::pair<PeerLUID, bool>> GetRelayPeer(const ConnectParameters& params, String& error_details) noexcept;

		Result<PeerLUID> GetRelayPeer(const Vector<BinaryIPAddress>& excl_addr1,
									  const Vector<BinaryIPAddress>& excl_addr2) const noexcept;

		Result<bool> AreRelayIPsInSameNetwork(const BinaryIPAddress& ip1, const BinaryIPAddress& ip2) const noexcept;
		Result<bool> AreRelayIPsInSameNetwork(const BinaryIPAddress& ip,
											  const Vector<BinaryIPAddress>& addresses) noexcept;

		bool Add(PeerSharedPointer& peerths) noexcept;
		void Remove(const PeerSharedPointer& peer_ths) noexcept;
		void Remove(const Containers::List<PeerSharedPointer>& peerlist) noexcept;
		void RemoveAll() noexcept;

		Result<PeerLUID> DirectConnectTo(ConnectParameters&& params, ConnectCallback&& function) noexcept;

		Result<std::pair<PeerLUID, bool>> RelayConnectTo(ConnectParameters&& params,
														 ConnectCallback&& function) noexcept;

		Result<> DisconnectFrom(Peer_ThS& peerths, DisconnectCallback&& function) noexcept;
		void Disconnect(Peer& peer, const bool graceful) noexcept;
		void DisconnectAndRemoveAll() noexcept;

		Result<Size> Send(const ExtenderUUID& extuuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						  Peer& peer, const BufferView& buffer, const SendParameters& params, SendCallback&& callback) noexcept;

		Result<> SendTo(const ExtenderUUID& extuuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						Peer& peer, Buffer&& buffer, const SendParameters& params, SendCallback&& callback) noexcept;

		bool BroadcastExtenderUpdate();

		void OnAccessUpdate() noexcept;
		void OnLocalExtenderUpdate(const Vector<ExtenderUUID>& extuuids, const bool added);
		void OnPeerEvent(const Peer& peer, const Event&& event) noexcept;

		void SchedulePeerCallback(const UInt64 threadpool_key, Callback<void()>&& callback) noexcept;

		void AddReportedPublicIPEndpoint(const IPEndpoint& pub_endpoint, const IPEndpoint& rep_peer,
										 const PeerConnectionType rep_con_type, const bool trusted) noexcept;

		Result<Buffer> GetExtenderUpdateData() const noexcept;

		void PrimaryThreadWait(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event);
		void PrimaryThreadProcessor(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event);

		void WorkerThreadWait(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event);
		void WorkerThreadWaitInterrupt(ThreadPoolData& thpdata);
		void WorkerThreadProcessor(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event);

	private:
		std::atomic_bool m_Running{ false };
		const Settings_CThS& m_Settings;
		LocalEnvironment_ThS& m_LocalEnvironment;
		UDP::Connection::Manager& m_UDPConnectionManager;
		KeyGeneration::Manager& m_KeyGenerationManager;
		Access::Manager& m_AccessManager;
		Extender::Manager& m_ExtenderManager;

		LookupMaps_ThS m_LookupMaps;
		PeerMap_ThS m_AllPeers;
		ThreadPoolMap m_ThreadPools;

		Relay::Manager m_RelayManager{ *this };

		Access::Manager::AccessUpdateCallbackHandle m_AccessUpdateCallbackHandle;
		Extender::Manager::ExtenderUpdateCallbackHandle m_ExtenderUpdateCallbackHandle;
	};
}