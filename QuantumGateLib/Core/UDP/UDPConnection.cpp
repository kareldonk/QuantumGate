// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnection.h"
#include "..\..\Crypto\Crypto.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	bool Connection::Open(const Network::IP::AddressFamily af,
						  const bool nat_traversal, UDP::Socket& socket) noexcept
	{
		try
		{
			m_NextSendSequenceNumber = static_cast<UInt16>(Util::GetPseudoRandomNumber());
			m_Socket = Network::Socket(af, Network::Socket::Type::Datagram, Network::IP::Protocol::UDP);
			m_ConnectionData = std::make_shared<Socket::ConnectionData_ThS>(&m_Socket.GetEvent());
			m_SendQueue = std::make_unique<SendQueue>();
			m_ReceiveQueue = std::make_unique<ReceiveQueue>();

			if (m_Socket.SetNATTraversal(nat_traversal))
			{
				if (SetStatus(Status::Open))
				{
					socket.SetConnectionData(m_ConnectionData);
					return true;
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Exception while initializing UDP connection - %s", Util::ToStringW(e.what()).c_str());
		}

		return false;
	}

	void Connection::Close() noexcept
	{
		assert(GetStatus() != Status::Closed);

		SetCloseCondition(CloseCondition::CloseRequest);

		DiscardReturnValue(SetStatus(Status::Closed));

		if (m_Socket.GetIOStatus().IsOpen()) m_Socket.Close();
	}

	std::optional<ConnectionID> Connection::MakeConnectionID() noexcept
	{
		if (const auto cid = Crypto::GetCryptoRandomNumber(); cid.has_value())
		{
			return { *cid };
		}

		return std::nullopt;
	}

	bool Connection::SetStatus(const Status status) noexcept
	{
		auto success = true;
		const auto prev_status = m_Status;

		switch (status)
		{
			case Status::Open:
				assert(prev_status == Status::Closed);
				if (prev_status == Status::Closed) m_Status = status;
				else success = false;
				break;
			case Status::Handshake:
				assert(prev_status == Status::Open);
				if (prev_status == Status::Open) m_Status = status;
				else success = false;
				break;
			case Status::Connected:
				assert(prev_status == Status::Handshake);
				if (prev_status == Status::Handshake) m_Status = status;
				else success = false;
				break;
			case Status::Closed:
				assert(prev_status != Status::Closed);
				if (prev_status != Status::Closed) m_Status = status;
				else success = false;
				break;
			default:
				assert(false);
				success = false;
				break;
		}

		if (success)
		{
			m_LastStatusChangeSteadyTime = Util::GetCurrentSteadyTime();
		}
		else
		{
			// If we fail to change the status disconnect as soon as possible
			LogErr(L"UDP connection: failed to change status for connection %llu to %d", GetID(), status);
			SetCloseCondition(CloseCondition::GeneralFailure);
		}

		return success;
	}

	void Connection::SetCloseCondition(const CloseCondition cc, int socket_error_code) noexcept
	{
		if (ShouldClose()) return;

		m_CloseCondition = cc;

		if (socket_error_code == -1)
		{
			switch (cc)
			{
				case CloseCondition::GeneralFailure:
				case CloseCondition::ReceiveError:
				case CloseCondition::SendError:
				case CloseCondition::UnknownMessageError:
					socket_error_code = WSAECONNABORTED;
					break;
				case CloseCondition::TimedOutError:
					socket_error_code = WSAETIMEDOUT;
					break;
				case CloseCondition::SocketError:
					socket_error_code = WSAECONNRESET;
					break;
				case CloseCondition::CloseRequest:
					socket_error_code = WSAEDISCON;
					break;
				case CloseCondition::IPNotAllowed:
					socket_error_code = WSAEACCES;
					break;
				default:
					// Shouldn't get here
					assert(false);
					break;
			}
		}

		SetException(socket_error_code);
	}

	void Connection::SetException(const int error_code) noexcept
	{
		m_ConnectionData->WithUniqueLock([&](auto& connection_data)
		{
			connection_data.RemoveSendEvent();
			connection_data.HasException = true;
			connection_data.ErrorCode = error_code;
		});
	}

	bool Connection::SendOutboundSyn(const IPEndpoint endpoint) noexcept
	{
		LogDbg(L"UDP connection: sending outbound SYN to peer %s for connection %llu",
			   endpoint.GetString().c_str(), GetID());

		Message msg(Message::Type::Syn);
		msg.SetProtocolVersion(ProtocolVersion::Major, ProtocolVersion::Minor);
		msg.SetConnectionID(GetID());
		msg.SetMessageSequenceNumber(m_NextSendSequenceNumber);
		msg.SetMessageAckNumber(static_cast<UInt16>(Util::GetPseudoRandomNumber()));

		if (Send(std::move(msg)))
		{
			IncrementSendSequenceNumber();
			return true;
		}

		return false;
	}

	bool Connection::SendInboundSyn(const IPEndpoint endpoint) noexcept
	{
		LogDbg(L"UDP connection: sending inbound SYN to peer %s for connection %llu",
			   endpoint.GetString().c_str(), GetID());

		Message msg(Message::Type::Syn);
		msg.SetProtocolVersion(ProtocolVersion::Major, ProtocolVersion::Minor);
		msg.SetConnectionID(GetID());
		msg.SetMessageSequenceNumber(m_NextSendSequenceNumber);
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);

		if (Send(std::move(msg)))
		{
			IncrementSendSequenceNumber();
			return true;
		}

		return false;
	}

	bool Connection::SendPendingAcks() noexcept
	{
		if (m_ReceivePendingAckList.empty()) return true;

		LogDbg(L"UDP connection: sending acks to peer %s for connection %llu",
			   m_ConnectionData->WithSharedLock()->PeerEndpoint.GetString().c_str(), GetID());

		Message msg(Message::Type::Normal);
		msg.SetMessageSequenceNumber(m_NextSendSequenceNumber);
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);

		const auto max_num_acks = msg.GetMaxAckSequenceNumbersPerMessage();
		if (m_ReceivePendingAckList.size() <= max_num_acks)
		{
			msg.SetAckSequenceNumbers(std::move(m_ReceivePendingAckList));
		}
		else
		{
			Vector<MessageSequenceNumber> temp_acks;
			const auto last = m_ReceivePendingAckList.begin() + max_num_acks;
			std::copy(m_ReceivePendingAckList.begin(), last, std::back_inserter(temp_acks));
			msg.SetAckSequenceNumbers(std::move(temp_acks));
			m_ReceivePendingAckList.erase(m_ReceivePendingAckList.begin(), last);
		}

		if (Send(std::move(msg)))
		{
			m_ReceivePendingAckList.clear();

			IncrementSendSequenceNumber();

			return true;
		}

		return false;
	}

	void Connection::IncrementSendSequenceNumber() noexcept
	{
		m_NextSendSequenceNumber = GetNextExpectedSequenceNumber(m_NextSendSequenceNumber);
	}

	MessageSequenceNumber Connection::GetNextExpectedSequenceNumber(const MessageSequenceNumber current) const noexcept
	{
		if (current == std::numeric_limits<MessageSequenceNumber>::max())
		{
			return 0;
		}
		else return current + 1;
	}

	bool Connection::Send(Message&& msg) noexcept
	{
		assert(msg.IsValid());

		try
		{
			SendQueueItem itm{
				.AckRequired = (msg.IsSyn() || (msg.IsNormal() && msg.IsData())),
				.SequenceNumber = msg.GetMessageSequenceNumber(),
				.TimeSent = Util::GetCurrentSteadyTime()
			};

			if (msg.Write(itm.Data))
			{
				m_SendQueue->emplace_back(std::move(itm));
				return true;
			}
		}
		catch (...) {}

		return false;
	}

	bool Connection::SendFromQueue() noexcept
	{
		const auto endpoint = m_ConnectionData->WithSharedLock()->PeerEndpoint;
		
		for (auto it = m_SendQueue->begin(); it != m_SendQueue->end();)
		{
			auto erase = false;

			if (it->NumTries == 0 || Util::GetCurrentSteadyTime() - it->TimeSent >= m_RetransmissionTimeout)
			{
				if (it->AckRequired) LogDbg(L"Sending message with seq# %u", it->SequenceNumber);
				
				if (it->NumTries > 0)
				{
					LogWarn(L"Retransmitting (%u) message with seq# %u", it->NumTries, it->SequenceNumber);
				}

				const auto result = m_Socket.SendTo(endpoint, it->Data);
				if (result.Succeeded())
				{
					// If data was actually sent, otherwise buffer may
					// temporarily be full/unavailable
					if (*result == it->Data.GetSize())
					{
						if (it->AckRequired)
						{
							// We'll wait for ack or else continue sending
							it->TimeSent = Util::GetCurrentSteadyTime();
							++it->NumTries;
						}
						else erase = true;
					}
				}
				else
				{
					LogErr(L"UDP connection: send failed for peer %s connection %llu (%s)",
						   endpoint.GetString().c_str(), GetID(), result.GetErrorString().c_str());
					return false;
				}
			}

			if (erase) it = m_SendQueue->erase(it);
			else ++it;
		}

		return true;
	}

	bool Connection::ReceiveToQueue() noexcept
	{
		IPEndpoint endpoint;
		Buffer buffer;

		while (HasAvailableReceiveWindowSpace())
		{
			if (m_Socket.UpdateIOStatus(0ms))
			{
				if (m_Socket.GetIOStatus().CanRead())
				{
					const auto result = m_Socket.ReceiveFrom(endpoint, buffer);
					if (result.Succeeded())
					{
						if (*result > 0)
						{
							if (!ProcessReceivedData(endpoint, buffer))
							{
								return false;
							}
						}
						else break;
					}
					else
					{
						LogErr(L"UDP connection: receive failed for connection %llu (%s)",
							   GetID(), result.GetErrorString().c_str());

						if (result.GetErrorCode().category() == std::system_category())
						{
							SetCloseCondition(CloseCondition::ReceiveError, result.GetErrorCode().value());
						}

						return false;
					}

					buffer.Clear();
				}
				else if (m_Socket.GetIOStatus().HasException())
				{
					LogErr(L"UDP connection: exception on socket for connection %llu", GetID());

					SetCloseCondition(CloseCondition::ReceiveError,
										   m_Socket.GetIOStatus().GetErrorCode());

					return false;
				}
				else break;
			}
			else
			{
				LogDbg(L"UDP connection: failed to update socket IOStatus for connection %llu", GetID());
				
				return false;
			}
		}

		return true;
	}
	
	bool Connection::ProcessReceivedData(const IPEndpoint& endpoint, const Buffer& buffer) noexcept
	{
		switch (GetStatus())
		{
			case Status::Handshake:
				return ProcessReceivedDataHandshake(endpoint, buffer);
			case Status::Connected:
				return ProcessReceivedDataConnected(endpoint, buffer);
			default:
				// Shouldn't get here
				assert(false);
				break;
		}

		return false;
	}

	bool Connection::ProcessReceivedDataHandshake(const IPEndpoint& endpoint, const Buffer& buffer) noexcept
	{
		if (GetType() == PeerConnectionType::Outbound)
		{
			Message msg(Message::Type::Syn);
			if (msg.Read(buffer) && msg.IsValid())
			{
				const auto version = msg.GetProtocolVersion();

				if (version.first == UDP::ProtocolVersion::Major && version.second == UDP::ProtocolVersion::Minor)
				{
					if (GetID() == msg.GetConnectionID())
					{
						m_LastInSequenceReceivedSequenceNumber = msg.GetMessageSequenceNumber();

						ProcessReceivedAck(msg.GetMessageAckNumber());

						m_ReceivePendingAckList.emplace_back(msg.GetMessageSequenceNumber());

						if (SetStatus(Status::Connected))
						{
							m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
							{
								// Endpoint update
								connection_data.LocalEndpoint = m_Socket.GetLocalEndpoint();
								connection_data.PeerEndpoint = endpoint;
								// Socket can now send data
								connection_data.CanWrite = true;
								// Notify of state change
								connection_data.ReceiveEvent.Set();
							});

							return true;
						}
					}
					else LogErr(L"UDP connection: received invalid SYN message from peer %s; unexpected connection ID",
								endpoint.GetString().c_str());
				}
				else LogErr(L"UDP connection: could not accept connection from peer %s; unsupported UDP protocol version",
							endpoint.GetString().c_str());
			}
			else LogErr(L"UDP connection: received invalid message from peer %s", endpoint.GetString().c_str());
		}
		else if (GetType() == PeerConnectionType::Inbound)
		{
			Message msg(Message::Type::Normal);
			if (msg.Read(buffer) && msg.IsValid())
			{
				if (ProcessReceivedMessageConnected(std::move(msg)))
				{
					if (SetStatus(Status::Connected))
					{
						m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
						{
							// Socket can now send data
							connection_data.CanWrite = true;
							// Notify of state change
							connection_data.ReceiveEvent.Set();
						});

						return true;
					}
				}
			}
			else LogErr(L"UDP connection: received invalid message from peer %s", endpoint.GetString().c_str());
		}

		return false;
	}

	bool Connection::ProcessReceivedDataConnected(const IPEndpoint& endpoint, const Buffer& buffer) noexcept
	{
		Message msg(Message::Type::Normal);
		if (msg.Read(buffer) && msg.IsValid())
		{
			return ProcessReceivedMessageConnected(std::move(msg));
		}
		else
		{
			LogErr(L"UDP connection: received invalid message from peer %s", endpoint.GetString().c_str());
		}

		return false;
	}

	bool Connection::ProcessReceivedMessageConnected(Message&& msg) noexcept
	{
		try
		{
			ProcessReceivedAck(msg.GetMessageAckNumber());

			if (msg.IsAck())
			{
				ProcessReceivedAcks(msg.GetAckSequenceNumbers());
			}
			else
			{
				m_ReceivePendingAckList.emplace_back(msg.GetMessageSequenceNumber());
			}

			ReceiveQueueItem itm{
				.SequenceNumber = msg.GetMessageSequenceNumber(),
				.TimeReceived = Util::GetCurrentSteadyTime(),
			};

			if (msg.IsData()) itm.Data = msg.MoveMessageData();

			m_ReceiveQueue->emplace(msg.GetMessageSequenceNumber(), std::move(itm));

			return true;
		}
		catch (...) {}

		return false;
	}

	bool Connection::AckSentMessage(const MessageSequenceNumber seqnum) noexcept
	{
		auto it = std::find_if(m_SendQueue->begin(), m_SendQueue->end(), [&](const auto& itm)
		{
			return (itm.SequenceNumber == seqnum);
		});

		if (it != m_SendQueue->end())
		{
			LogDbg(L"Received ack for message with seq# %u", seqnum);

			m_SendQueue->erase(it);
		}

		return true;
	}

	bool Connection::ProcessReceivedAck(const MessageSequenceNumber seqnum) noexcept
	{
		for (auto it = m_SendQueue->begin(); it != m_SendQueue->end();)
		{
			if (it->SequenceNumber <= seqnum)
			{
				it = m_SendQueue->erase(it);
			}
			else ++it;
		}

		return true;
	}

	bool Connection::ProcessReceivedAcks(const Vector<MessageSequenceNumber> acks) noexcept
	{
		for (const auto ack_num : acks)
		{
			AckSentMessage(ack_num);
		}

		return true;
	}

	void Connection::ProcessEvents() noexcept
	{
		ProcessSocketEvents();

		if (ShouldClose()) return;

		if (!ReceiveToQueue())
		{
			SetCloseCondition(CloseCondition::ReceiveError);
		}

		SendPendingAcks();

		if (!ReceivePendingSocketData())
		{
			SetCloseCondition(CloseCondition::ReceiveError);
		}

		if (!SendPendingSocketData())
		{
			SetCloseCondition(CloseCondition::SendError);
		}

		if (!SendFromQueue())
		{
			SetCloseCondition(CloseCondition::SendError);
		}
	}

	bool Connection::SendPendingSocketData() noexcept
	{
		try
		{
			auto connection_data = m_ConnectionData->WithUniqueLock();

			while (HasAvailableSendWindowSpace() && connection_data->SendBuffer.GetReadSize() > 0)
			{
				Message msg(Message::Type::Normal);
				msg.SetMessageSequenceNumber(m_NextSendSequenceNumber);
				msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);

				auto read_size = connection_data->SendBuffer.GetReadSize();
				if (read_size > msg.GetMaxMessageDataSize()) read_size = msg.GetMaxMessageDataSize();

				Buffer buffer(read_size);
				connection_data->SendBuffer.Read(buffer);
				msg.SetMessageData(std::move(buffer));

				if (Send(std::move(msg)))
				{
					IncrementSendSequenceNumber();
				}
				else return false;
			}
		}
		catch (...) { return false; }

		return true;
	}

	bool Connection::ReceivePendingSocketData() noexcept
	{
		if (m_ReceiveQueue->empty()) return true;

		auto next_itm = m_ReceiveQueue->find(GetNextExpectedSequenceNumber(m_LastInSequenceReceivedSequenceNumber));
		if (next_itm == m_ReceiveQueue->end()) return true;

		try
		{
			auto connection_data = m_ConnectionData->WithUniqueLock();
			
			auto rcv_event = false;

			while (next_itm != m_ReceiveQueue->end())
			{
				auto remove = false;

				auto& rcv_itm = next_itm->second;

				if (rcv_itm.Data.IsEmpty())
				{
					remove = true;
				}
				else if (connection_data->ReceiveBuffer.GetWriteSize() >= rcv_itm.Data.GetSize())
				{
					connection_data->ReceiveBuffer.Write(rcv_itm.Data);
					rcv_event = true;
					remove = true;
				}
				else break;

				if (remove)
				{
					m_LastInSequenceReceivedSequenceNumber = rcv_itm.SequenceNumber;
					m_ReceiveQueue->erase(next_itm);
				}

				next_itm = m_ReceiveQueue->find(GetNextExpectedSequenceNumber(m_LastInSequenceReceivedSequenceNumber));
			}

			if (rcv_event)
			{
				connection_data->ReceiveEvent.Set();
				connection_data->CanRead = true;
			}
		}
		catch (...) { return false; }

		return true;
	}

	void Connection::ProcessSocketEvents() noexcept
	{
		auto close_condition = CloseCondition::None;

		m_ConnectionData->WithSharedLock([&](const auto& connection_data)
		{
			// Connect requested by socket
			if (connection_data.Connect && GetStatus() == Status::Open)
			{
				auto success = true;

				switch (GetType())
				{
					case PeerConnectionType::Inbound:
						success = SendInboundSyn(connection_data.PeerEndpoint);
						break;
					case PeerConnectionType::Outbound:
						success = SendOutboundSyn(connection_data.PeerEndpoint);
						break;
					default:
						assert(false);
						success = false;
						break;
				}

				if (success) success = SetStatus(Status::Handshake);

				if (!success) close_condition = CloseCondition::GeneralFailure;
			}

			// Close requested by socket
			if (connection_data.Close)
			{
				close_condition = CloseCondition::CloseRequest;
			}
		});

		if (close_condition != CloseCondition::None)
		{
			SetCloseCondition(close_condition);
		}
	}

	bool Connection::HasAvailableReceiveWindowSpace() const noexcept
	{
		return (m_ReceiveWindowSize > m_ReceiveQueue->size());
	}

	bool Connection::HasAvailableSendWindowSpace() const noexcept
	{
		return (m_SendWindowSize > m_SendQueue->size());
	}
}