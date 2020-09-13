// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnection.h"
#include "..\..\Crypto\Crypto.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	Connection::Connection(const PeerConnectionType type, const ConnectionID id,
						   const Message::SequenceNumber seqnum) noexcept :
		m_Type(type), m_ID(id), m_LastInSequenceReceivedSequenceNumber(seqnum)
	{}

	Connection::~Connection()
	{
		if (m_Socket.GetIOStatus().IsOpen()) m_Socket.Close();
	}

	bool Connection::Open(const Network::IP::AddressFamily af,
						  const bool nat_traversal, UDP::Socket& socket) noexcept
	{
		try
		{
			m_Socket = Network::Socket(af, Network::Socket::Type::Datagram, Network::IP::Protocol::UDP);

			if (m_Socket.Bind(IPEndpoint(IPEndpoint::Protocol::UDP,
										 af == Network::IP::AddressFamily::IPv4 ? IPAddress::AnyIPv4() : IPAddress::AnyIPv6(),
										 0), nat_traversal))
			{
				m_ConnectionData = std::make_shared<ConnectionData_ThS>(&m_Socket.GetEvent());
				m_MTUDiscovery = std::make_unique<MTUDiscovery>();
				
				if (SetStatus(Status::Open))
				{
					socket.SetConnectionData(m_ConnectionData);
					return true;
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"UDP connection: an exception occured while initializing connection %llu - %s",
				   Util::ToStringW(e.what()).c_str(), GetID());
		}

		return false;
	}

	void Connection::Close() noexcept
	{
		assert(GetStatus() != Status::Closed);

		auto connection_data = m_ConnectionData->WithSharedLock();
		if (!connection_data->HasCloseRequest())
		{
			SendImmediateReset(connection_data->GetPeerEndpoint());
		}

		DiscardReturnValue(SetStatus(Status::Closed));
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
				if (prev_status == Status::Handshake)
				{
					m_Status = status;
					ResetKeepAliveTimeout();
				}
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
				case CloseCondition::LocalCloseRequest:
				case CloseCondition::PeerCloseRequest:
					break;
				default:
					// Shouldn't get here
					assert(false);
					break;
			}
		}

		SetSocketException(socket_error_code);
	}

	void Connection::SetSocketException(const int error_code) noexcept
	{
		if (error_code == -1) return;

		m_ConnectionData->WithUniqueLock([&](auto& connection_data)
		{
			connection_data.RemoveSendEvent();
			connection_data.SetException(error_code);
		});
	}

	void Connection::ProcessEvents() noexcept
	{
		ProcessSocketEvents();

		if (ShouldClose()) return;

		if (!ReceiveToQueue())
		{
			SetCloseCondition(CloseCondition::ReceiveError);
		}

		const auto endpoint = m_ConnectionData->WithSharedLock()->GetPeerEndpoint();

		if (!SendPendingAcks(endpoint))
		{
			SetCloseCondition(CloseCondition::SendError);
		}

		if (!m_SendQueue.Process(endpoint))
		{
			SetCloseCondition(CloseCondition::SendError);
		}

		switch (GetStatus())
		{
			case Status::Handshake:
			{
				if (Util::GetCurrentSteadyTime() - m_LastStatusChangeSteadyTime >= ConnectTimeout)
				{
					LogDbg(L"UDP connection: connect timed out for connection %llu", GetID());

					SetCloseCondition(CloseCondition::TimedOutError);
				}
				break;
			}
			case Status::Connected:
			{

				if (!CheckKeepAlive(endpoint) || !ProcessMTUDiscovery(endpoint))
				{
					SetCloseCondition(CloseCondition::GeneralFailure);
				}

				if (!ReceivePendingSocketData())
				{
					SetCloseCondition(CloseCondition::ReceiveError);
				}

				if (!SendPendingSocketData())
				{
					SetCloseCondition(CloseCondition::SendError);
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}

	bool Connection::CheckKeepAlive(const IPEndpoint& endpoint) noexcept
	{
		if (Util::GetCurrentSteadyTime() - m_LastSendSteadyTime >= m_KeepAliveTimeout)
		{
			ResetKeepAliveTimeout();

			return SendKeepAlive(endpoint);
		}

		return true;
	}

	void Connection::ResetKeepAliveTimeout() noexcept
	{
		m_KeepAliveTimeout = std::chrono::seconds(Random::GetPseudoRandomNumber(MinKeepAliveTimeout.count(),
																				MaxKeepAliveTimeout.count()));
	}

	bool Connection::ProcessMTUDiscovery(const IPEndpoint& endpoint) noexcept
	{
		if (!m_MTUDiscovery) return true;

		const auto status = m_MTUDiscovery->Process(m_Socket, endpoint);
		switch (status)
		{
			case MTUDiscovery::Status::Finished:
			case MTUDiscovery::Status::Failed:
			{
				m_SendQueue.SetMaxMessageSize(m_MTUDiscovery->GetMaxMessageSize());
				m_ReceiveWindowSize = std::min(MaxReceiveWindowItemSize,
											   MaxReceiveWindowBytes / static_cast<UInt32>(m_MTUDiscovery->GetMaxMessageSize()));
#ifdef UDPCON_DEBUG
				SLogInfo(SLogFmt(FGCyan) << L"UDP connection: maximum message size is " <<
						 m_MTUDiscovery->GetMaxMessageSize() << L" bytes, receive window size is " << m_ReceiveWindowSize <<
						 L" for connection " << GetID() << SLogFmt(Default));
#endif
				m_MTUDiscovery.reset();

				return SendStateUpdate(endpoint);
			}
			default:
			{
				break;
			}
		}

		return true;
	}

	bool Connection::SendOutboundSyn(const IPEndpoint& endpoint) noexcept
	{
		Dbg(L"UDP connection: sending outbound SYN to peer %s for connection %llu (seq# %u)",
			endpoint.GetString().c_str(), GetID(), m_SendQueue.GetNextSendSequenceNumber());

		Message msg(Message::Type::Syn, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetProtocolVersion(ProtocolVersion::Major, ProtocolVersion::Minor);
		msg.SetConnectionID(GetID());
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());

		if (Send(endpoint, std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send outbound SYN to peer %s for connection %llu",
				   endpoint.GetString().c_str(), GetID());
		}

		return false;
	}

	bool Connection::SendInboundSyn(const IPEndpoint& endpoint) noexcept
	{
		Dbg(L"UDP connection: sending inbound SYN to peer %s for connection %llu (seq# %u)",
			endpoint.GetString().c_str(), GetID(), m_SendQueue.GetNextSendSequenceNumber());

		Message msg(Message::Type::Syn, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetProtocolVersion(ProtocolVersion::Major, ProtocolVersion::Minor);
		msg.SetConnectionID(GetID());
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetPort(m_Socket.GetLocalEndpoint().GetPort());

		if (Send(endpoint, std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send inbound SYN to peer %s for connection %llu",
				   endpoint.GetString().c_str(), GetID());
		}

		return false;
	}

	bool Connection::SendData(const IPEndpoint& endpoint, Buffer&& data) noexcept
	{
		Dbg(L"UDP connection: sending data to peer %s for connection %llu (seq# %u)",
			endpoint.GetString().c_str(), GetID(), m_SendQueue.GetNextSendSequenceNumber());

		Message msg(Message::Type::Data, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetMessageData(std::move(data));

		if (Send(endpoint, std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send data to peer %s for connection %llu",
				   endpoint.GetString().c_str(), GetID());
		}

		return false;
	}

	bool Connection::SendStateUpdate(const IPEndpoint& endpoint) noexcept
	{
		Dbg(L"UDP connection: sending state update to peer %s for connection %llu (seq# %u)",
			endpoint.GetString().c_str(), GetID(), m_SendQueue.GetNextSendSequenceNumber());

		Message msg(Message::Type::State, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetStateData(
			Message::StateData{
				.MaxWindowSize = static_cast<UInt32>(m_ReceiveWindowSize),
				.MaxWindowSizeBytes = static_cast<UInt32>(MaxReceiveWindowBytes)
			});

		if (Send(endpoint, std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send state update to peer %s for connection %llu",
				   endpoint.GetString().c_str(), GetID());
		}

		return false;
	}

	bool Connection::SendPendingAcks(const IPEndpoint& endpoint) noexcept
	{
		if (m_ReceivePendingAckList.empty()) return true;
		/*
		std::erase_if(m_ReceivePendingAckList, [&](const auto& seqnum)
		{
			return IsMessageSequenceNumberInPreviousWindow(seqnum,
														   m_LastInSequenceReceivedSequenceNumber,
														   m_ReceiveWindowSize);
		});*/

		Dbg(L"UDP connection: sending acks to peer %s for connection %llu",
			endpoint.GetString().c_str(), GetID());

		Message msg(Message::Type::EAck, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);

		const auto max_num_acks = msg.GetMaxAckSequenceNumbersPerMessage();
		if (m_ReceivePendingAckList.size() <= max_num_acks)
		{
			msg.SetAckSequenceNumbers(std::move(m_ReceivePendingAckList));
		}
		else
		{
			Vector<Message::SequenceNumber> temp_acks;
			const auto last = m_ReceivePendingAckList.begin() + max_num_acks;
			std::copy(m_ReceivePendingAckList.begin(), last, std::back_inserter(temp_acks));
			msg.SetAckSequenceNumbers(std::move(temp_acks));
			m_ReceivePendingAckList.erase(m_ReceivePendingAckList.begin(), last);
		}

		if (Send(endpoint, std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send acks to peer %s for connection %llu",
				   endpoint.GetString().c_str(), GetID());
		}

		return false;
	}

	bool Connection::SendKeepAlive(const IPEndpoint& endpoint) noexcept
	{
		if (GetStatus() != Status::Connected) return true;

		Dbg(L"UDP connection: sending keepalive to peer %s for connection %llu",
			endpoint.GetString().c_str(), GetID());

		Message msg(Message::Type::Null, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetMessageData(Random::GetPseudoRandomBytes(Random::GetPseudoRandomNumber(0, msg.GetMaxMessageDataSize())));

		if (Send(endpoint, std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send keepalive to peer %s for connection %llu",
				   endpoint.GetString().c_str(), GetID());
		}

		return false;
	}

	void Connection::SendImmediateReset(const IPEndpoint& endpoint) noexcept
	{
		if (GetStatus() != Status::Handshake && GetStatus() != Status::Connected) return;

		Dbg(L"UDP connection: sending reset to peer %s for connection %llu",
			endpoint.GetString().c_str(), GetID());

		Message msg(Message::Type::Reset, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());

		if (!Send(endpoint, std::move(msg)))
		{
			LogErr(L"UDP connection: failed to send reset to peer %s for connection %llu",
				   endpoint.GetString().c_str(), GetID());
		}
	}

	bool Connection::Send(const IPEndpoint& endpoint, Message&& msg) noexcept
	{
		try
		{
			Buffer data = m_SendQueue.GetFreeBuffer();
			if (msg.Write(data))
			{
				const auto now = Util::GetCurrentSteadyTime();

				// Messages with sequence numbers need to be tracked
				// for ack and go into the send queue.
				if (msg.HasSequenceNumber())
				{
					SendQueue::Item itm{
						.SequenceNumber = msg.GetMessageSequenceNumber(),
						.MessageType = msg.GetType(),
						.TimeSent = now,
						.TimeResent = now,
						.Data = std::move(data)
					};

					return m_SendQueue.Add(endpoint, std::move(itm));
				}
				else
				{
					// Messages without sequence numbers are sent in one try
					// and we don't care if they arrive or not
					const auto result = Send(now, endpoint, data, false);
					if (result.Succeeded())
					{
						return true;
					}
					else
					{
						LogErr(L"UDP connection: send failed for peer %s connection %llu (%s)",
							   endpoint.GetString().c_str(), GetID(), result.GetErrorString().c_str());
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"UDP connection: exception while sending data for peer %s connection %llu - %s",
				   endpoint.GetString().c_str(), GetID(), Util::ToStringW(e.what()).c_str());
		}

		return false;
	}

	Result<Size> Connection::Send(const SteadyTime& now, const IPEndpoint& endpoint,
								  const Buffer& data, const bool use_listener_socket) noexcept
	{
		Network::Socket* s = &m_Socket;
		if (use_listener_socket)
		{
			LogWarn(L"UDP connection: using listener socket to send UDP msg");
			s = m_ConnectionData->WithUniqueLock()->GetListenerSocket();
		}

		auto result = s->SendTo(endpoint, data);
		if (result.Succeeded())
		{
			m_LastSendSteadyTime = now;
		}

		return result;
	}

	bool Connection::ReceiveToQueue() noexcept
	{
		IPEndpoint endpoint;
		Buffer buffer;

		while (true)
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
					LogErr(L"UDP connection: exception on socket for connection %llu (%s)",
						   GetID(), GetSysErrorString(m_Socket.GetIOStatus().GetErrorCode()).c_str());

					SetCloseCondition(CloseCondition::ReceiveError, m_Socket.GetIOStatus().GetErrorCode());

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
			Message msg(Message::Type::Syn, Message::Direction::Incoming);
			if (msg.Read(buffer) && msg.IsValid())
			{
				// Handshake response should come from same IP address that
				// we tried connecting to
				if (endpoint == m_ConnectionData->WithSharedLock()->GetPeerEndpoint())
				{
					const auto version = msg.GetProtocolVersion();

					if (version.first == UDP::ProtocolVersion::Major && version.second == UDP::ProtocolVersion::Minor)
					{
						if (GetID() == msg.GetConnectionID())
						{
							m_LastInSequenceReceivedSequenceNumber = msg.GetMessageSequenceNumber();

							m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());

							if (AckReceivedMessage(msg.GetMessageSequenceNumber()))
							{
								if (SetStatus(Status::Connected))
								{
									m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
									{
										// Endpoint update
										connection_data.SetLocalEndpoint(m_Socket.GetLocalEndpoint());
										connection_data.SetPeerEndpoint(IPEndpoint(endpoint.GetProtocol(),
																				   endpoint.GetIPAddress(),
																				   msg.GetPort()));
										// Socket can now send data
										connection_data.SetWrite(true);
										// Notify of state change
										connection_data.SignalReceiveEvent();
									});

									return true;
								}
							}
						}
						else LogErr(L"UDP connection: received invalid SYN message from peer %s on connection %llu; unexpected connection ID %llu",
									endpoint.GetString().c_str(), GetID(), msg.GetConnectionID());
					}
					else LogErr(L"UDP connection: could not accept connection from peer %s on connection %llu; unsupported UDP protocol version",
								endpoint.GetString().c_str(), GetID());
				}
				else LogErr(L"UDP connection: received handshake response from unexpected IP address %s on connection %llu",
							endpoint.GetString().c_str(), GetID());
			}
			else
			{
				LogErr(L"UDP connection: received invalid message from peer %s on connection %llu",
					   endpoint.GetString().c_str(), GetID());

				SetCloseCondition(CloseCondition::UnknownMessageError);
			}
		}
		else if (GetType() == PeerConnectionType::Inbound)
		{
			Message msg(Message::Type::Unknown, Message::Direction::Incoming);
			if (msg.Read(buffer) && msg.IsValid())
			{
				if (ProcessReceivedMessageConnected(endpoint, std::move(msg)))
				{
					if (SetStatus(Status::Connected))
					{
						m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
						{
							// Socket can now send data
							connection_data.SetWrite(true);
							// Notify of state change
							connection_data.SignalReceiveEvent();
						});

						return true;
					}
				}
			}
			else
			{
				LogErr(L"UDP connection: received invalid message from peer %s on connection %llu",
					   endpoint.GetString().c_str(), GetID());

				SetCloseCondition(CloseCondition::UnknownMessageError);
			}
		}

		return false;
	}

	bool Connection::ProcessReceivedDataConnected(const IPEndpoint& endpoint, const Buffer& buffer) noexcept
	{
		Message msg(Message::Type::Unknown, Message::Direction::Incoming);
		if (msg.Read(buffer) && msg.IsValid())
		{
			return ProcessReceivedMessageConnected(endpoint, std::move(msg));
		}
		else
		{
			LogErr(L"UDP connection: received invalid message from peer %s on connection %llu",
				   endpoint.GetString().c_str(), GetID());

			SetCloseCondition(CloseCondition::UnknownMessageError);
		}

		return false;
	}

	bool Connection::ProcessReceivedMessageConnected(const IPEndpoint& endpoint, Message&& msg) noexcept
	{
		switch (msg.GetType())
		{
			case Message::Type::Data:
			{
				Dbg(L"UDP connection: received data message from peer %s (seq# %u) on connection %llu",
					endpoint.GetString().c_str(), msg.GetMessageSequenceNumber(), GetID());

				if (IsExpectedMessageSequenceNumber(msg.GetMessageSequenceNumber()))
				{
					m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());

					if (AckReceivedMessage(msg.GetMessageSequenceNumber()))
					{
						return AddToReceiveQueue(
							ReceiveQueueItem{
								.SequenceNumber = msg.GetMessageSequenceNumber(),
								.Data = msg.MoveMessageData()
							});
					}
				}
				else return true;
				break;
			}
			case Message::Type::State:
			{
				Dbg(L"UDP connection: received state message from peer %s (seq# %u) on connection %llu",
					endpoint.GetString().c_str(), msg.GetMessageSequenceNumber(), GetID());

				m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());
				
				if (AckReceivedMessage(msg.GetMessageSequenceNumber()))
				{
					const auto state_data = msg.GetStateData();
					m_SendQueue.SetPeerAdvertisedReceiveWindowSizes(state_data.MaxWindowSize, state_data.MaxWindowSizeBytes);
			
					return AddToReceiveQueue(
						ReceiveQueueItem{
							.SequenceNumber = msg.GetMessageSequenceNumber()
						});
				}
				break;
			}
			case Message::Type::EAck:
			{
				Dbg(L"UDP connection: received ack message from peer %s on connection %llu",
					endpoint.GetString().c_str(), GetID());

				m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());
				m_SendQueue.ProcessReceivedAcks(msg.GetAckSequenceNumbers());
				return true;
			}
			case Message::Type::MTUD:
			{
				if (!msg.HasAck())
				{
					MTUDiscovery::AckReceivedMessage(m_Socket, endpoint, msg.GetMessageSequenceNumber());
				}
				else
				{
					if (m_MTUDiscovery) m_MTUDiscovery->ProcessReceivedAck(msg.GetMessageAckNumber());
				}
				return true;
			}
			case Message::Type::Reset:
			{
				Dbg(L"UDP connection: received reset message from peer %s on connection %llu",
					endpoint.GetString().c_str(), GetID());

				m_ConnectionData->WithUniqueLock()->SetCloseRequest();
				SetCloseCondition(CloseCondition::PeerCloseRequest);
				return true;
			}
			case Message::Type::Null:
			{
				Dbg(L"UDP connection: received null message from peer %s on connection %llu",
					endpoint.GetString().c_str(), GetID());

				return true;
			}
			default:
			{
				LogErr(L"UDP connection: received unknown message from peer %s on connection %llu",
					   endpoint.GetString().c_str(), GetID());
				break;
			}
		}

		return false;
	}

	bool Connection::AddToReceiveQueue(ReceiveQueueItem&& itm) noexcept
	{
		try
		{
			m_ReceiveQueue.emplace(itm.SequenceNumber, std::move(itm));
			return true;
		}
		catch (...) {}

		return false;
	}

	bool Connection::IsExpectedMessageSequenceNumber(const Message::SequenceNumber seqnum) noexcept
	{
		if (IsMessageSequenceNumberInCurrentWindow(seqnum, m_LastInSequenceReceivedSequenceNumber,
												   m_ReceiveWindowSize)) return true;

		if (IsMessageSequenceNumberInPreviousWindow(seqnum, m_LastInSequenceReceivedSequenceNumber,
													m_ReceiveWindowSize))
		{
			// May have been retransmitted due to delays; send an ack
			DiscardReturnValue(AckReceivedMessage(seqnum));
		}

		return false;
	}

	bool Connection::IsMessageSequenceNumberInCurrentWindow(const Message::SequenceNumber seqnum,
															const Message::SequenceNumber last_seqnum,
															const Size wnd_size) noexcept
	{
		constexpr const auto max_seqnum = std::numeric_limits<Message::SequenceNumber>::max();

		if (max_seqnum - wnd_size >= last_seqnum)
		{
			if (last_seqnum < seqnum && seqnum <= last_seqnum + wnd_size)
			{
				return true;
			}
		}
		else
		{
			const auto r1 = max_seqnum - last_seqnum;
			const auto r2 = wnd_size - r1;
			if (last_seqnum < seqnum && seqnum <= last_seqnum + r1)
			{
				return true;
			}
			else if (seqnum < r2) return true;
		}

		return false;
	}

	bool Connection::IsMessageSequenceNumberInPreviousWindow(const Message::SequenceNumber seqnum,
															 const Message::SequenceNumber last_seqnum,
															 const Size wnd_size) noexcept
	{
		constexpr const auto max_seqnum = std::numeric_limits<Message::SequenceNumber>::max();

		auto inprev_wnd = false;

		if (last_seqnum >= wnd_size)
		{
			if (last_seqnum - wnd_size <= seqnum && seqnum <= last_seqnum)
			{
				inprev_wnd = true;
			}
		}
		else
		{
			const auto r1 = last_seqnum;
			const auto r2 = max_seqnum - (wnd_size - r1);
			if (0 <= seqnum && seqnum <= r1)
			{
				inprev_wnd = true;
			}
			else if (r2 < seqnum && seqnum <= max_seqnum)
			{
				inprev_wnd = true;
			}
		}

		return inprev_wnd;
	}

	bool Connection::AckReceivedMessage(const Message::SequenceNumber seqnum) noexcept
	{
		try
		{
			m_ReceivePendingAckList.emplace_back(seqnum);
			return true;
		}
		catch (...) {}

		return false;
	}

	bool Connection::SendPendingSocketData() noexcept
	{
		try
		{
			auto connection_data = m_ConnectionData->WithUniqueLock();

			const Message msg(Message::Type::Data, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());

			auto sendwnd_bytes = m_SendQueue.GetAvailableSendWindowByteSize();

			while (sendwnd_bytes >= m_SendQueue.GetMaxMessageSize() && connection_data->GetSendBuffer().GetReadSize() > 0)
			{
				auto read_size = connection_data->GetSendBuffer().GetReadSize();
				if (read_size > msg.GetMaxMessageDataSize())
				{
					read_size = msg.GetMaxMessageDataSize();
				}

				Buffer buffer(read_size);
				if (connection_data->GetSendBuffer().Read(buffer) == read_size)
				{
					if (!SendData(connection_data->GetPeerEndpoint(), std::move(buffer)))
					{
						return false;
					}
				}
				else return false;

				sendwnd_bytes = m_SendQueue.GetAvailableSendWindowByteSize();
			}
		}
		catch (...) { return false; }

		return true;
	}

	bool Connection::ReceivePendingSocketData() noexcept
	{
		if (m_ReceiveQueue.empty()) return true;

		auto next_itm = m_ReceiveQueue.find(Message::GetNextSequenceNumber(m_LastInSequenceReceivedSequenceNumber));
		if (next_itm == m_ReceiveQueue.end())
		{
			return true;
		}

		try
		{
			auto connection_data = m_ConnectionData->WithUniqueLock();

			auto rcv_event = false;

			while (next_itm != m_ReceiveQueue.end())
			{
				auto remove = false;

				auto& rcv_itm = next_itm->second;

				if (rcv_itm.Data.IsEmpty())
				{
					remove = true;
				}
				else if (connection_data->GetReceiveBuffer().GetWriteSize() >= rcv_itm.Data.GetSize())
				{
					if (connection_data->GetReceiveBuffer().Write(rcv_itm.Data) ==  rcv_itm.Data.GetSize())
					{
						rcv_event = true;
						remove = true;
					}
					else return false;
				}
				else break;

				if (remove)
				{
					m_LastInSequenceReceivedSequenceNumber = rcv_itm.SequenceNumber;
					m_ReceiveQueue.erase(next_itm);
				}

				next_itm = m_ReceiveQueue.find(Message::GetNextSequenceNumber(m_LastInSequenceReceivedSequenceNumber));
			}

			if (rcv_event)
			{
				connection_data->SetRead(true);
				connection_data->SignalReceiveEvent();
			}
		}
		catch (...) { return false; }

		return true;
	}

	void Connection::ProcessSocketEvents() noexcept
	{
		auto close_condition = CloseCondition::None;

		{
			auto connection_data = m_ConnectionData->WithSharedLock();

			// Connect requested by socket
			if (GetStatus() == Status::Open && connection_data->HasConnectRequest())
			{
				auto success = true;

				const auto endpoint = connection_data->GetPeerEndpoint();

				connection_data.UnlockShared();

				switch (GetType())
				{
					case PeerConnectionType::Inbound:
						success = SendInboundSyn(endpoint);
						break;
					case PeerConnectionType::Outbound:
						success = SendOutboundSyn(endpoint);
						break;
					default:
						assert(false);
						success = false;
						break;
				}

				if (success) success = SetStatus(Status::Handshake);

				if (!success) close_condition = CloseCondition::GeneralFailure;

				connection_data.LockShared();
			}

			// Close requested by socket
			if (connection_data->HasCloseRequest())
			{
				SendImmediateReset(connection_data->GetPeerEndpoint());

				close_condition = CloseCondition::LocalCloseRequest;
			}
		}

		if (close_condition != CloseCondition::None)
		{
			SetCloseCondition(close_condition);
		}
	}
}