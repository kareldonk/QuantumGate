// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPSocket.h"
#include "UDPMessage.h"
#include "..\..\Concurrency\Queue.h"

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Connection final
	{
		struct SendQueueItem final
		{
			MessageSequenceNumber SequenceNumber{ 0 };
			UInt NumTries{ 0 };
			SteadyTime TimeSent;
			Buffer Data;
			bool Acked{ false };
			SteadyTime TimeAcked;
		};

		using SendQueue = Containers::List<SendQueueItem>;

		struct ReceiveQueueItem final
		{
			MessageSequenceNumber SequenceNumber{ 0 };
			SteadyTime TimeReceived;
			Buffer Data;
		};

		using ReceiveQueue = Containers::UnorderedMap<MessageSequenceNumber, ReceiveQueueItem>;

		using ReceiveAckList = Vector<MessageSequenceNumber>;
		
	public:
		enum class Status { Open, Handshake, Connected, Closed };

		enum class CloseCondition
		{
			None, GeneralFailure, TimedOutError, ReceiveError, SendError, UnknownMessageError, CloseRequest
		};

		Connection(const PeerConnectionType type, const ConnectionID id, const MessageSequenceNumber seqnum) noexcept :
			m_Type(type), m_ID(id), m_LastInSequenceReceivedSequenceNumber(seqnum) {}

		Connection(const Connection&) = delete;
		Connection(Connection&& other) noexcept = delete;
		~Connection() = default;
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
		static constexpr std::chrono::milliseconds MinRetransmissionTimeout{ 100 };
		static constexpr Size MinSendWindowSize{ 2 };
		static constexpr Size MinReceiveWindowSize{ 32 };

	private:
		[[nodiscard]] bool SetStatus(const Status status) noexcept;

		[[nodiscard]] inline CloseCondition GetCloseCondition() const noexcept { return m_CloseCondition; }
		void SetCloseCondition(const CloseCondition cc, int socket_error_code = -1) noexcept;
		void SetException(const int error_code) noexcept;

		void IncrementSendSequenceNumber() noexcept;
		MessageSequenceNumber GetNextExpectedSequenceNumber(const MessageSequenceNumber current) const noexcept;
		MessageSequenceNumber GetPreviousSequenceNumber(const MessageSequenceNumber current) const noexcept;

		[[nodiscard]] bool AckSentMessage(const MessageSequenceNumber seqnum) noexcept;
		[[nodiscard]] bool ProcessReceivedAck(const MessageSequenceNumber seqnum) noexcept;
		[[nodiscard]] bool ProcessReceivedAcks(const Vector<MessageSequenceNumber> acks) noexcept;

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
		[[nodiscard]] bool ProcessReceivedMessageConnected(Message&& msg) noexcept;
		[[nodiscard]] bool ReceivePendingSocketData() noexcept;

		void ProcessSocketEvents() noexcept;
		[[nodiscard]] bool HasAvailableReceiveWindowSpace() const noexcept;
		[[nodiscard]] bool HasAvailableSendWindowSpace() const noexcept;

	private:
		PeerConnectionType m_Type{ PeerConnectionType::Unknown };
		Status m_Status{ Status::Closed };
		ConnectionID m_ID{ 0 };
		Network::Socket m_Socket;
		SteadyTime m_LastStatusChangeSteadyTime;
		std::shared_ptr<UDP::Socket::ConnectionData_ThS> m_ConnectionData;

		MessageSequenceNumber m_NextSendSequenceNumber{ 0 };
		std::chrono::milliseconds m_RetransmissionTimeout{ MinRetransmissionTimeout };
		SendQueue m_SendQueue;
		Size m_SendWindowSize{ MinSendWindowSize };

		MessageSequenceNumber m_LastInSequenceReceivedSequenceNumber{ 0 };
		Size m_ReceiveWindowSize{ MinReceiveWindowSize };
		ReceiveQueue m_ReceiveQueue;
		ReceiveAckList m_ReceivePendingAckList;
		
		CloseCondition m_CloseCondition{ CloseCondition::None };
	};
}