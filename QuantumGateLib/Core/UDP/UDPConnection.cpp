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
				m_NextSendSequenceNumber = static_cast<Message::SequenceNumber>(Random::GetPseudoRandomNumber());
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
			LogErr(L"Exception while initializing UDP connection - %s", Util::ToStringW(e.what()).c_str());
		}

		return false;
	}

	void Connection::Close() noexcept
	{
		assert(GetStatus() != Status::Closed);

		if (!m_ConnectionData->WithSharedLock()->HasCloseRequest())
		{
			SendImmediateReset();
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

		if (!SendPendingAcks())
		{
			SetCloseCondition(CloseCondition::SendError);
		}

		if (!SendFromQueue())
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
				if (!ProcessMTUDiscovery())
				{
					SetCloseCondition(CloseCondition::SendError);
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

	bool Connection::ProcessMTUDiscovery() noexcept
	{
		if (!m_MTUDiscovery) return true;

		const auto endpoint = m_ConnectionData->WithSharedLock()->GetPeerEndpoint();
		const auto status = m_MTUDiscovery->Process(m_Socket, endpoint);
		switch (status)
		{
			case MTUDiscovery::Status::Finished:
			case MTUDiscovery::Status::Failed:
			{
				m_MaxMessageSize = m_MTUDiscovery->GetMaxMessageSize();
				m_ReceiveWindowSize = std::min(MaxReceiveWindowSize, MaxReceiveWindowBytes / static_cast<UInt32>(m_MaxMessageSize));
				
				RecalcPeerReceiveWindowSize();
				
				LogWarn(L"UDP connection: receive window size is %zu", m_ReceiveWindowSize);

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
		LogDbg(L"UDP connection: sending outbound SYN to peer %s for connection %llu (seq# %u)",
			   endpoint.GetString().c_str(), GetID(), m_NextSendSequenceNumber);

		Message msg(Message::Type::Syn, Message::Direction::Outgoing, m_MaxMessageSize);
		msg.SetProtocolVersion(ProtocolVersion::Major, ProtocolVersion::Minor);
		msg.SetConnectionID(GetID());
		msg.SetMessageSequenceNumber(m_NextSendSequenceNumber);

		if (Send(endpoint, std::move(msg), true))
		{
			IncrementSendSequenceNumber();
			return true;
		}

		return false;
	}

	bool Connection::SendInboundSyn(const IPEndpoint& endpoint) noexcept
	{
		LogDbg(L"UDP connection: sending inbound SYN to peer %s for connection %llu (seq# %u)",
			   endpoint.GetString().c_str(), GetID(), m_NextSendSequenceNumber);

		Message msg(Message::Type::Syn, Message::Direction::Outgoing, m_MaxMessageSize);
		msg.SetProtocolVersion(ProtocolVersion::Major, ProtocolVersion::Minor);
		msg.SetConnectionID(GetID());
		msg.SetMessageSequenceNumber(m_NextSendSequenceNumber);
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetPort(m_Socket.GetLocalEndpoint().GetPort());

		if (Send(endpoint, std::move(msg), true))
		{
			IncrementSendSequenceNumber();
			return true;
		}

		return false;
	}

	bool Connection::SendData(const IPEndpoint& endpoint, Buffer&& data) noexcept
	{
		LogDbg(L"UDP connection: sending data to peer %s for connection %llu (seq# %u)",
			   endpoint.GetString().c_str(), GetID(), m_NextSendSequenceNumber);

		Message msg(Message::Type::Data, Message::Direction::Outgoing, m_MaxMessageSize);
		msg.SetMessageSequenceNumber(m_NextSendSequenceNumber);
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetMessageData(std::move(data));

		if (Send(endpoint, std::move(msg), true))
		{
			IncrementSendSequenceNumber();
			return true;
		}

		return false;
	}

	bool Connection::SendStateUpdate(const IPEndpoint& endpoint) noexcept
	{
		LogDbg(L"UDP connection: sending state update to peer %s for connection %llu (seq# %u)",
			   endpoint.GetString().c_str(), GetID(), m_NextSendSequenceNumber);

		Message msg(Message::Type::State, Message::Direction::Outgoing, m_MaxMessageSize);
		msg.SetMessageSequenceNumber(m_NextSendSequenceNumber);
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetStateData(
			Message::StateData{
				.MaxWindowSize = static_cast<UInt32>(m_ReceiveWindowSize),
				.MaxWindowSizeBytes = static_cast<UInt32>(MaxReceiveWindowBytes)
			});

		if (Send(endpoint, std::move(msg), true))
		{
			IncrementSendSequenceNumber();
			return true;
		}

		return false;
	}

	bool Connection::SendPendingAcks() noexcept
	{
		if (m_ReceivePendingAckList.empty()) return true;
		/*
		std::erase_if(m_ReceivePendingAckList, [&](const auto& seqnum)
		{
			return IsMessageSequenceNumberInPreviousWindow(seqnum,
														   m_LastInSequenceReceivedSequenceNumber,
														   m_ReceiveWindowSize);
		});*/

		const auto endpoint = m_ConnectionData->WithSharedLock()->GetPeerEndpoint();

		LogDbg(L"UDP connection: sending acks to peer %s for connection %llu",
			   endpoint.GetString().c_str(), GetID());

		Message msg(Message::Type::EAck, Message::Direction::Outgoing, m_MaxMessageSize);
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

		if (Send(endpoint, std::move(msg), false))
		{
			return true;
		}

		return false;
	}

	void Connection::SendImmediateReset() noexcept
	{
		if (GetStatus() != Status::Handshake && GetStatus() != Status::Connected) return;

		LogDbg(L"UDP connection: sending reset to peer %s for connection %llu",
			   m_ConnectionData->WithSharedLock()->GetPeerEndpoint().GetString().c_str(), GetID());

		const auto endpoint = m_ConnectionData->WithSharedLock()->GetPeerEndpoint();

		Message msg(Message::Type::Reset, Message::Direction::Outgoing, m_MaxMessageSize);
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);

		if (!Send(endpoint, std::move(msg), false))
		{
			LogErr(L"UDP connection: failed to send reset message to peer %s for connection %llu",
				   endpoint.GetString().c_str(), GetID());
		}
	}

	void Connection::IncrementSendSequenceNumber() noexcept
	{
		m_NextSendSequenceNumber = GetNextSequenceNumber(m_NextSendSequenceNumber);
	}

	Message::SequenceNumber Connection::GetNextSequenceNumber(const Message::SequenceNumber current) const noexcept
	{
		if (current == std::numeric_limits<Message::SequenceNumber>::max())
		{
			return 0;
		}
		else return current + 1;
	}

	Message::SequenceNumber Connection::GetPreviousSequenceNumber(const Message::SequenceNumber current) const noexcept
	{
		if (current == 0)
		{
			return std::numeric_limits<Message::SequenceNumber>::max();
		}
		else return current - 1;
	}

	bool Connection::Send(const IPEndpoint& endpoint, Message&& msg, const bool queue) noexcept
	{
		assert(msg.IsValid());

		try
		{
			Buffer data;
			if (msg.Write(data))
			{
				if (queue)
				{
					const auto now = Util::GetCurrentSteadyTime();

					SendQueueItem itm{
						.SequenceNumber = msg.GetMessageSequenceNumber(),
						.IsSyn = (msg.GetType() == Message::Type::Syn),
						.TimeSent = now,
						.TimeResent = now,
						.Data = std::move(data)
					};

					Network::Socket* socket = &m_Socket;
					if (itm.IsSyn && GetType() == PeerConnectionType::Inbound)
					{
						LogWarn(L"Using listener socket to send UDP msg");
						socket = m_ConnectionData->WithUniqueLock()->GetListenerSocket();
					}

					const auto result = socket->SendTo(endpoint, itm.Data);
					if (result.Succeeded()) itm.NumTries = 1;

					m_SendQueue.emplace_back(std::move(itm));

					return true;
				}
				else
				{
					const auto result = m_Socket.SendTo(endpoint, data);
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
		catch (...)
		{
			LogErr(L"UDP connection: exception while sending data for peer %s connection %llu",
				   endpoint.GetString().c_str(), GetID());
		}

		return false;
	}

	bool Connection::SendFromQueue() noexcept
	{
		if (m_SendQueue.empty()) return true;

		m_Statistics.RecalcRetransmissionTimeout();

		const auto endpoint = m_ConnectionData->WithSharedLock()->GetPeerEndpoint();

		const auto rtt_timeout = std::invoke([&]()
		{
			return (GetStatus() < Status::Connected) ? ConnectRetransmissionTimeout : m_Statistics.GetRetransmissionTimeout();
		});

		Size loss{ 0 };

		for (auto it = m_SendQueue.begin(); it != m_SendQueue.end(); ++it)
		{
			if (it->NumTries == 0 || (Util::GetCurrentSteadyTime() - it->TimeResent >= rtt_timeout * it->NumTries))
			{
				if (it->NumTries > 0)
				{
					SLogDbg(SLogFmt(FGBrightCyan) << L"UDP connection: retransmitting (" << it->NumTries <<
							") message with sequence number " <<  it->SequenceNumber << L" (timeout " <<
							rtt_timeout.count() * it->NumTries << L"ms)" << SLogFmt(Default));
					++loss;
				}
				else
				{
					LogDbg(L"UDP connection: sending message with sequence number %u", it->SequenceNumber);
				}

				Network::Socket* socket = &m_Socket;
				if (it->IsSyn && GetType() == PeerConnectionType::Inbound)
				{
					LogWarn(L"UDP connection: using listener socket to send UDP msg");
					socket = m_ConnectionData->WithUniqueLock()->GetListenerSocket();
				}

				const auto result = socket->SendTo(endpoint, it->Data);
				if (result.Succeeded())
				{
					// If data was actually sent, otherwise buffer may
					// temporarily be full/unavailable
					if (*result == it->Data.GetSize())
					{
						// We'll wait for ack or else continue sending
						it->TimeResent = Util::GetCurrentSteadyTime();
						++it->NumTries;
					}
					else
					{
						// We'll try again later
						break;
					}
				}
				else
				{
					LogErr(L"UDP connection: send failed for peer %s connection %llu (%s)",
						   endpoint.GetString().c_str(), GetID(), result.GetErrorString().c_str());
					return false;
				}
			}
		}

		m_Statistics.RecordPacketLoss(loss);
		m_Statistics.RecordSendWindowSizeStats();

		DbgInvoke([&]()
		{
			if (loss > 0)
			{
				LogWarn(L"UDP connection: retransmitted %zu packets (queue size %zu, send window size %zu, RTT %jdms)",
						loss, m_SendQueue.size(), GetSendWindowSize(), m_Statistics.GetRetransmissionTimeout().count());
			}
		});

		return true;
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
					LogErr(L"UDP connection: exception on socket for connection %llu", GetID());

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

							ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());

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
						else LogErr(L"UDP connection: received invalid SYN message from peer %s; unexpected connection ID",
									endpoint.GetString().c_str());
					}
					else LogErr(L"UDP connection: could not accept connection from peer %s; unsupported UDP protocol version",
								endpoint.GetString().c_str());
				}
				else LogErr(L"UDP connection: received handshake repsonse from unexpected IP address %s",
							endpoint.GetString().c_str());
			}
			else LogErr(L"UDP connection: received invalid message from peer %s", endpoint.GetString().c_str());
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
			else LogErr(L"UDP connection: received invalid message from peer %s", endpoint.GetString().c_str());
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
			LogErr(L"UDP connection: received invalid message from peer %s", endpoint.GetString().c_str());
		}

		return false;
	}

	bool Connection::ProcessReceivedMessageConnected(const IPEndpoint& endpoint, Message&& msg) noexcept
	{
		switch (msg.GetType())
		{
			case Message::Type::Data:
			{
				LogDbg(L"UDP connection: received data message from peer %s (seq# %u)",
					   endpoint.GetString().c_str(), msg.GetMessageSequenceNumber());

				if (IsExpectedMessageSequenceNumber(msg.GetMessageSequenceNumber()))
				{
					assert(msg.HasAck());
					ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());

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
				LogDbg(L"UDP connection: received state message from peer %s (seq# %u)",
					   endpoint.GetString().c_str(), msg.GetMessageSequenceNumber());

				assert(msg.HasAck());
				ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());
				
				if (AckReceivedMessage(msg.GetMessageSequenceNumber()))
				{
					const auto state_data = msg.GetStateData();
					m_PeerAdvReceiveWindowSize = state_data.MaxWindowSize;
					m_PeerAdvReceiveWindowSizeBytes = state_data.MaxWindowSizeBytes;

					RecalcPeerReceiveWindowSize();
				
					return AddToReceiveQueue(
						ReceiveQueueItem{
							.SequenceNumber = msg.GetMessageSequenceNumber()
						});
				}
				break;
			}
			case Message::Type::EAck:
			{
				LogDbg(L"UDP connection: received ack message from peer %s", endpoint.GetString().c_str());

				assert(msg.HasAck());
				ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());
				ProcessReceivedAcks(msg.GetAckSequenceNumbers());
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
				LogDbg(L"UDP connection: received reset message from peer %s", endpoint.GetString().c_str());

				m_ConnectionData->WithUniqueLock()->SetCloseRequest();
				SetCloseCondition(CloseCondition::PeerCloseRequest);
				return true;
			}
			default:
			{
				LogErr(L"UDP connection: received unknown message on connection %llu", GetID());
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

	bool Connection::AckSentMessage(const Message::SequenceNumber seqnum) noexcept
	{
		auto it = std::find_if(m_SendQueue.begin(), m_SendQueue.end(), [&](const auto& itm)
		{
			return (itm.SequenceNumber == seqnum);
		});

		if (it != m_SendQueue.end())
		{
			LogDbg(L"UDP connection: received ack for message with seq# %u", seqnum);

			if (!it->Acked)
			{
				it->Acked = true;
				it->TimeAcked = Util::GetCurrentSteadyTime();

				if (it->NumTries == 1)
				{
					m_Statistics.RecordPacketRTT(std::chrono::duration_cast<std::chrono::milliseconds>(it->TimeAcked - it->TimeResent));
				}

				return true;
			}
		}

		return false;
	}

	void Connection::PurgeAckedMessages() noexcept
	{
		// Remove all acked messages from the front of the list
		// to make room for new messages in the send window
		for (auto it = m_SendQueue.begin(); it != m_SendQueue.end();)
		{
			if (it->Acked)
			{
				it = m_SendQueue.erase(it);
			}
			else break;
		}
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

	void Connection::ProcessReceivedInSequenceAck(const Message::SequenceNumber seqnum) noexcept
	{
		if (m_LastInSequenceAckedSequenceNumber == seqnum) return;

		m_LastInSequenceAckedSequenceNumber = seqnum;

		auto it = std::find_if(m_SendQueue.begin(), m_SendQueue.end(), [&](const auto& itm)
		{
			return (itm.SequenceNumber == seqnum);
		});

		if (it != m_SendQueue.end())
		{
			Size num_acks{ 0 };

			for (auto it2 = m_SendQueue.begin();;)
			{
				if (it2->NumTries > 0)
				{
					if (!it2->Acked)
					{
						it2->Acked = true;
						it2->TimeAcked = Util::GetCurrentSteadyTime();

						if (it2->NumTries == 1)
						{
							m_Statistics.RecordPacketRTT(std::chrono::duration_cast<std::chrono::milliseconds>(it2->TimeAcked - it2->TimeResent));
						}

						++num_acks;
					}
				}

				if (it2->SequenceNumber == it->SequenceNumber) break;
				else ++it2;
			}

			if (num_acks > 0)
			{
				m_Statistics.RecordPacketAck(num_acks);
				PurgeAckedMessages();
			}
		}
	}

	void Connection::ProcessReceivedAcks(const Vector<Message::SequenceNumber>& acks) noexcept
	{
		Size num_acks{ 0 };

		for (const auto ack_num : acks)
		{
			if (AckSentMessage(ack_num))
			{
				++num_acks;
			}
		}

		if (num_acks > 0)
		{
			m_Statistics.RecordPacketAck(num_acks);
			PurgeAckedMessages();
		}
	}

	bool Connection::SendPendingSocketData() noexcept
	{
		try
		{
			m_Statistics.RecalcSendWindowSize();

			auto connection_data = m_ConnectionData->WithUniqueLock();

			Message msg(Message::Type::Data, Message::Direction::Outgoing, m_MaxMessageSize);

			while (HasAvailableSendWindowSpace() && connection_data->GetSendBuffer().GetReadSize() > 0)
			{
				auto read_size = connection_data->GetSendBuffer().GetReadSize();
				if (read_size > msg.GetMaxMessageDataSize()) read_size = msg.GetMaxMessageDataSize();

				Buffer buffer(read_size);
				if (connection_data->GetSendBuffer().Read(buffer) == read_size)
				{
					if (!SendData(connection_data->GetPeerEndpoint(), std::move(buffer)))
					{
						return false;
					}
				}
				else return false;
			}
		}
		catch (...) { return false; }

		return true;
	}

	bool Connection::ReceivePendingSocketData() noexcept
	{
		if (m_ReceiveQueue.empty()) return true;

		auto next_itm = m_ReceiveQueue.find(GetNextSequenceNumber(m_LastInSequenceReceivedSequenceNumber));
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

				next_itm = m_ReceiveQueue.find(GetNextSequenceNumber(m_LastInSequenceReceivedSequenceNumber));
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

				connection_data.LockShared();

				if (success) success = SetStatus(Status::Handshake);

				if (!success) close_condition = CloseCondition::GeneralFailure;
			}

			// Close requested by socket
			if (connection_data->HasCloseRequest())
			{
				close_condition = CloseCondition::LocalCloseRequest;
			}
		}

		if (close_condition != CloseCondition::None)
		{
			if (close_condition == CloseCondition::LocalCloseRequest)
			{
				SendImmediateReset();
			}

			SetCloseCondition(close_condition);
		}
	}

	void Connection::RecalcPeerReceiveWindowSize() noexcept
	{
		const auto wndsize = std::max(MinReceiveWindowSize, m_PeerAdvReceiveWindowSizeBytes / m_MaxMessageSize);
		m_PeerReceiveWindowSize = std::min(wndsize, m_PeerAdvReceiveWindowSize);

		LogWarn(L"UDP connection: PeerAdvReceiveWindowSizeBytes: %zu - PeerAdvReceiveWindowSize: %zu - PeerReceiveWindowSize: %zu",
				m_PeerAdvReceiveWindowSizeBytes, m_PeerAdvReceiveWindowSize, m_PeerReceiveWindowSize);
	}

	Size Connection::GetSendWindowSize() const noexcept
	{
		return std::min(m_Statistics.GetSendWindowSize(), m_PeerReceiveWindowSize);
	}

	bool Connection::HasAvailableSendWindowSpace() const noexcept
	{
		return (m_SendQueue.size() < GetSendWindowSize());
	}
}