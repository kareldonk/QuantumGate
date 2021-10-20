// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPSocket.h"
#include "UDPConnectionSendQueue.h"
#include "..\..\Memory\StackBuffer.h"
#include "..\..\Common\Containers.h"
#include "..\Access\AccessManager.h"

// Use to enable/disable debug console output
// #define UDPCON_DEBUG

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Connection final
	{
		friend class SendQueue;
		friend class MTUDiscovery;

		using ReceiveBuffer = Memory::StackBuffer<UDPMessageSizes::Max>;

		using ReceiveQueue = Containers::Map<Message::SequenceNumber, Message>;

		enum class ReceiveWindow { Unknown, Current, Previous };

		struct DelayedSendItem final
		{
			Message::Type MessageType{ Message::Type::Unknown };
			std::optional<Message::SequenceNumber> SequenceNumber;
			std::shared_ptr<Listener::SendQueue_ThS> ListenerSendQueue;
			std::optional<IPEndpoint> PeerEndpoint;
			SteadyTime ScheduleSteadyTime;
			std::chrono::milliseconds ScheduleMilliseconds{ 0 };
			Buffer Data;

			inline bool IsTime(const SteadyTime now) const noexcept
			{
				if ((now - ScheduleSteadyTime) >= ScheduleMilliseconds) return true;

				return false;
			}

			inline static bool Compare(const DelayedSendItem& item1, const DelayedSendItem& item2) noexcept
			{
				return (item1.ScheduleSteadyTime + item1.ScheduleMilliseconds) > (item2.ScheduleSteadyTime + item2.ScheduleMilliseconds);
			}
		};

		using DelayedSendItemQueue = Containers::PriorityQueue<DelayedSendItem, Vector<DelayedSendItem>, decltype(&DelayedSendItem::Compare)>;

		class LastSequenceNumber final
		{
		public:
			LastSequenceNumber(const Message::SequenceNumber number) noexcept :
				m_SequenceNumber(number)
			{}

			inline operator Message::SequenceNumber() const noexcept { return m_SequenceNumber; }

			inline LastSequenceNumber& operator=(const Message::SequenceNumber number) noexcept
			{
				m_SequenceNumber = number;
				m_Acked = false;
				return *this;
			}

			[[nodiscard]] inline bool IsAcked() const noexcept { return m_Acked; }
			inline void SetAcked() noexcept { m_Acked = true; }
			inline void ResetAcked() noexcept { m_Acked = false; }

		private:
			Message::SequenceNumber m_SequenceNumber{ 0 };
			bool m_Acked{ false };
		};

	public:
		using SymmetricKeysCollection = std::array<SymmetricKeys, 2>;

		class HandshakeTracker final
		{
		public:
			HandshakeTracker(std::atomic_int64_t& nhip) noexcept :
				m_NumHandshakesInProgress(nhip)
			{
				++m_NumHandshakesInProgress;
				m_Active = true;
			}

			HandshakeTracker(const HandshakeTracker&) = delete;
			HandshakeTracker(HandshakeTracker&&) noexcept = delete;

			~HandshakeTracker()
			{
				Deactivate();
			}

			HandshakeTracker& operator=(const HandshakeTracker&) = delete;
			HandshakeTracker& operator=(HandshakeTracker&&) noexcept = delete;

			void Deactivate() noexcept
			{
				if (m_Active)
				{			
					auto num = m_NumHandshakesInProgress.load();

					while (true)
					{
						auto new_num = num;

						assert(new_num > 0);
						if (new_num > 0) --new_num;
						else new_num = 0;

						if (m_NumHandshakesInProgress.compare_exchange_strong(num, new_num))
						{
							break;
						}
					}
					
					m_Active = false;
				}
			}

		private:
			std::atomic_int64_t& m_NumHandshakesInProgress;
			bool m_Active{ false };
		};

		Connection(const Settings_CThS& settings, KeyGeneration::Manager& keymgr, Access::Manager& accessmgr,
				   const PeerConnectionType type, const ConnectionID id, const Message::SequenceNumber seqnum,
				   ProtectedBuffer&& handshake_data, std::optional<ProtectedBuffer>&& shared_secret,
				   std::unique_ptr<Connection::HandshakeTracker>&& handshake_tracker);
		Connection(const Connection&) = delete;
		Connection(Connection&&) noexcept = delete;
		~Connection();
		Connection& operator=(const Connection&) = delete;
		Connection& operator=(Connection&&) noexcept = delete;

		[[nodiscard]] inline PeerConnectionType GetType() const noexcept { return m_Type; }
		[[nodiscard]] inline Status GetStatus() const noexcept { return m_Status; }
		[[nodiscard]] inline ConnectionID GetID() const noexcept { return m_ID; }
		[[nodiscard]] inline const SymmetricKeys& GetSymmetricKeys() const noexcept { return m_SymmetricKeys[0]; }
		[[nodiscard]] inline const IPEndpoint& GetPeerEndpoint() const noexcept { return m_PeerEndpoint;  }

		[[nodiscard]] bool Open(const Network::IP::AddressFamily af,
								const bool nat_traversal, UDP::Socket& socket) noexcept;
		void Close() noexcept;

		Concurrency::Event& GetReadEvent() noexcept { return m_Socket.GetEvent(); }

		void ProcessEvents(const SteadyTime current_steadytime) noexcept;
		[[nodiscard]] inline bool ShouldClose() const noexcept { return (m_CloseCondition != CloseCondition::None); }

		void OnLocalIPInterfaceChanged() noexcept;

		static std::optional<ConnectionID> MakeConnectionID() noexcept;

	private:
		[[nodiscard]] const Settings& GetSettings() const noexcept { return m_Settings.GetCache(true); }

		[[nodiscard]] bool SetStatus(const Status status) noexcept;
		[[nodiscard]] bool OnStatusChange(const Status old_status, const Status new_status) noexcept;

		[[nodiscard]] const ProtectedBuffer& GetGlobalSharedSecret() const noexcept;
		[[nodiscard]] bool InitializeKeyExchange(KeyGeneration::Manager& keymgr, ProtectedBuffer&& handshake_data) noexcept;
		[[nodiscard]] bool FinalizeKeyExchange() noexcept;

		[[nodiscard]] bool Suspend() noexcept;
		[[nodiscard]] bool Resume() noexcept;

		[[nodiscard]] inline CloseCondition GetCloseCondition() const noexcept { return m_CloseCondition; }
		void SetCloseCondition(const CloseCondition cc, int socket_error_code = -1) noexcept;
		void SetSocketException(const int error_code) noexcept;
		
		inline Result<bool> SetMTUDiscovery(const bool enabled) noexcept { return m_Socket.SetMTUDiscovery(enabled); }
		void ResetMTU() noexcept;
		[[nodiscard]] bool OnMTUUpdate(const Size mtu) noexcept;

		[[nodiscard]] bool IsEndpointAllowed(const IPEndpoint& endpoint) noexcept;
		void CheckEndpointChange(const IPEndpoint& endpoint) noexcept;

		[[nodiscard]] bool SendOutboundSyn(std::optional<Message::CookieData>&& cookie = std::nullopt) noexcept;
		[[nodiscard]] bool SendInboundSyn() noexcept;
		[[nodiscard]] bool SendData(Buffer&& data) noexcept;
		[[nodiscard]] bool SendStateUpdate() noexcept;
		[[nodiscard]] bool SendPendingAcks() noexcept;
		[[nodiscard]] bool SendKeepAlive() noexcept;
		[[nodiscard]] bool SendDelayedItems(const SteadyTime current_steadytime) noexcept;
		void SendImmediateReset() noexcept;
		void SendDecoyMessages(const Size max_num, const std::chrono::milliseconds max_interval) noexcept;
		
		[[nodiscard]] bool Send(Message&& msg, const std::chrono::milliseconds delay = std::chrono::milliseconds{ 0 },
								const bool save_endpoint = false) noexcept;
		[[nodiscard]] bool Send(const SteadyTime current_steadytime, const Message::Type msgtype,
								const std::optional<Message::SequenceNumber>& msgseqnum, Buffer&& msgdata,
								std::shared_ptr<Listener::SendQueue_ThS>&& listener_send_queue,
								std::optional<IPEndpoint>&& peer_endpoint) noexcept;
		[[nodiscard]] Result<Size> Send(const SteadyTime current_steadytime, const Buffer& msgdata,
										const std::shared_ptr<Listener::SendQueue_ThS>& listener_send_queue,
										const std::optional<IPEndpoint>& peer_endpoint) noexcept;

		[[nodiscard]] ReceiveBuffer& GetReceiveBuffer() const noexcept;
		[[nodiscard]] bool ReceiveToQueue(const SteadyTime current_steadytime) noexcept;
		[[nodiscard]] bool ProcessReceivedData(const SteadyTime current_steadytime, const IPEndpoint& endpoint, BufferSpan& buffer) noexcept;
		[[nodiscard]] bool ProcessReceivedMessageHandshake(const IPEndpoint& endpoint, Message&& msg) noexcept;
		[[nodiscard]] bool ProcessReceivedMessageConnected(const IPEndpoint& endpoint, Message&& msg) noexcept;
		[[nodiscard]] bool AckReceivedMessage(const Message::SequenceNumber seqnum) noexcept;

		[[nodiscard]] ReceiveWindow GetMessageSequenceNumberWindow(const Message::SequenceNumber seqnum) noexcept;
		[[nodiscard]] bool IsMessageSequenceNumberInCurrentWindow(const Message::SequenceNumber seqnum,
																  const Message::SequenceNumber last_seqnum,
																  const Size wnd_size) noexcept;
		[[nodiscard]] bool IsMessageSequenceNumberInPreviousWindow(const Message::SequenceNumber seqnum,
																   const Message::SequenceNumber last_seqnum,
																   const Size wnd_size) noexcept;
		
		[[nodiscard]] bool SendPendingSocketData() noexcept;
		[[nodiscard]] bool ReceivePendingSocketData() noexcept;
		
		[[nodiscard]] bool CheckKeepAlive(const Settings& settings, const SteadyTime current_steadytime) noexcept;
		void ResetKeepAliveTimeout(const Settings& settings) noexcept;
		[[nodiscard]] bool ProcessMTUDiscovery() noexcept;

		void ProcessSocketEvents(const Settings& settings) noexcept;

		void UpdateReputation(const IPEndpoint& endpoint, const Access::IPReputationUpdate rep_update) noexcept;

	private:
		static constexpr const std::chrono::seconds SuspendTimeoutMargin{ 15 };

	private:
		const Settings_CThS& m_Settings;
		Access::Manager& m_AccessManager;

		const PeerConnectionType m_Type{ PeerConnectionType::Unknown };
		Status m_Status{ Status::Closed };
		const ConnectionID m_ID{ 0 };
		
		std::unique_ptr<KeyExchange> m_KeyExchange{ nullptr };
		std::optional<ProtectedBuffer> m_GlobalSharedSecret;
		SymmetricKeysCollection m_SymmetricKeys;

		Network::Socket m_Socket;
		SteadyTime m_LastStatusChangeSteadyTime;
		std::shared_ptr<ConnectionData_ThS> m_ConnectionData;

		std::unique_ptr<MTUDiscovery> m_MTUDiscovery;

		DelayedSendItemQueue m_DelayedSendQueue{ &DelayedSendItem::Compare };
		SendQueue m_SendQueue{ *this };
		SteadyTime m_LastSendSteadyTime;
		IPEndpoint m_OriginalPeerEndpoint;
		IPEndpoint m_PeerEndpoint;
		std::chrono::seconds m_KeepAliveTimeout{ 60 };

		LastSequenceNumber m_LastInOrderReceivedSequenceNumber;
		Size m_ReceiveWindowSize{ MinReceiveWindowItemSize };
		ReceiveQueue m_ReceiveQueue;
		SteadyTime m_LastReceiveSteadyTime;
		Vector<Message::SequenceNumber> m_ReceivePendingAcks;
		Vector<Message::AckRange> m_ReceivePendingAckRanges;

		CloseCondition m_CloseCondition{ CloseCondition::None };

		std::unique_ptr<HandshakeTracker> m_HandshakeTracker;
	};
}