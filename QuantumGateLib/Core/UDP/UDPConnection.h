// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPSocket.h"
#include "UDPConnectionSendQueue.h"
#include "..\..\Common\Containers.h"

// Use to enable/disable debug console output
#define UDPCON_DEBUG

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Connection final
	{
		friend class SendQueue;
		friend class MTUDiscovery;

		struct ReceiveQueueItem final
		{
			Message::SequenceNumber SequenceNumber{ 0 };
			Buffer Data;
		};

		using ReceiveQueue = Containers::UnorderedMap<Message::SequenceNumber, ReceiveQueueItem>;

		using ReceiveAckList = Vector<Message::SequenceNumber>;

	public:
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

		void OnLocalIPInterfaceChanged() noexcept;

		static std::optional<ConnectionID> MakeConnectionID() noexcept;

	private:
		[[nodiscard]] bool SetStatus(const Status status) noexcept;

		[[nodiscard]] inline CloseCondition GetCloseCondition() const noexcept { return m_CloseCondition; }
		void SetCloseCondition(const CloseCondition cc, int socket_error_code = -1) noexcept;
		void SetSocketException(const int error_code) noexcept;
		
		inline Result<bool> SetMTUDiscovery(const bool enabled) noexcept { return m_Socket.SetMTUDiscovery(enabled); }
		void ResetMTU() noexcept;
		[[nodiscard]] bool OnMTUUpdate(const Size mtu) noexcept;

		[[nodiscard]] bool CheckEndpointChange(const IPEndpoint& endpoint) noexcept;

		[[nodiscard]] bool SendOutboundSyn() noexcept;
		[[nodiscard]] bool SendInboundSyn() noexcept;
		[[nodiscard]] bool SendData(Buffer&& data) noexcept;
		[[nodiscard]] bool SendStateUpdate() noexcept;
		[[nodiscard]] bool SendPendingAcks() noexcept;
		[[nodiscard]] bool SendKeepAlive() noexcept;
		void SendImmediateReset() noexcept;
		
		[[nodiscard]] bool Send(Message&& msg) noexcept;
		[[nodiscard]] Result<Size> Send(const SteadyTime& now, const Buffer& data, const bool use_listener_socket) noexcept;

		[[nodiscard]] bool ReceiveToQueue() noexcept;
		[[nodiscard]] bool ProcessReceivedData(const IPEndpoint& endpoint, const Buffer& buffer) noexcept;
		[[nodiscard]] bool ProcessReceivedDataHandshake(const IPEndpoint& endpoint, const Buffer& buffer) noexcept;
		[[nodiscard]] bool ProcessReceivedDataConnected(const IPEndpoint& endpoint, const Buffer& buffer) noexcept;
		[[nodiscard]] bool ProcessReceivedMessageConnected(const IPEndpoint& endpoint, Message&& msg) noexcept;
		[[nodiscard]] bool AddToReceiveQueue(ReceiveQueueItem&& itm) noexcept;
		[[nodiscard]] bool AckReceivedMessage(const Message::SequenceNumber seqnum) noexcept;

		[[nodiscard]] bool IsExpectedMessageSequenceNumber(const Message::SequenceNumber seqnum) noexcept;
		[[nodiscard]] bool IsMessageSequenceNumberInCurrentWindow(const Message::SequenceNumber seqnum,
																  const Message::SequenceNumber last_seqnum,
																  const Size wnd_size) noexcept;
		[[nodiscard]] bool IsMessageSequenceNumberInPreviousWindow(const Message::SequenceNumber seqnum,
																   const Message::SequenceNumber last_seqnum,
																   const Size wnd_size) noexcept;
		
		[[nodiscard]] bool SendPendingSocketData() noexcept;
		[[nodiscard]] bool ReceivePendingSocketData() noexcept;
		
		[[nodiscard]] bool CheckKeepAlive() noexcept;
		void ResetKeepAliveTimeout() noexcept;
		[[nodiscard]] bool ProcessMTUDiscovery() noexcept;

		void ProcessSocketEvents() noexcept;

	private:
		PeerConnectionType m_Type{ PeerConnectionType::Unknown };
		Status m_Status{ Status::Closed };
		ConnectionID m_ID{ 0 };
		Network::Socket m_Socket;
		SteadyTime m_LastStatusChangeSteadyTime;
		std::shared_ptr<ConnectionData_ThS> m_ConnectionData;

		std::unique_ptr<MTUDiscovery> m_MTUDiscovery;

		SendQueue m_SendQueue{ *this };
		SteadyTime m_LastSendSteadyTime;
		IPEndpoint m_PeerEndpoint;
		std::chrono::seconds m_KeepAliveTimeout{ MaxKeepAliveTimeout };

		Message::SequenceNumber m_LastInSequenceReceivedSequenceNumber{ 0 };
		Size m_ReceiveWindowSize{ MinReceiveWindowItemSize };
		ReceiveQueue m_ReceiveQueue;
		SteadyTime m_LastReceiveSteadyTime;
		ReceiveAckList m_ReceivePendingAckList;

		CloseCondition m_CloseCondition{ CloseCondition::None };
	};
}