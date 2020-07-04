// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "RelayLink.h"
#include "RelayEvents.h"
#include "..\..\Concurrency\SharedSpinMutex.h"
#include "..\..\Concurrency\EventGroup.h"

namespace QuantumGate::Implementation::Core::Relay
{
	class Manager final
	{
		using ThreadKey = UInt64;

		using EventQueue = Concurrency::Queue<Event>;
		using EventQueue_ThS = Concurrency::ThreadSafe<EventQueue, std::shared_mutex>;
		using EventQueueMap = Containers::UnorderedMap<ThreadKey, std::unique_ptr<EventQueue_ThS>>;

		using RelayPortToThreadKeyMap = Containers::UnorderedMap<RelayPort, ThreadKey>;
		using RelayPortToThreadKeyMap_ThS = Concurrency::ThreadSafe<RelayPortToThreadKeyMap, Concurrency::SharedSpinMutex>;

		using ThreadKeyToLinkTotalMap = Containers::UnorderedMap<ThreadKey, Size>;
		using ThreadKeyToLinkTotalMap_ThS = Concurrency::ThreadSafe<ThreadKeyToLinkTotalMap, Concurrency::SharedSpinMutex>;

		using LinkMap = Containers::UnorderedMap<RelayPort, std::unique_ptr<Link_ThS>>;
		using LinkMap_ThS = Concurrency::ThreadSafe<LinkMap, std::shared_mutex>;

		struct ThreadData final
		{
			ThreadData(const ThreadKey thread_key) noexcept : ThreadKey(thread_key) {}

			ThreadKey ThreadKey{ 0 };
		};

		struct ThreadPoolData final
		{
			RelayPortToThreadKeyMap_ThS RelayPortToThreadKeys;
			ThreadKeyToLinkTotalMap_ThS ThreadKeyToLinkTotals;
			EventQueueMap RelayEventQueues;
			Concurrency::EventGroup WorkEvents;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData, ThreadData>;

		enum RelayDataProcessResult
		{
			Failed, Succeeded, Retry
		};

	public:
		Manager(Peer::Manager& peers) noexcept : m_Peers(peers) {}
		Manager(const Manager&) = delete;
		Manager(Manager&&) noexcept = default;
		~Manager() { if (IsRunning()) Shutdown(); }
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) noexcept = default;

		Peer::Manager& GetPeers() const noexcept;
		Access::Manager& GetAccessManager() const noexcept;
		const Settings& GetSettings() const noexcept;

		[[nodiscard]] bool Startup() noexcept;
		void Shutdown() noexcept;
		[[nodiscard]] inline bool IsRunning() const noexcept { return m_Running; }

		std::optional<RelayPort> MakeRelayPort() const noexcept;

		[[nodiscard]] bool Connect(const PeerLUID in_peer, const PeerLUID out_peer,
								   const IPEndpoint& endpoint, const RelayPort rport, const RelayHop hops) noexcept;

		[[nodiscard]] bool AddRelayEvent(const RelayPort rport, Event&& event) noexcept;

	private:
		void PreStartup() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] bool StartupThreadPool() noexcept;
		void ShutdownThreadPool() noexcept;

		std::optional<ThreadKey> GetThreadKey(const RelayPort rport) const noexcept;

		std::optional<ThreadKey> GetThreadKeyWithLeastLinks() const noexcept;
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
									   const Access::IPReputationUpdate rep_update =
									   Access::IPReputationUpdate::DeteriorateMinimal) const noexcept;

		const Link_ThS* Get(const RelayPort rport) const noexcept;
		Link_ThS* Get(const RelayPort rport) noexcept;

		bool PrimaryThreadWaitProcessor(ThreadPoolData& thpdata, ThreadData& thdata, std::chrono::milliseconds max_wait,
										const Concurrency::Event& shutdown_event);

		ThreadPool::ThreadCallbackResult PrimaryThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata,
																const Concurrency::Event& shutdown_event);

		ThreadPool::ThreadCallbackResult WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata,
															   const Concurrency::Event& shutdown_event);

		[[nodiscard]] bool ProcessRelayConnect(Link& rc,
											   Peer::Peer_ThS::UniqueLockedType& in_peer,
											   Peer::Peer_ThS::UniqueLockedType& out_peer);

		[[nodiscard]] std::pair<bool, bool> ProcessRelayConnected(Link& rc,
																  Peer::Peer_ThS::UniqueLockedType& in_peer,
																  Peer::Peer_ThS::UniqueLockedType& out_peer);

		void ProcessRelayDisconnect(Link& rc,
									Peer::Peer_ThS::UniqueLockedType& in_peer,
									Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept;

		template<typename T>
		[[nodiscard]] bool ValidateEventOrigin(const T& event, const Link& rl) const noexcept;

		bool ProcessRelayEvent(const Events::Connect& event) noexcept;
		bool ProcessRelayEvent(const Events::StatusUpdate& event) noexcept;
		[[nodiscard]] RelayDataProcessResult ProcessRelayEvent(Events::RelayData& event) noexcept;
		bool ProcessRelayEvent(const Events::RelayDataAck& event) noexcept;

	private:
		std::atomic_bool m_Running{ false };
		Peer::Manager& m_Peers;

		LinkMap_ThS m_RelayLinks;

		ThreadPool m_ThreadPool;
	};
}