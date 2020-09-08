// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPSocket.h"
#include "UDPMessage.h"
#include "UDPConnectionMTUD.h"
#include "UDPConnectionStats.h"
#include "..\..\Common\Containers.h"

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Connection final
	{
		struct SendQueueItem final
		{
			Message::SequenceNumber SequenceNumber{ 0 };
			bool IsSyn{ false };
			UInt NumTries{ 0 };
			SteadyTime TimeSent;
			SteadyTime TimeResent;
			Buffer Data;
			bool Acked{ false };
			SteadyTime TimeAcked;
		};

		using SendQueue = Containers::List<SendQueueItem>;

		struct ReceiveQueueItem final
		{
			Message::SequenceNumber SequenceNumber{ 0 };
			Buffer Data;
		};

		using ReceiveQueue = Containers::UnorderedMap<Message::SequenceNumber, ReceiveQueueItem>;

		using ReceiveAckList = Vector<Message::SequenceNumber>;

	public:
		enum class Status { Open, Handshake, Connected, Closed };

		enum class CloseCondition
		{
			None, GeneralFailure, TimedOutError, ReceiveError, SendError, UnknownMessageError,
			LocalCloseRequest, PeerCloseRequest
		};

		Connection(const PeerConnectionType type, const ConnectionID id, const Message::SequenceNumber seqnum) noexcept;
		Connection(const Connection&) = delete;
		Connection(Connection&&) noexcept = delete;
		~Connection();
		Connection& operator=(const Connection&) = delete;
		Connection& operator=(Connection&&) noexcept = delete;

		[[nodiscard]] inline PeerConnectionType GetType() const noexcept { return m_Type; }
		[[nodiscard]] inline Status GetStatus() const noexcept { return m_Status; }
		[[nodiscard]] inline ConnectionID GetID() const noexcept { return m_ID; }

		[[nodiscard]] bool Open(const Network::IP::AddressFamily af,
								const bool nat_traversal, UDP::Socket& socket) noexcept;
		void Close() noexcept;

		Concurrency::Event& GetReadEvent() noexcept { return m_Socket.GetEvent(); }

		void ProcessEvents() noexcept;
		[[nodiscard]] inline bool ShouldClose() const noexcept { return (m_CloseCondition != CloseCondition::None); }

		static std::optional<ConnectionID> MakeConnectionID() noexcept;

	private:
		[[nodiscard]] bool SetStatus(const Status status) noexcept;

		[[nodiscard]] inline CloseCondition GetCloseCondition() const noexcept { return m_CloseCondition; }
		void SetCloseCondition(const CloseCondition cc, int socket_error_code = -1) noexcept;
		void SetSocketException(const int error_code) noexcept;

		void IncrementSendSequenceNumber() noexcept;
		Message::SequenceNumber GetNextSequenceNumber(const Message::SequenceNumber current) const noexcept;
		Message::SequenceNumber GetPreviousSequenceNumber(const Message::SequenceNumber current) const noexcept;

		void AckSentMessage(const Message::SequenceNumber seqnum) noexcept;
		void ProcessReceivedInSequenceAck(const Message::SequenceNumber seqnum) noexcept;
		void ProcessReceivedAcks(const Vector<Message::SequenceNumber>& acks) noexcept;
		void PurgeAckedMessages() noexcept;
		[[nodiscard]] bool AckReceivedMessage(const Message::SequenceNumber seqnum) noexcept;

		[[nodiscard]] bool SendOutboundSyn(const IPEndpoint& endpoint) noexcept;
		[[nodiscard]] bool SendInboundSyn(const IPEndpoint& endpoint) noexcept;
		[[nodiscard]] bool SendData(const IPEndpoint& endpoint, Buffer&& data) noexcept;
		[[nodiscard]] bool SendPendingAcks() noexcept;
		void SendImmediateReset() noexcept;

		[[nodiscard]] bool Send(const IPEndpoint& endpoint, Message&& msg, const bool queue) noexcept;
		[[nodiscard]] bool SendFromQueue() noexcept;
		[[nodiscard]] bool SendPendingSocketData() noexcept;

		[[nodiscard]] bool ReceiveToQueue() noexcept;
		[[nodiscard]] bool ProcessReceivedData(const IPEndpoint& endpoint, const Buffer& buffer) noexcept;
		[[nodiscard]] bool ProcessReceivedDataHandshake(const IPEndpoint& endpoint, const Buffer& buffer) noexcept;
		[[nodiscard]] bool ProcessReceivedDataConnected(const IPEndpoint& endpoint, const Buffer& buffer) noexcept;
		[[nodiscard]] bool ProcessReceivedMessageConnected(const IPEndpoint& endpoint, Message&& msg) noexcept;
		[[nodiscard]] bool IsExpectedMessageSequenceNumber(const Message::SequenceNumber seqnum) noexcept;
		[[nodiscard]] bool ReceivePendingSocketData() noexcept;
		void ProcessMTUDiscovery() noexcept;

		void ProcessSocketEvents() noexcept;
		[[nodiscard]] bool HasAvailableSendWindowSpace() const noexcept;

	private:
		static constexpr std::chrono::seconds ConnectTimeout{ 30 };
		static constexpr std::chrono::milliseconds ConnectRetransmissionTimeout{ 1000 };
		static constexpr Size MinReceiveWindowSize{ 128 };
		static constexpr Size MaxReceiveWindowSize{ 65535 };
		static constexpr Size MaxReceiveWindowBytes{ 1 << 20 };

	private:
		PeerConnectionType m_Type{ PeerConnectionType::Unknown };
		Status m_Status{ Status::Closed };
		ConnectionID m_ID{ 0 };
		Network::Socket m_Socket;
		Size m_MaxMessageSize{ MTUDiscovery::MessageSizes[0] };
		SteadyTime m_LastStatusChangeSteadyTime;
		std::shared_ptr<ConnectionData_ThS> m_ConnectionData;

		std::unique_ptr<MTUDiscovery> m_MTUDiscovery;
		Statistics m_Statistics;

		Message::SequenceNumber m_NextSendSequenceNumber{ 0 };
		Message::SequenceNumber m_LastInSequenceAckedSequenceNumber{ 0 };
		SendQueue m_SendQueue;

		Message::SequenceNumber m_LastInSequenceReceivedSequenceNumber{ 0 };
		Size m_ReceiveWindowSize{ MinReceiveWindowSize };
		ReceiveQueue m_ReceiveQueue;
		ReceiveAckList m_ReceivePendingAckList;

		CloseCondition m_CloseCondition{ CloseCondition::None };
	};
}