// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnection.h"
#include "..\..\Crypto\Crypto.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	Connection::Connection(const Settings_CThS& settings, Access::Manager& accessmgr, const PeerConnectionType type,
						   const ConnectionID id, const Message::SequenceNumber seqnum,
						   const ProtectedBuffer& shared_secret) noexcept :
		m_Settings(settings), m_AccessManager(accessmgr), m_Type(type), m_ID(id),
		m_SymmetricKeys(shared_secret), m_LastInSequenceReceivedSequenceNumber(seqnum)
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
				   GetID(), Util::ToStringW(e.what()).c_str());
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
					m_Status = status;
					ResetKeepAliveTimeout(GetSettings());
				}
				else success = false;
				break;
			case Status::Suspended:
				assert(prev_status == Status::Connected);
				if (prev_status == Status::Connected) m_Status = status;
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
				case CloseCondition::PeerNotAllowed:
					socket_error_code = WSAEACCES;
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

		const auto& settings = GetSettings();
		
		assert(settings.Local.SuspendTimeout > SuspendTimeoutMargin);
		const auto max_keepalive_timeout = settings.Local.SuspendTimeout - SuspendTimeoutMargin;

		switch (GetStatus())
		{
			case Status::Handshake:
			{
				if (Util::GetCurrentSteadyTime() - m_LastStatusChangeSteadyTime >= settings.UDP.ConnectTimeout)
				{
					LogDbg(L"UDP connection: handshake timed out for connection %llu", GetID());

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

				if (!CheckKeepAlive(settings) || !ProcessMTUDiscovery())
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

				if (Util::GetCurrentSteadyTime() - m_LastReceiveSteadyTime >= max_keepalive_timeout)
				{
					if (!Suspend())
					{
						SetCloseCondition(CloseCondition::GeneralFailure);
					}
				}
				break;
			}
			case Status::Suspended:
			{
				const auto suspended_steadytime = m_LastReceiveSteadyTime + max_keepalive_timeout;
				if (Util::GetCurrentSteadyTime() - suspended_steadytime >= settings.Local.MaxSuspendDuration)
				{
					// Connection has been in the suspended state for
					// too long so we disconnect it now
					LogDbg(L"UDP connection: suspend duration timed out for connection %llu", GetID());

					SetCloseCondition(CloseCondition::TimedOutError);
				}
				else
				{
					// Try to make contact again
					if (!CheckKeepAlive(settings))
					{
						SetCloseCondition(CloseCondition::GeneralFailure);
					}
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
	}

	bool Connection::CheckKeepAlive(const Settings& settings) noexcept
	{
		const auto now = Util::GetCurrentSteadyTime();
	
		if (now - m_LastSendSteadyTime >= m_KeepAliveTimeout)
		{
			ResetKeepAliveTimeout(settings);

			return SendKeepAlive();
		}

		return true;
	}

	void Connection::ResetKeepAliveTimeout(const Settings& settings) noexcept
	{
		const auto max_keepalive_timeout = settings.Local.SuspendTimeout - SuspendTimeoutMargin;
		m_KeepAliveTimeout = std::chrono::seconds(Random::GetPseudoRandomNumber(0, max_keepalive_timeout.count()));
	}

	bool Connection::Suspend() noexcept
	{
		assert(GetStatus() == Status::Connected);

		LogDbg(L"UDP connection: connection %llu entering Suspended state", GetID());

		if (SetStatus(Status::Suspended))
		{
			auto connection_data = m_ConnectionData->WithUniqueLock();
			connection_data->SetSuspended(true);
			connection_data->SignalReceiveEvent();

			return true;
		}

		return false;
	}

	bool Connection::Resume() noexcept
	{
		assert(GetStatus() == Status::Suspended);

		LogDbg(L"UDP connection: connection %llu resuming from Suspended state", GetID());

		if (SetStatus(Status::Connected))
		{
			auto connection_data = m_ConnectionData->WithUniqueLock();
			connection_data->SetSuspended(false);
			connection_data->SignalReceiveEvent();

			return true;
		}

		return false;
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
		catch (const std::exception& e)
		{
			LogErr(L"UDP connection: MTU reset failed for connection %llu due to exception - %s",
				   GetID(), Util::ToStringW(e.what()).c_str());
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
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());
		msg.SetSynData(
			Message::SynData{
				.ProtocolVersionMajor = ProtocolVersion::Major,
				.ProtocolVersionMinor = ProtocolVersion::Minor,
				.ConnectionID = GetID(),
				.Port = static_cast<UInt16>(Random::GetPseudoRandomNumber())
			});

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
		msg.SetMessageSequenceNumber(m_SendQueue.GetNextSendSequenceNumber());
		msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);
		msg.SetSynData(
			Message::SynData{
				.ProtocolVersionMajor = ProtocolVersion::Major,
				.ProtocolVersionMinor = ProtocolVersion::Minor,
				.ConnectionID = GetID(),
				.Port = m_Socket.GetLocalEndpoint().GetPort()
			});

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
		try
		{
			for (auto it = m_ReceivePendingAckSet.begin(); it != m_ReceivePendingAckSet.end();)
			{
				auto begin = *it;
				auto end = begin;

				auto it2 = std::next(it, 1);
				if (it2 == m_ReceivePendingAckSet.end())
				{
					it = m_ReceivePendingAckSet.erase(it);
				}
				else
				{
					auto next = begin;
					for (; it2 != m_ReceivePendingAckSet.end();)
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

					m_ReceivePendingAckSet.erase(m_ReceivePendingAckSet.begin(), it2);
					it = m_ReceivePendingAckSet.begin();

					end = next;
				}

				assert(begin <= end);

				m_ReceivePendingAckRanges.emplace_back(
					Message::AckRange{
						.Begin = begin,
						.End = end
					});
			}

			while (!m_ReceivePendingAckRanges.empty())
			{
				Dbg(L"UDP connection: sending acks on connection %llu", GetID());

				Message msg(Message::Type::EAck, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
				msg.SetMessageAckNumber(m_LastInSequenceReceivedSequenceNumber);

				const auto max_num_ranges = msg.GetMaxAckRangesPerMessage();
				if (m_ReceivePendingAckRanges.size() <= max_num_ranges)
				{
					msg.SetAckRanges(std::move(m_ReceivePendingAckRanges));
				}
				else
				{
					Vector<Message::AckRange> temp_ack_ranges;
					const auto last = m_ReceivePendingAckRanges.begin() + max_num_ranges;
					std::copy(m_ReceivePendingAckRanges.begin(), last, std::back_inserter(temp_ack_ranges));
					m_ReceivePendingAckRanges.erase(m_ReceivePendingAckRanges.begin(), last);
					
					msg.SetAckRanges(std::move(temp_ack_ranges));
				}

				if (!Send(std::move(msg)))
				{
					LogErr(L"UDP connection: failed to send acks on connection %llu", GetID());
					return false;
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"UDP connection: an exception occured while sending acks on connection %llu - %s",
				   GetID(), Util::ToStringW(e.what()).c_str());
			return false;
		}

		return true;
	}

	bool Connection::SendKeepAlive() noexcept
	{
		Dbg(L"UDP connection: sending keepalive on connection %llu", GetID());

		Message msg(Message::Type::Null, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());

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
		if (GetStatus() != Status::Connected) return;

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
			if (msg.Write(data, m_SymmetricKeys))
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
			LogErr(L"UDP connection: an exception occured while sending data on connection %llu - %s",
				   GetID(), Util::ToStringW(e.what()).c_str());
		}

		return false;
	}

	Result<Size> Connection::Send(const SteadyTime& now, const Buffer& data, const bool use_listener_socket) noexcept
	{
		m_LastSendSteadyTime = now;

		if (use_listener_socket)
		{
			// Need to use the listener socket to send syn replies for inbound connections.
			// This is because if the peer is behind NAT, it will expect a reply from the same
			// IP and port it sent a syn to which is to our listener socket. Our syn will contain
			// the new port to which the peer should send subsequent messages to.

			// Only in handshake state
			assert(GetStatus() < Status::Connected);

			// Should still have listener send queue (only in handshake state)
			assert(m_ConnectionData->WithSharedLock()->HasListenerSendQueue());

			try
			{
				m_ConnectionData->WithUniqueLock()->GetListenerSendQueue().
					WithUniqueLock()->emplace(
						Listener::SendQueueItem{
							.Endpoint = m_PeerEndpoint,
							.Data = data
						});

				return data.GetSize();
			}
			catch (...) {}

			return ResultCode::Failed;
		}
		else
		{
			auto result = m_Socket.SendTo(m_PeerEndpoint, data);
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
					if (Suspend())
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
	}

	Connection::ReceiveBuffer& Connection::GetReceiveBuffer() const noexcept
	{
		static thread_local ReceiveBuffer rcvbuf{ ReceiveBuffer::GetMaxSize() };
		return rcvbuf;
	}

	bool Connection::ReceiveToQueue() noexcept
	{
		IPEndpoint endpoint;
		auto& buffer = GetReceiveBuffer();

		while (true)
		{
			if (m_Socket.UpdateIOStatus(0ms))
			{
				if (m_Socket.GetIOStatus().CanRead())
				{
					auto bufspan = BufferSpan(buffer);

					const auto result = m_Socket.ReceiveFrom(endpoint, bufspan);
					if (result.Succeeded())
					{
						if (*result > 0)
						{
							bufspan = bufspan.GetFirst(*result);

							if (!ProcessReceivedData(endpoint, bufspan))
							{
								return false;
							}
						}
						else break;
					}
					else
					{
						if (result.GetErrorCode().category() == std::system_category() &&
							result.GetErrorCode().value() == WSAECONNRESET)
						{
							LogDbg(L"UDP connection: port unreachable for connection %llu (%s)",
									GetID(), result.GetErrorString().c_str());

							// If the port is unreachable this is not a fatal error for us; the
							// connection will be suspended until we hear back from the peer
							break;
						}
						else
						{
							LogErr(L"UDP connection: receive failed for connection %llu (%s)",
								   GetID(), result.GetErrorString().c_str());

							SetCloseCondition(CloseCondition::ReceiveError, result.GetErrorCode().value());

							return false;
						}
					}
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

	bool Connection::ProcessReceivedData(const IPEndpoint& endpoint, BufferSpan& buffer) noexcept
	{
		m_LastReceiveSteadyTime = Util::GetCurrentSteadyTime();

		Message msg(Message::Type::Unknown, Message::Direction::Incoming);
		if (msg.Read(buffer, m_SymmetricKeys) && msg.IsValid())
		{
			switch (GetStatus())
			{
				case Status::Handshake:
					return ProcessReceivedMessageHandshake(endpoint, std::move(msg));
				case Status::Suspended:
					// Receiving data while suspended, so wake up first
					if (!Resume())
					{
						SetCloseCondition(CloseCondition::GeneralFailure);
						return false;
					}
					[[fallthrough]];
				case Status::Connected:
					return ProcessReceivedMessageConnected(endpoint, std::move(msg));
				default:
					// Shouldn't get here
					assert(false);
					break;
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

	bool Connection::ProcessReceivedMessageHandshake(const IPEndpoint& endpoint, Message&& msg) noexcept
	{
		if (GetType() == PeerConnectionType::Outbound)
		{
			if (msg.GetType() == Message::Type::Syn)
			{
				// Handshake response should come from same IP address that
				// we tried connecting to
				if (endpoint == m_PeerEndpoint)
				{
					const auto& syn_data = msg.GetSynData();

					if (syn_data.ProtocolVersionMajor == UDP::ProtocolVersion::Major &&
						syn_data.ProtocolVersionMinor == UDP::ProtocolVersion::Minor)
					{
						if (GetID() == syn_data.ConnectionID)
						{
							m_LastInSequenceReceivedSequenceNumber = msg.GetMessageSequenceNumber();

							assert(msg.HasAck());

							if (msg.HasAck())
							{
								m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());
							}

							if (AckReceivedMessage(msg.GetMessageSequenceNumber()))
							{
								if (SetStatus(Status::Connected))
								{
									// Endpoint update with new received port
									m_PeerEndpoint = IPEndpoint(endpoint.GetProtocol(), endpoint.GetIPAddress(), syn_data.Port);

									m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
									{
										// Endpoint update
										connection_data.SetLocalEndpoint(m_Socket.GetLocalEndpoint());
										// Don't need listener send queue anymore
										connection_data.ReleaseListenerSendQueue();
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
									endpoint.GetString().c_str(), GetID(), syn_data.ConnectionID);
					}
					else LogErr(L"UDP connection: could not accept connection from peer %s on connection %llu; unsupported UDP protocol version",
								endpoint.GetString().c_str(), GetID());
				}
				else LogErr(L"UDP connection: received handshake response from unexpected IP address %s on connection %llu",
							endpoint.GetString().c_str(), GetID());
			}
			else LogErr(L"UDP connection: received unexpected message type %u during handshake on connection %llu",
						msg.GetType(), GetID());
		}
		else if (GetType() == PeerConnectionType::Inbound)
		{
			if (ProcessReceivedMessageConnected(endpoint, std::move(msg)))
			{
				if (SetStatus(Status::Connected))
				{
					m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
					{
						// Don't need listener send queue anymore
						connection_data.ReleaseListenerSendQueue();
						// Socket can now send data
						connection_data.SetWrite(true);
						// Notify of state change
						connection_data.SignalReceiveEvent();
					});

					return true;
				}
			}
		}

		return false;
	}

	bool Connection::CheckEndpointChange(const IPEndpoint& endpoint) noexcept
	{
		if (m_PeerEndpoint != endpoint)
		{
			const auto result1 = m_AccessManager.GetIPAllowed(endpoint.GetIPAddress(), Access::CheckType::IPFilters);
			const auto result2 = m_AccessManager.GetIPAllowed(endpoint.GetIPAddress(), Access::CheckType::IPReputations);

			if ((result1 && *result1) && (result2 && *result2))
			{
				m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
				{
					connection_data.SetPeerEndpoint(endpoint);
				});

				LogWarn(L"UDP connection: peer endpoint changed from %s to %s for connection %llu",
						m_PeerEndpoint.GetString().c_str(), endpoint.GetString().c_str(), GetID());

				m_PeerEndpoint = endpoint;
			}
			else
			{
				LogErr(L"UDP connection: attempt to change peer endpoint from %s to %s for connection %llu failed; IP address is not allowed by access configuration",
					   m_PeerEndpoint.GetString().c_str(), endpoint.GetString().c_str(), GetID());
				
				SetCloseCondition(CloseCondition::PeerNotAllowed);

				return false;
			}
		}

		return true;
	}

	bool Connection::ProcessReceivedMessageConnected(const IPEndpoint& endpoint, Message&& msg) noexcept
	{
		auto success = false;
		auto endpoint_check = true;

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
						assert(msg.HasAck());

						if (msg.HasAck())
						{
							m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());
						}

						if (AckReceivedMessage(msg.GetMessageSequenceNumber()))
						{
							try
							{
								m_ReceiveQueue.emplace(msg.GetMessageSequenceNumber(), std::move(msg));
								success = true;
							}
							catch (...) {}
						}
					}
					case ReceiveWindow::Previous:
					{
						// May have been retransmitted due to delays or lost ack;
						// send an ack (again) and drop message
						DiscardReturnValue(AckReceivedMessage(msg.GetMessageSequenceNumber()));
						success = true;
						break;
					}
					case ReceiveWindow::Unknown:
					{
						// Drop message
						success = true;
						break;
					}
					default:
					{
						assert(false);
						break;
					}
				}

				break;
			}
			case Message::Type::EAck:
			{
				Dbg(L"UDP connection: received ack message from peer %s on connection %llu",
					endpoint.GetString().c_str(), GetID());

				assert(msg.HasAck());

				if (msg.HasAck())
				{
					m_SendQueue.ProcessReceivedInSequenceAck(msg.GetMessageAckNumber());
				}

				m_SendQueue.ProcessReceivedAcks(msg.GetAckRanges());
				success = true;
				break;
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
				success = true;
				break;
			}
			case Message::Type::Reset:
			{
				Dbg(L"UDP connection: received reset message from peer %s on connection %llu",
					endpoint.GetString().c_str(), GetID());

				m_ConnectionData->WithUniqueLock()->SetCloseRequest();
				SetCloseCondition(CloseCondition::PeerCloseRequest);
				success = true;
				break;
			}
			case Message::Type::Null:
			{
				Dbg(L"UDP connection: received null message from peer %s on connection %llu",
					endpoint.GetString().c_str(), GetID());

				success = true;
				break;
			}
			case Message::Type::Syn:
			{
				Dbg(L"UDP connection: received syn message from peer %s on connection %llu",
					endpoint.GetString().c_str(), GetID());

				// Should not be receiving syn messages in connected state;
				// may have been retransmitted duplicate so we drop it

				success = true;
				endpoint_check = false;
				break;
			}
			default:
			{
				LogErr(L"UDP connection: received unknown message from peer %s on connection %llu",
					   endpoint.GetString().c_str(), GetID());
				
				SetCloseCondition(CloseCondition::UnknownMessageError);
				endpoint_check = false;
				break;
			}
		}

		if (success && endpoint_check)
		{
			return CheckEndpointChange(endpoint);
		}

		return success;
	}

	Connection::ReceiveWindow Connection::GetMessageSequenceNumberWindow(const Message::SequenceNumber seqnum) noexcept
	{
		if (IsMessageSequenceNumberInCurrentWindow(seqnum, m_LastInSequenceReceivedSequenceNumber, m_ReceiveWindowSize))
		{
			return ReceiveWindow::Current;
		}

		if (IsMessageSequenceNumberInPreviousWindow(seqnum, m_LastInSequenceReceivedSequenceNumber, m_ReceiveWindowSize))
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
		try
		{
			m_ReceivePendingAckSet.emplace(seqnum);
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
				LogErr(L"UDP connection: unhandled messagetype in receive queue");
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