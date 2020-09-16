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
										 (af == Network::IP::AddressFamily::IPv4) ? IPAddress::AnyIPv4() : IPAddress::AnyIPv6(),
										 0), nat_traversal))
			{
				m_ConnectionData = std::make_shared<ConnectionData_ThS>(&m_Socket.GetEvent());
				
				ResetMTU();

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

		if (!m_ConnectionData->WithSharedLock()->HasCloseRequest())
		{
			SendImmediateReset();
		}

		DiscardReturnValue(SetStatus(Status::Closed));
	}

	void Connection::OnLocalIPInterfaceChanged() noexcept
	{
		ResetMTU();

		// Send immediate keepalive to let the peer know of
		// address change in order to update endpoint
		DiscardReturnValue(SendKeepAlive());
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
				assert(prev_status == Status::Handshake || prev_status == Status::Suspended);
				if (prev_status == Status::Handshake || prev_status == Status::Suspended)
				{
					LogDbg(L"UDP connection: connection %llu has entered Connected state", GetID());
					m_Status = status;
					ResetKeepAliveTimeout();
				}
				else success = false;
				break;
			case Status::Suspended:
				assert(prev_status == Status::Connected);
				if (prev_status == Status::Connected)
				{
					LogDbg(L"UDP connection: connection %llu has entered Suspended state", GetID());
					m_Status = status;
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

		switch (GetStatus())
		{
			case Status::Handshake:
			{
				if (Util::GetCurrentSteadyTime() - m_LastStatusChangeSteadyTime >= ConnectTimeout)
				{
					LogDbg(L"UDP connection: connect timed out for connection %llu", GetID());

					SetCloseCondition(CloseCondition::TimedOutError);
				}

				if (!m_SendQueue.Process())
				{
					SetCloseCondition(CloseCondition::SendError);
				}
				break;
			}
			case Status::Connected:
			{
				if (!m_SendQueue.Process())
				{
					SetCloseCondition(CloseCondition::SendError);
				}

				if (!CheckKeepAlive() || !ProcessMTUDiscovery())
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
			case Status::Suspended:
			{
				if (!CheckKeepAlive())
				{
					SetCloseCondition(CloseCondition::GeneralFailure);
				}
				break;
			}
			default:
			{
				break;
			}
		}

		if (!SendPendingAcks())
		{
			SetCloseCondition(CloseCondition::SendError);
		}

		/*if (!SendPendingNAcks())
		{
			SetCloseCondition(CloseCondition::SendError);
		}*/
	}

	bool Connection::CheckKeepAlive() noexcept
	{
		const auto now = Util::GetCurrentSteadyTime();
		if (now - m_LastSendSteadyTime >= m_KeepAliveTimeout)
		{
			ResetKeepAliveTimeout();

			return SendKeepAlive();
		}

		if (GetStatus() == Status::Connected)
		{
			if (now - m_LastReceiveSteadyTime >= MaxKeepAliveTimeout)
			{
				if (!SetStatus(Status::Suspended))
				{
					return false;
				}
			}
		}

		return true;
	}

	void Connection::ResetKeepAliveTimeout() noexcept
	{
		m_KeepAliveTimeout = std::chrono::seconds(Random::GetPseudoRandomNumber(MinKeepAliveTimeout.count(),
																				MaxKeepAliveTimeout.count()));
	}

	bool Connection::ProcessMTUDiscovery() noexcept
	{
		if (!m_MTUDiscovery) return true;

		const auto status = m_MTUDiscovery->Process();
		switch (status)
		{
			case MTUDiscovery::Status::Finished:
			case MTUDiscovery::Status::Failed:
			{
				const auto new_mtu = m_MTUDiscovery->GetMaxMessageSize();
				m_MTUDiscovery.reset();

				return OnMTUUpdate(new_mtu);
			}
			default:
			{
				break;
			}
		}

		return true;
	}

	void Connection::ResetMTU() noexcept
	{
		try
		{
			m_MTUDiscovery = std::make_unique<MTUDiscovery>(*this);
			
			if (OnMTUUpdate(m_MTUDiscovery->GetMaxMessageSize()))
			{
				return;
			}
		}
		catch (...)
		{
			LogErr(L"UDP connection: MTU reset failed for connection %llu due to exception", GetID());
		}

		SetCloseCondition(CloseCondition::GeneralFailure);
	}

	bool Connection::OnMTUUpdate(const Size mtu) noexcept
	{
		assert(mtu >= MTUDiscovery::MinMessageSize);

		m_SendQueue.SetMaxMessageSize(mtu);
		
		m_ReceiveWindowSize = std::min(MaxReceiveWindowItemSize, MaxReceiveWindowBytes / mtu);
		m_ReceiveWindowSize = std::max(MinReceiveWindowItemSize, m_ReceiveWindowSize);

#ifdef UDPCON_DEBUG
		SLogInfo(SLogFmt(FGCyan) << L"UDP connection: maximum message size is now " <<
				 mtu << L" bytes, receive window size is " << m_ReceiveWindowSize <<
				 L" for connection " << GetID() << SLogFmt(Default));
#endif

		if (GetStatus() == Status::Connected)
		{
			// If we're connected let the peer know about the new receive window size
			return SendStateUpdate();
		}

		return true;
	}

	bool Connection::SendOutboundSyn() noexcept
	{
		Dbg(L"UDP connection: sending outbound SYN on connection %llu (seq# %u)",
			GetID(), m_SendQueue.GetNextSendSequenceNumber());

		Message msg(Message::Type::Syn, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetProtocolVersion(ProtocolVersion::Major, ProtocolVersion::Minor);
		msg.SetConnectionID(GetID());
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());

		if (Send(std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send outbound SYN on connection %llu", GetID());
		}

		return false;
	}

	bool Connection::SendInboundSyn() noexcept
	{
		Dbg(L"UDP connection: sending inbound SYN on connection %llu (seq# %u)",
			GetID(), m_SendQueue.GetNextSendSequenceNumber());

		Message msg(Message::Type::Syn, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetProtocolVersion(ProtocolVersion::Major, ProtocolVersion::Minor);
		msg.SetConnectionID(GetID());
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetPort(m_Socket.GetLocalEndpoint().GetPort());

		if (Send(std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send inbound SYN on connection %llu", GetID());
		}

		return false;
	}

	bool Connection::SendData(Buffer&& data) noexcept
	{
		Dbg(L"UDP connection: sending data on connection %llu (seq# %u)",
			GetID(), m_SendQueue.GetNextSendSequenceNumber());

		Message msg(Message::Type::Data, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetMessageData(std::move(data));

		if (Send(std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send data on connection %llu", GetID());
		}

		return false;
	}

	bool Connection::SendStateUpdate() noexcept
	{
		Dbg(L"UDP connection: sending state update on connection %llu (seq# %u)",
			GetID(), m_SendQueue.GetNextSendSequenceNumber());

		Message msg(Message::Type::State, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetStateData(
			Message::StateData{
				.MaxWindowSize = static_cast<UInt32>(m_ReceiveWindowSize),
				.MaxWindowSizeBytes = static_cast<UInt32>(MaxReceiveWindowBytes)
			});

		if (Send(std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send state update on connection %llu", GetID());
		}

		return false;
	}

	bool Connection::SendPendingAcks() noexcept
	{
		/*
		std::erase_if(m_ReceivePendingAckList, [&](const auto& seqnum)
		{
			return IsMessageSequenceNumberInPreviousWindow(seqnum,
														   m_LastInSequenceReceivedSequenceNumber,
														   m_ReceiveWindowSize);
		});*/

		Dbg(L"UDP connection: sending acks on connection %llu", GetID());

		for (auto it = m_ReceivePendingAckList.begin(); it != m_ReceivePendingAckList.end();)
		{
			auto begin = *it;
			auto end = begin;

			auto it2 = std::next(it, 1);
			if (it2 == m_ReceivePendingAckList.end())
			{
				it = m_ReceivePendingAckList.erase(it);
			}
			else
			{
				auto next = begin;
				for (; it2 != m_ReceivePendingAckList.end();)
				{
					if (next < std::numeric_limits<Message::SequenceNumber>::max() &&
						*it2 == next + 1)
					{
						++it2;
						++next;
					}
					else
					{
						break;
					}
				}

				m_ReceivePendingAckList.erase(m_ReceivePendingAckList.begin(), it2);
				it = m_ReceivePendingAckList.begin();

				end = next;
			}

			assert(begin <= end);

			//LogWarn(L"UDP connection: adding ack range %u - %u", begin, end);

			m_ReceivePendingAckRanges.emplace_back(
				Message::AckRange{
					.Begin = begin,
					.End = end
				});
		}

		while (!m_ReceivePendingAckRanges.empty())
		{
			Message msg(Message::Type::EAck, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
			msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);

			const auto max_num_acks = msg.GetMaxAckRangesPerMessage();
			if (m_ReceivePendingAckRanges.size() <= max_num_acks)
			{
				msg.SetAckRanges(std::move(m_ReceivePendingAckRanges));
			}
			else
			{
				Vector<Message::AckRange> temp_acks;
				const auto last = m_ReceivePendingAckRanges.begin() + max_num_acks;
				std::copy(m_ReceivePendingAckRanges.begin(), last, std::back_inserter(temp_acks));
				msg.SetAckRanges(std::move(temp_acks));
				m_ReceivePendingAckRanges.erase(m_ReceivePendingAckRanges.begin(), last);
			}

			if (!Send(std::move(msg)))
			{
				LogErr(L"UDP connection: failed to send acks on connection %llu", GetID());
				return false;
			}
		}

		return true;
	}

	bool Connection::SendPendingNAcks() noexcept
	{
		if (m_ReceivePendingNAckList.empty() && m_ReceiveCumulativeAckRequired)
		{
			Dbg(L"UDP connection: sending cummulative ack on connection %llu", GetID());

			Message msg(Message::Type::NAck, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
			msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
			
			if (!Send(std::move(msg)))
			{
				LogErr(L"UDP connection: failed to send nacks on connection %llu", GetID());
				return false;
			}

			m_ReceiveCumulativeAckRequired = false;
		}
		else if (!m_ReceivePendingNAckList.empty())
		{
			Dbg(L"UDP connection: sending nacks on connection %llu", GetID());

			while (!m_ReceivePendingNAckList.empty())
			{
				Message msg(Message::Type::NAck, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
				msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);

				const auto max_num_nacks = msg.GetMaxNAckRangesPerMessage();
				if (m_ReceivePendingNAckList.size() <= max_num_nacks)
				{
					msg.SetNAckRanges(std::move(m_ReceivePendingNAckList));
				}
				else
				{
					Vector<Message::NAckRange> temp_acks;
					const auto last = m_ReceivePendingNAckList.begin() + max_num_nacks;
					std::copy(m_ReceivePendingNAckList.begin(), last, std::back_inserter(temp_acks));
					msg.SetNAckRanges(std::move(temp_acks));
					m_ReceivePendingNAckList.erase(m_ReceivePendingNAckList.begin(), last);
				}

				if (!Send(std::move(msg)))
				{
					LogErr(L"UDP connection: failed to send nacks on connection %llu", GetID());
					return false;
				}
			}

			m_LastNAckSteadyTime = Util::GetCurrentSteadyTime();
		}

		return true;
	}

	bool Connection::SendKeepAlive() noexcept
	{
		Dbg(L"UDP connection: sending keepalive on connection %llu", GetID());

		Message msg(Message::Type::Null, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
		msg.SetMessageData(Random::GetPseudoRandomBytes(Random::GetPseudoRandomNumber(0, msg.GetMaxMessageDataSize())));

		if (Send(std::move(msg)))
		{
			return true;
		}
		else
		{
			LogErr(L"UDP connection: failed to send keepalive on connection %llu", GetID());
		}

		return false;
	}

	void Connection::SendImmediateReset() noexcept
	{
		if (GetStatus() != Status::Handshake && GetStatus() != Status::Connected) return;

		Dbg(L"UDP connection: sending reset on connection %llu", GetID());

		Message msg(Message::Type::Reset, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());

		if (!Send(std::move(msg)))
		{
			LogErr(L"UDP connection: failed to send reset on connection %llu", GetID());
		}
	}

	bool Connection::Send(Message&& msg) noexcept
	{
		try
		{
			Buffer data;
			if (msg.Write(data))
			{
				const auto now = Util::GetCurrentSteadyTime();

				// Messages with sequence numbers need to be tracked
				// for ack and go into the send queue.
				if (msg.HasSequenceNumber())
				{
					SendQueue::Item itm{
						.MessageType = msg.GetType(),
						.SequenceNumber = msg.GetMessageSequenceNumber(),
						.TimeSent = now,
						.TimeResent = now,
						.Data = std::move(data)
					};

					return m_SendQueue.Add(std::move(itm));
				}
				else
				{
					// Messages without sequence numbers are sent in one try
					// and we don't care if they arrive or not
					const auto result = Send(now, data, false);
					if (result.Succeeded())
					{
						return true;
					}
					else
					{
						LogErr(L"UDP connection: send failed on connection %llu (%s)",
							   GetID(), result.GetErrorString().c_str());
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"UDP connection: exception while sending data on connection %llu - %s",
				   GetID(), Util::ToStringW(e.what()).c_str());
		}

		return false;
	}

	Result<Size> Connection::Send(const SteadyTime& now, const Buffer& data,
								  const bool use_listener_socket) noexcept
	{
		m_LastSendSteadyTime = now;

		Network::Socket* s = &m_Socket;
		if (use_listener_socket)
		{
			LogWarn(L"UDP connection: using listener socket to send UDP msg");
			s = m_ConnectionData->WithUniqueLock()->GetListenerSocket();
		}

		auto result = s->SendTo(m_PeerEndpoint, data);
		if (result.Failed())
		{
			if (result.GetErrorCode().category() == std::system_category() &&
				result.GetErrorCode().value() == 10065)
			{
				LogDbg(L"UDP connection: failed to send data on connection %llu (host unreachable)", GetID());

				// Host unreachable error; this may occur when the peer is temporarily
				// not online due to changing IP address or network. In this case
				// we will keep retrying until we get a message from the peer
				// with an updated endpoint. We return success with 0 bytes sent and
				// suspend the socket until we hear from the peer again.
				if (SetStatus(Status::Suspended))
				{
					return 0;
				}
				else
				{
					SetCloseCondition(CloseCondition::GeneralFailure);
					return ResultCode::Failed;
				}
			}
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
		m_LastReceiveSteadyTime = Util::GetCurrentSteadyTime();

		switch (GetStatus())
		{
			case Status::Handshake:
				return ProcessReceivedDataHandshake(endpoint, buffer);
			case Status::Suspended:
				if (!SetStatus(Status::Connected))
				{
					SetCloseCondition(CloseCondition::GeneralFailure);
					return false;
				}
				[[fallthrough]];
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
									// Endpoint update with new received port
									m_PeerEndpoint = IPEndpoint(endpoint.GetProtocol(), endpoint.GetIPAddress(), msg.GetPort());

									m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
									{
										// Endpoint update
										connection_data.SetLocalEndpoint(m_Socket.GetLocalEndpoint());
										connection_data.SetPeerEndpoint(m_PeerEndpoint);
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

	bool Connection::CheckEndpointChange(const IPEndpoint& endpoint) noexcept
	{
		if (m_PeerEndpoint != endpoint)
		{
			m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
			{
				connection_data.SetPeerEndpoint(endpoint);
			});

			LogWarn(L"UDP connection: peer endpoint changed from %s to %s for connection %llu",
					m_PeerEndpoint.GetString().c_str(), endpoint.GetString().c_str(), GetID());

			m_PeerEndpoint = endpoint;
		}

		return true;
	}

	bool Connection::ProcessReceivedDataConnected(const IPEndpoint& endpoint, const Buffer& buffer) noexcept
	{
		Message msg(Message::Type::Unknown, Message::Direction::Incoming);
		if (msg.Read(buffer) && msg.IsValid())
		{
			if (ProcessReceivedMessageConnected(endpoint, std::move(msg)))
			{
				return CheckEndpointChange(endpoint);
			}
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
			case Message::Type::State:
			{
				Dbg(L"UDP connection: received data/state message from peer %s (seq# %u) on connection %llu",
					endpoint.GetString().c_str(), msg.GetMessageSequenceNumber(), GetID());

				const auto window = GetMessageSequenceNumberWindow(msg.GetMessageSequenceNumber());
				switch (window)
				{
					case ReceiveWindow::Current:
					{
						m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());

						if (AckReceivedMessage(msg.GetMessageSequenceNumber()))
						{
							return AddToReceiveQueue(std::move(msg));
						}
					}
					case ReceiveWindow::Previous:
					{
						// May have been retransmitted due to delays; send an ack and drop message
						DiscardReturnValue(AckReceivedMessage(msg.GetMessageSequenceNumber()));
					}
					case ReceiveWindow::Unknown:
					{
						// Drop message
						break;
					}
					default:
					{
						assert(false);
						break;
					}
				}
				
				return true;
			}
			case Message::Type::EAck:
			{
				Dbg(L"UDP connection: received ack message from peer %s on connection %llu",
					endpoint.GetString().c_str(), GetID());

				m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());
				m_SendQueue.ProcessReceivedAcks(msg.GetAckRanges());
				return true;
			}
			case Message::Type::NAck:
			{
				Dbg(L"UDP connection: received nack message from peer %s on connection %llu",
					endpoint.GetString().c_str(), GetID());

				m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());
				m_SendQueue.ProcessReceivedNAcks(msg.GetNAckRanges());
				return true;
			}
			case Message::Type::MTUD:
			{
				if (!msg.HasAck())
				{
					MTUDiscovery::AckReceivedMessage(*this, msg.GetMessageSequenceNumber());
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

	bool Connection::AddToReceiveQueue(Message&& msg) noexcept
	{
		try
		{
			m_ReceiveQueue.emplace(msg.GetMessageSequenceNumber(), std::move(msg));
			return true;
		}
		catch (...) {}

		return false;
	}

	Connection::ReceiveWindow Connection::GetMessageSequenceNumberWindow(const Message::SequenceNumber seqnum) noexcept
	{
		if (IsMessageSequenceNumberInCurrentWindow(seqnum, m_LastInSequenceReceivedSequenceNumber,
												   m_ReceiveWindowSize))
		{
			return ReceiveWindow::Current;
		}

		if (IsMessageSequenceNumberInPreviousWindow(seqnum, m_LastInSequenceReceivedSequenceNumber,
													m_ReceiveWindowSize))
		{
			return ReceiveWindow::Previous;
		}

		return ReceiveWindow::Unknown;
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
		/*m_ReceiveCumulativeAckRequired = true;

		return true;
		*/
		try
		{
			m_ReceivePendingAckList.emplace(seqnum);
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
					if (!SendData(std::move(buffer)))
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
			//return ProcessNAcks();
		}

		auto connection_data = m_ConnectionData->WithUniqueLock();

		auto rcv_event = false;

		while (next_itm != m_ReceiveQueue.end())
		{
			auto remove = false;

			auto& msg = next_itm->second;

			if (msg.GetType() == Message::Type::Data)
			{
				if (connection_data->GetReceiveBuffer().GetWriteSize() >= msg.GetMessageData().GetSize())
				{
					if (connection_data->GetReceiveBuffer().Write(msg.GetMessageData()) == msg.GetMessageData().GetSize())
					{
						rcv_event = true;
						remove = true;
					}
					else return false;
				}
				else break;
			}
			else if (msg.GetType() == Message::Type::State)
			{
				const auto state_data = msg.GetStateData();
				m_SendQueue.SetPeerAdvertisedReceiveWindowSizes(state_data.MaxWindowSize, state_data.MaxWindowSizeBytes);

				remove = true;
			}
			else
			{
				assert(false);
				LogErr(L"UDP connection: unhandled message in receive queue");
				return false;
			}

			if (remove)
			{
				m_LastInSequenceReceivedSequenceNumber = msg.GetMessageSequenceNumber();
				m_ReceiveQueue.erase(next_itm);
			}

			next_itm = m_ReceiveQueue.find(Message::GetNextSequenceNumber(m_LastInSequenceReceivedSequenceNumber));
		}

		if (rcv_event)
		{
			connection_data->SetRead(true);
			connection_data->SignalReceiveEvent();
		}

		return true;
	}

	bool Connection::ProcessNAcks() noexcept
	{
		if (Util::GetCurrentSteadyTime() - m_LastNAckSteadyTime < 2ms)
		{
			return SendPendingNAcks();
		}

		auto curseqnum = m_LastInSequenceReceivedSequenceNumber;
		
		for (auto it = m_ReceiveQueue.begin(); it != m_ReceiveQueue.end(); ++it)
		{
			const auto dif = it->second.GetMessageSequenceNumber() - curseqnum;
			if (dif > 0 && dif < MaxReceiveWindowItemSize)
			{
				//LogWarn(L"UDP connection: adding nack range %u - %u",
				//		curseqnum, it->second.GetMessageSequenceNumber());

				m_ReceivePendingNAckList.emplace_back(
					Message::NAckRange{
						.Begin = curseqnum,
						.End = it->second.GetMessageSequenceNumber()
					});
			}
			else curseqnum = it->second.GetMessageSequenceNumber();
		}

		return SendPendingNAcks();
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

				// Update endpoint with the one we should connect to
				m_PeerEndpoint = connection_data->GetPeerEndpoint();

				connection_data.UnlockShared();

				switch (GetType())
				{
					case PeerConnectionType::Inbound:
						success = SendInboundSyn();
						break;
					case PeerConnectionType::Outbound:
						success = SendOutboundSyn();
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
				SendImmediateReset();

				close_condition = CloseCondition::LocalCloseRequest;
			}
		}

		if (close_condition != CloseCondition::None)
		{
			SetCloseCondition(close_condition);
		}
	}
}