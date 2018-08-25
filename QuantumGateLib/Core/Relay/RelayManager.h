// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "RelayLink.h"
#include "RelayEvents.h"
#include "..\..\Concurrency\SharedSpinMutex.h"

namespace QuantumGate::Implementation::Core::Relay
{
	using Event = std::variant<Events::Connect, Events::StatusUpdate, Events::RelayData>;

	class Manager
	{
		using ThreadKey = UInt;

		using EventQueue = Concurrency::Queue<Event>;
		using EventQueue_ThS = Concurrency::ThreadSafe<EventQueue, std::shared_mutex>;
		using EventQueueMap = std::unordered_map<ThreadKey, std::unique_ptr<EventQueue_ThS>>;

		using RelayPortToThreadKeyMap = std::unordered_map<RelayPort, ThreadKey>;
		using RelayPortToThreadKeyMap_ThS = Concurrency::ThreadSafe<RelayPortToThreadKeyMap, Concurrency::SharedSpinMutex>;

		using ThreadKeyToLinkTotalMap = std::unordered_map<ThreadKey, Size>;
		using ThreadKeyToLinkTotalMap_ThS = Concurrency::ThreadSafe<ThreadKeyToLinkTotalMap, Concurrency::SharedSpinMutex>;

		using LinkMap = std::unordered_map<RelayPort, std::unique_ptr<Link_ThS>>;
		using LinkMap_ThS = Concurrency::ThreadSafe<LinkMap, std::shared_mutex>;

		struct ThreadData
		{
			ThreadData(const UInt thread_key) noexcept : ThreadKey(thread_key) {}

			UInt ThreadKey{ 0 };
		};

		struct ThreadPoolData
		{
			ThreadPoolData(Manager& mgr) noexcept : RelayManager(mgr) {}

			Manager& RelayManager;
			RelayPortToThreadKeyMap_ThS RelayPortToThreadKeys;
			ThreadKeyToLinkTotalMap_ThS ThreadKeyToLinkTotals;
			EventQueueMap RelayEventQueues;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData, ThreadData>;

	public:
		Manager(Peer::Manager& peers) noexcept : m_Peers(peers) {}
		Manager(const Manager&) = delete;
		Manager(Manager&&) = default;
		~Manager() = default;
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = default;

		Peer::Manager& GetPeers() const noexcept;
		Access::Manager& GetAccessManager() const noexcept;
		const Settings& GetSettings() const noexcept;

		[[nodiscard]] const bool Startup() noexcept;
		void Shutdown() noexcept;

		inline const bool IsRunning() const noexcept { return m_Running; }

		std::optional<RelayPort> MakeRelayPort() const noexcept;

		[[nodiscard]] const bool Connect(const PeerLUID in_peer, const PeerLUID out_peer,
										 const IPEndpoint& endpoint, const RelayPort rport, const RelayHop hops) noexcept;

		[[nodiscard]] const bool AddRelayEvent(const RelayPort rport, Event&& event) noexcept;

	private:
		void PreStartup() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] const bool StartupThreadPool() noexcept;
		void ShutdownThreadPool() noexcept;

		const std::optional<ThreadKey> GetThreadKey(const RelayPort rport) const noexcept;

		const std::optional<ThreadKey> GetThreadKeyWithLeastLinks() const noexcept;
		[[nodiscard]] const bool MapRelayPortToThreadKey(const RelayPort rport) noexcept;
		void UnMapRelayPortFromThreadKey(const RelayPort rport) noexcept;

		[[nodiscard]] const bool Accept(const Events::Connect& rcevent, const PeerLUID out_peer) noexcept;

		[[nodiscard]] const bool Add(const RelayPort rport, std::unique_ptr<Link_ThS>&& rl) noexcept;
		void Remove(const std::list<RelayPort>& rlist) noexcept;
		void DisconnectAndRemoveAll() noexcept;

		void GetUniqueLocks(PeerDetails& ipeer, Peer::Peer_ThS::UniqueLockedType& in_peer,
							PeerDetails& opeer, Peer::Peer_ThS::UniqueLockedType& out_peer) const noexcept;

		void GetUniqueLock(PeerDetails& rpeer, Peer::Peer_ThS::UniqueLockedType& peer) const noexcept;

		void DeterioratePeerReputation(const PeerLUID pluid,
									   const Access::IPReputationUpdate rep_update =
									   Access::IPReputationUpdate::DeteriorateMinimal) const noexcept;

		const Link_ThS* Get(const RelayPort rport) const noexcept;
		Link_ThS* Get(const RelayPort rport) noexcept;

		static const std::pair<bool, bool> PrimaryThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata,
																  const Concurrency::EventCondition& shutdown_event);

		static const std::pair<bool, bool> WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata,
																 const Concurrency::EventCondition& shutdown_event);

		const bool ProcessRelayConnect(Link& rc,
									   Peer::Peer_ThS::UniqueLockedType& in_peer,
									   Peer::Peer_ThS::UniqueLockedType& out_peer);

		void ProcessRelayDisconnect(Link& rc,
									Peer::Peer_ThS::UniqueLockedType& in_peer,
									Peer::Peer_ThS::UniqueLockedType& out_peer) noexcept;

		template<typename T>
		[[nodiscard]] const bool ValidateEventOrigin(const T& event, const Link& rl) const noexcept;

		const bool ProcessRelayEvent(const Events::Connect& event) noexcept;
		const bool ProcessRelayEvent(const Events::StatusUpdate& event) noexcept;
		const bool ProcessRelayEvent(Events::RelayData& event) noexcept;

	private:
		std::atomic_bool m_Running{ false };
		Peer::Manager& m_Peers;

		LinkMap_ThS m_RelayLinks;

		ThreadPool m_ThreadPool{ *this };
	};
}