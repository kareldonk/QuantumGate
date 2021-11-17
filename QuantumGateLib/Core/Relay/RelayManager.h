// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "RelayLink.h"
#include "RelayEvents.h"
#include "..\..\Concurrency\SharedSpinMutex.h"
#include "..\..\Concurrency\EventGroup.h"
#include "..\..\Concurrency\DequeMap.h"

namespace QuantumGate::Implementation::Core::Relay
{
	class Manager final
	{
		using ThreadKey = UInt64;

		using EventQueueMap_ThS = Concurrency::DequeMap<RelayPort, Event>;
		using ThreadKeyToEventQueueMap = Containers::UnorderedMap<ThreadKey, std::unique_ptr<EventQueueMap_ThS>>;

		using RelayPortToThreadKeyMap = Containers::UnorderedMap<RelayPort, ThreadKey>;
		using RelayPortToThreadKeyMap_ThS = Concurrency::ThreadSafe<RelayPortToThreadKeyMap, Concurrency::SharedSpinMutex>;

		using ThreadKeyToLinkTotalMap = Containers::UnorderedMap<ThreadKey, Size>;
		using ThreadKeyToLinkTotalMap_ThS = Concurrency::ThreadSafe<ThreadKeyToLinkTotalMap, Concurrency::SharedSpinMutex>;

		using LinkMap = Containers::UnorderedMap<RelayPort, std::unique_ptr<Link_ThS>>;
		using LinkMap_ThS = Concurrency::ThreadSafe<LinkMap, std::shared_mutex>;

		struct ThreadData final
		{
			ThreadData(const ThreadKey thread_key, EventQueueMap_ThS* event_queue_map) noexcept :
				ThreadKey(thread_key), EventQueueMap(event_queue_map)
			{}

			ThreadKey ThreadKey{ 0 };
			EventQueueMap_ThS* EventQueueMap{ nullptr };
		};

		struct ThreadPoolData final
		{
			RelayPortToThreadKeyMap_ThS RelayPortToThreadKeys;
			ThreadKeyToLinkTotalMap_ThS ThreadKeyToLinkTotals;
			ThreadKeyToEventQueueMap RelayEventQueues;
			Concurrency::EventGroup WorkEvents;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData, ThreadData>;

		enum class RelayEventProcessResult
		{
			Failed, Succeeded, Retry
		};

	public:
		Manager(Peer::Manager& peers) noexcept : m_PeerManager(peers) {}
		Manager(const Manager&) = delete;
		Manager(Manager&&) noexcept = default;
		~Manager() { if (IsRunning()) Shutdown(); }
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) noexcept = default;

		Peer::Manager& GetPeerManager() const noexcept;
		Access::Manager& GetAccessManager() const noexcept;
		const Settings& GetSettings() const noexcept;

		[[nodiscard]] bool Startup() noexcept;
		void Shutdown() noexcept;
		[[nodiscard]] inline bool IsRunning() const noexcept { return m_Running; }

		std::optional<RelayPort> MakeRelayPort() const noexcept;

		[[nodiscard]] bool Connect(const PeerLUID in_peer, const PeerLUID out_peer,
								   const Endpoint& endpoint, const RelayPort rport, const RelayHop hops) noexcept;

		[[nodiscard]] bool AddRelayEvent(RelayPort rport, Event&& event) noexcept;

	private:
		void PreStartup() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] bool StartupThreadPool() noexcept;
		void BeginShutdownThreadPool() noexcept;
		void EndShutdownThreadPool() noexcept;

		[[nodiscard]] std::optional<ThreadKey> GetThreadKey(const RelayPort rport) const noexcept;

		[[nodiscard]] std::optional<ThreadKey> GetThreadKeyWithLeastLinks() const noexcept;
		[[nodiscard]] bool MapRelayPortToThreadKey(const RelayPort rport) noexcept;
		void UnMapRelayPortFromThreadKey(const RelayPort rport) noexcept;

		[[nodiscard]] bool Accept(const Events::Connect& rcevent, const PeerLUID out_peer) noexcept;

		[[nodiscard]] bool Add(const RelayPort rport, std::unique_ptr<Link_ThS>&& rl) noexcept;
		void Remove(const Containers::List<RelayPort>& rlist) noexcept;
		void DisconnectAndRemoveAll() noexcept;

		void GetUniqueLocks(PeerDetails& ipeer, Peer::Peer_ThS::UniqueLockedType& in_peer,
							PeerDetails& opeer, Peer::Peer_ThS::UniqueLockedType& out_peer) const noexcept;

		void GetUniqueLock(PeerDetails& rpeer, Peer::Peer_ThS::UniqueLockedType& peer) const noexcept;

		void DeterioratePeerReputation(const PeerLUID pluid,
									   const Access::AddressReputationUpdate rep_update =
									   Access::AddressReputationUpdate::DeteriorateMinimal) const noexcept;

		const Link_ThS* Get(const RelayPort rport) const noexcept;
		Link_ThS* Get(const RelayPort rport) noexcept;

		void PrimaryThreadWait(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);
		void PrimaryThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);

		void WorkerThreadWait(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);
		void WorkerThreadWaitInterrupt(ThreadPoolData& thpdata, ThreadData& thdata);
		void WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event);

		void ProcessEvents(EventQueueMap_ThS& queue_map, const Concurrency::Event& shutdown_event);

		bool UpdateRelayStatus(Link& rl,
							   Peer::Peer_ThS::UniqueLockedType& in_peer,
							   Peer::Peer_ThS::UniqueLockedType& out_peer,
							   const Status status, const Exception exception = Exception::Unknown) noexcept;

		bool UpdateRelayStatus(Link& rl,
							   Peer::Peer_ThS::UniqueLockedType& in_peer,
							   Peer::Peer_ThS::UniqueLockedType& out_peer,
							   const PeerLUID from_pluid, const RelayStatusUpdate status) noexcept;

		[[nodiscard]] bool OnRelayStatusUpdate(const Link& rl,
											   Peer::Peer_ThS::UniqueLockedType& in_peer,
											   Peer::Peer_ThS::UniqueLockedType& out_peer,
											   const Status prev_status) noexcept;

		[[nodiscard]] bool ProcessRelayConnect(Link& rc,
											   Peer::Peer_ThS::UniqueLockedType& in_peer,
											   Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept;

		[[nodiscard]] bool ProcessRelayConnected(Link& rc,
												 Peer::Peer_ThS::UniqueLockedType& in_peer,
												 Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept;

		[[nodiscard]] bool ProcessRelaySuspended(Link& rc,
												 Peer::Peer_ThS::UniqueLockedType& in_peer,
												 Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept;

		void ProcessRelayDisconnect(Link& rc,
									Peer::Peer_ThS::UniqueLockedType& in_peer,
									Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept;

		template<typename T>
		[[nodiscard]] bool ValidateEventOrigin(const T& event, const Link& rl) const noexcept;

		RelayEventProcessResult ProcessRelayEvent(const Events::Connect& event) noexcept;
		RelayEventProcessResult ProcessRelayEvent(const Events::StatusUpdate& event) noexcept;
		[[nodiscard]] RelayEventProcessResult ProcessRelayEvent(Events::RelayData& event) noexcept;
		RelayEventProcessResult ProcessRelayEvent(const Events::RelayDataAck& event) noexcept;

	private:
		static constexpr RelayPort DefaultQueueRelayPort{ 0 };

	private:
		std::atomic_bool m_Running{ false };
		Peer::Manager& m_PeerManager;

		LinkMap_ThS m_RelayLinks;

		ThreadPool m_ThreadPool;
	};
}