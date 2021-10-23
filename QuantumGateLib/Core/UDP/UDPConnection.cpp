// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnection.h"
#include "UDPConnectionManager.h"
#include "..\..\Crypto\Crypto.h"
#include "..\..\Common\ScopeGuard.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	Connection::Connection(const Settings_CThS& settings, KeyGeneration::Manager& keymgr, Access::Manager& accessmgr,
						   const PeerConnectionType type, const ConnectionID id, const Message::SequenceNumber seqnum,
						   ProtectedBuffer&& handshake_data, std::optional<ProtectedBuffer>&& shared_secret,
						   std::unique_ptr<Connection::HandshakeTracker>&& handshake_tracker) :
		m_Settings(settings), m_AccessManager(accessmgr), m_Type(type), m_ID(id),
		m_LastInOrderReceivedSequenceNumber(seqnum), m_HandshakeTracker(std::move(handshake_tracker))
	{
		if (shared_secret) m_GlobalSharedSecret = std::move(shared_secret);

		if (!InitializeKeyExchange(keymgr, std::move(handshake_data)))
		{
			throw std::exception("Failed to initialize keyexchange for UDP connection.");
		}
	}

	Connection::~Connection()
	{
		if (m_Socket.GetIOStatus().IsOpen()) m_Socket.Close();
	}

	bool Connection::InitializeKeyExchange(KeyGeneration::Manager& keymgr, ProtectedBuffer&& handshake_data) noexcept
	{
		try
		{
			auto& gss = GetGlobalSharedSecret();
			m_SymmetricKeys[0] = SymmetricKeys{ gss };

			m_KeyExchange = std::make_unique<KeyExchange>(keymgr, GetType(), std::move(handshake_data));

			return true;
		}
		catch (...) {}

		return false;
	}

	bool Connection::FinalizeKeyExchange() noexcept
	{
		assert(m_KeyExchange != nullptr);

		// Assuming peer handshakedata has been set, generate derived keys
		m_SymmetricKeys[1] = m_KeyExchange->GenerateSymmetricKeys(GetGlobalSharedSecret());
		if (m_SymmetricKeys[1])
		{
			// Set default key to expire (will still be used to decrypt messages for a grace period)
			m_SymmetricKeys[0].Expire();

			// Swap the keys so that the derived keys will be used from now on
			m_SymmetricKeys[0] = std::exchange(m_SymmetricKeys[1], std::move(m_SymmetricKeys[0]));

			// Remove asymmetric keys from memory
			m_KeyExchange.reset();

			return true;
		}

		return false;
	}

	bool Connection::Open(const Network::IP::AddressFamily af, const bool nat_traversal, UDP::Socket& socket) noexcept
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

	const ProtectedBuffer& Connection::GetGlobalSharedSecret() const noexcept
	{
		// If we have a specific global shared secret for this peer use it,
		// otherwise return the default from settings
		if (m_GlobalSharedSecret) return *m_GlobalSharedSecret;

		return GetSettings().Local.GlobalSharedSecret;
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
				if (prev_status == Status::Handshake || prev_status == Status::Suspended) m_Status = status;
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

		if (!success || !(success = OnStatusChange(prev_status, status)))
		{
			// If we fail to change the status disconnect as soon as possible
			LogErr(L"UDP connection: failed to change status for connection %llu to %d", GetID(), status);
			SetCloseCondition(CloseCondition::GeneralFailure);
		}

		return success;
	}

	bool Connection::OnStatusChange(const Status old_status, const Status new_status) noexcept
	{
		auto success = true;
		m_LastStatusChangeSteadyTime = Util::GetCurrentSteadyTime();

		switch (new_status)
		{
			case Status::Handshake:
			{
				if (GetType() == PeerConnectionType::Inbound)
				{
					success = FinalizeKeyExchange();
				}
				
				break;
			}
			case Status::Connected:
			{
				ResetKeepAliveTimeout(GetSettings());

				if (m_HandshakeTracker) m_HandshakeTracker.reset();

				if (GetType() == PeerConnectionType::Outbound && old_status == Status::Handshake)
				{
					success = FinalizeKeyExchange();
				}

				break;
			}
			default:
			{
				break;
			}
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

		m_ConnectionData->WithUniqueLock([&](auto& connection_data) noexcept
		{
			connection_data.RemoveSendEvent();
			connection_data.SetException(error_code);
		});
	}

	void Connection::ProcessEvents(const SteadyTime current_steadytime) noexcept
	{
		const auto& settings = GetSettings();

		const auto max_keepalive_timeout = settings.Local.SuspendTimeout + SuspendTimeoutMargin;

		ProcessSocketEvents(settings);

		if (ShouldClose()) return;

		if (!SendDelayedItems(current_steadytime))
		{
			SetCloseCondition(CloseCondition::SendError);
		}

		if (!ReceiveToQueue(current_steadytime))
		{
			SetCloseCondition(CloseCondition::ReceiveError);
		}

		switch (GetStatus())
		{
			case Status::Handshake:
			{
				if (current_steadytime - m_LastStatusChangeSteadyTime >= settings.UDP.ConnectTimeout)
				{
					LogDbg(L"UDP connection: handshake timed out for connection %llu", GetID());

					SetCloseCondition(CloseCondition::TimedOutError);

					// This might be an attack ("slowloris" for example) so limit the
					// number of times this may happen by updating the IP reputation
					UpdateReputation(m_PeerEndpoint, Access::IPReputationUpdate::DeteriorateMinimal);
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

				if (!CheckKeepAlive(settings, current_steadytime) || !ProcessMTUDiscovery())
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

				if (current_steadytime - m_LastReceiveSteadyTime >= max_keepalive_timeout)
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
				if (current_steadytime - suspended_steadytime >= settings.Local.MaxSuspendDuration)
				{
					// Connection has been in the suspended state for
					// too long so we disconnect it now
					LogDbg(L"UDP connection: suspend duration timed out for connection %llu", GetID());

					SetCloseCondition(CloseCondition::TimedOutError);
				}
				else
				{
					// Try to make contact again
					if (!CheckKeepAlive(settings, current_steadytime))
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

	void Connection::UpdateReputation(const IPEndpoint& endpoint, const Access::IPReputationUpdate rep_update) noexcept
	{
		const auto result = m_AccessManager.UpdateIPReputation(endpoint.GetIPAddress(), rep_update);
		if (result.Succeeded() && !result->second && m_PeerEndpoint == endpoint)
		{
			// Peer IP has an unacceptable reputation after the update;
			// disconnect the peer as soon as possible
			SetCloseCondition(CloseCondition::PeerNotAllowed);
		}
		else if (!result.Succeeded())
		{
			LogErr(L"UDP connection: couldn't update IP reputation for peer %s", endpoint.GetString().c_str());
		}
	}

	bool Connection::CheckKeepAlive(const Settings& settings, const SteadyTime current_steadytime) noexcept
	{
		if (current_steadytime - m_LastSendSteadyTime >= m_KeepAliveTimeout)
		{
			ResetKeepAliveTimeout(settings);

			return SendKeepAlive();
		}

		return true;
	}

	void Connection::ResetKeepAliveTimeout(const Settings& settings) noexcept
	{
		m_KeepAliveTimeout = std::chrono::seconds(Random::GetPseudoRandomNumber(0, settings.Local.SuspendTimeout.count()));
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
			m_MTUDiscovery = std::make_unique<MTUDiscovery>(*this, GetSettings().UDP.MaxMTUDiscoveryDelay);
			
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
		assert(mtu >= UDPMessageSizes::Min);

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

	bool Connection::SendOutboundSyn(std::optional<Message::CookieData>&& cookie) noexcept
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
				.Port = static_cast<UInt16>(Random::GetPseudoRandomNumber()),
				.Cookie = std::move(cookie),
				.HandshakeDataOut = &m_KeyExchange->GetHandshakeData()
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
		msg.SetMessageAckNumber(m_LastInOrderReceivedSequenceNumber);
		msg.SetSynData(
			Message::SynData{
				.ProtocolVersionMajor = ProtocolVersion::Major,
				.ProtocolVersionMinor = ProtocolVersion::Minor,
				.ConnectionID = GetID(),
				.Port = m_Socket.GetLocalEndpoint().GetPort(),
				.HandshakeDataOut = &m_KeyExchange->GetHandshakeData()
			});

		if (Send(std::move(msg)))
		{
			m_LastInOrderReceivedSequenceNumber.SetAcked();
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
		msg.SetMessageAckNumber(m_LastInOrderReceivedSequenceNumber);
		msg.SetMessageData(std::move(data));

		if (Send(std::move(msg)))
		{
			m_LastInOrderReceivedSequenceNumber.SetAcked();
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
		msg.SetMessageAckNumber(m_LastInOrderReceivedSequenceNumber);
		msg.SetStateData(
			Message::StateData{
				.MaxWindowSize = static_cast<UInt32>(m_ReceiveWindowSize),
				.MaxWindowSizeBytes = static_cast<UInt32>(MaxReceiveWindowBytes)
			});

		if (Send(std::move(msg)))
		{
			m_LastInOrderReceivedSequenceNumber.SetAcked();
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
		if (m_ReceivePendingAcks.empty()) return true;

		try
		{
			// Update when we leave
			auto sg = MakeScopeGuard([&]
			{
				m_ReceivePendingAcks.clear();
			});

			std::sort(m_ReceivePendingAcks.begin(), m_ReceivePendingAcks.end());

			// If the last sequence number in the list was already ACked
			// then no need to send ACKs
			const auto lastnum = *(m_ReceivePendingAcks.end()-1);
			if (lastnum <= m_LastInOrderReceivedSequenceNumber &&
				m_LastInOrderReceivedSequenceNumber.IsAcked())
			{
				return true;
			}
			
			// Make ranges out of sequence numbers; i.e. 2, 3, 4, 6, 7, 8, 9
			// becomes [2, 4], [6, 9]
			for (auto it = m_ReceivePendingAcks.begin(); it != m_ReceivePendingAcks.end();)
			{
				auto begin = *it;
				auto end = begin;

				++it;

				for (; it != m_ReceivePendingAcks.end();)
				{
					if (end < std::numeric_limits<Message::SequenceNumber>::max() &&
						(*it == end || *it == end + 1))
					{
						end = *it;
						++it;
					}
					else
					{
						break;
					}
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
				Dbg(L"UDP connection: sending ACKs on connection %llu", GetID());

				Message msg(Message::Type::EAck, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());
				msg.SetMessageAckNumber(m_LastInOrderReceivedSequenceNumber);

				const auto max_num_ranges = msg.GetMaxAckRangesPerMessage();
				if (m_ReceivePendingAckRanges.size() <= max_num_ranges)
				{
					msg.SetAckRanges(std::move(m_ReceivePendingAckRanges));
				}
				else
				{
					Vector<Message::AckRange> temp_ack_ranges;
					temp_ack_ranges.reserve(max_num_ranges);
					const auto last = m_ReceivePendingAckRanges.begin() + max_num_ranges;
					std::copy(m_ReceivePendingAckRanges.begin(), last, std::back_inserter(temp_ack_ranges));
					m_ReceivePendingAckRanges.erase(m_ReceivePendingAckRanges.begin(), last);
					
					msg.SetAckRanges(std::move(temp_ack_ranges));
				}

				if (Send(std::move(msg)))
				{
					m_LastInOrderReceivedSequenceNumber.SetAcked();
				}
				else
				{
					LogErr(L"UDP connection: failed to send ACKs on connection %llu", GetID());
					return false;
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"UDP connection: an exception occured while sending ACKs on connection %llu - %s",
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

	void Connection::SendDecoyMessages(const Size max_num, const std::chrono::milliseconds max_interval) noexcept
	{
		const auto num = static_cast<Size>(std::abs(Random::GetPseudoRandomNumber(0, max_num)));
		for (Size x = 0; x < num; ++x)
		{
			Message msg(Message::Type::Null, Message::Direction::Outgoing, m_SendQueue.GetMaxMessageSize());

			const auto delay = std::chrono::milliseconds(std::abs(Random::GetPseudoRandomNumber(0, max_interval.count())));
			// Note that we save the endpoint for decoy messages since they are intended for a specific endpoint
			DiscardReturnValue(Send(std::move(msg), delay, true));
		}
	}

	bool Connection::SendDelayedItems(const SteadyTime current_steadytime) noexcept
	{
		while (!m_DelayedSendQueue.empty())
		{
			auto& itm = m_DelayedSendQueue.top();

			if (itm.IsTime(current_steadytime))
			{
				Dbg(L"\r\nDelayed UDP senditem - time:%jd, sec:%jdms\r\n",
					itm.ScheduleSteadyTime.time_since_epoch().count(), itm.ScheduleMilliseconds.count());

				if (!Send(current_steadytime, itm.MessageType, itm.SequenceNumber,
						  std::move(const_cast<DelayedSendItem&>(itm).Data),
						  std::move(const_cast<DelayedSendItem&>(itm).ListenerSendQueue),
						  std::move(const_cast<DelayedSendItem&>(itm).PeerEndpoint))) return false;

				m_DelayedSendQueue.pop();

				if (m_DelayedSendQueue.empty())
				{
					// Release memory
					DelayedSendItemQueue tmp;
					m_DelayedSendQueue.swap(tmp);
				}
			}
			else break;
		}

		return true;
	}

	bool Connection::Send(Message&& msg, const std::chrono::milliseconds delay, const bool save_endpoint) noexcept
	{
		try
		{
			Buffer data;
			if (msg.Write(data, m_SymmetricKeys[0]))
			{
				const auto now = Util::GetCurrentSteadyTime();

				// Need to use the listener socket to send syn replies for inbound connections.
				// This is because if the peer is behind NAT, it will expect a reply from the same
				// IP and port it sent a syn to which is to our listener socket. Our syn will contain
				// the new port to which the peer should send subsequent messages to.
				// Also use the listener socket to send decoy messages (nulls) in handshake state.
				const bool use_listener_socket = ((msg.GetType() == Message::Type::Syn || msg.GetType() == Message::Type::Null) &&
												  GetType() == PeerConnectionType::Inbound &&
												  GetStatus() < Status::Connected);

				std::shared_ptr<Listener::SendQueue_ThS> listener_send_queue;

				if (use_listener_socket)
				{
					// Should still have listener send queue
					assert(m_ConnectionData->WithSharedLock()->HasListenerSendQueue());

					listener_send_queue = m_ConnectionData->WithUniqueLock()->GetListenerSendQueue();
				}

				std::optional<Message::SequenceNumber> msgseqnum;
				if (msg.HasSequenceNumber()) msgseqnum = msg.GetMessageSequenceNumber();

				// If the message is intended for a specific endpoint we save it
				std::optional<IPEndpoint> endpoint;
				if (save_endpoint) endpoint = m_PeerEndpoint;

				if (delay > 0ms)
				{
					m_DelayedSendQueue.emplace(DelayedSendItem{
						.MessageType = msg.GetType(),
						.SequenceNumber = msgseqnum,
						.ListenerSendQueue = std::move(listener_send_queue),
						.PeerEndpoint = std::move(endpoint),
						.ScheduleSteadyTime = now,
						.ScheduleMilliseconds = delay,
						.Data = std::move(data)});

					return true;
				}
				else
				{
					return Send(now, msg.GetType(), msgseqnum, std::move(data),
								std::move(listener_send_queue), std::move(endpoint));
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

	bool Connection::Send(const SteadyTime current_steadytime, const Message::Type msgtype,
						  const std::optional<Message::SequenceNumber>& msgseqnum, Buffer&& msgdata,
						  std::shared_ptr<Listener::SendQueue_ThS>&& listener_send_queue,
						  std::optional<IPEndpoint>&& peer_endpoint) noexcept
	{
		// Messages with sequence numbers need to be tracked
		// for ack and go into the send queue
		if (msgseqnum.has_value())
		{
			SendQueue::Item itm{
				.MessageType = msgtype,
				.SequenceNumber = *msgseqnum,
				.ListenerSendQueue = std::move(listener_send_queue),
				.PeerEndpoint = std::move(peer_endpoint),
				.TimeSent = current_steadytime,
				.TimeResent = current_steadytime,
				.Data = std::move(msgdata)
			};

			return m_SendQueue.Add(std::move(itm));
		}
		else
		{
			// Messages without sequence numbers are sent in one try
			// and we don't care if they arrive or not
			const auto result = Send(current_steadytime, msgdata, listener_send_queue, peer_endpoint);
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

		return false;
	}

	Result<Size> Connection::Send(const SteadyTime current_steadytime, const Buffer& msgdata,
								  const std::shared_ptr<Listener::SendQueue_ThS>& listener_send_queue,
								  const std::optional<IPEndpoint>& peer_endpoint) noexcept
	{
		m_LastSendSteadyTime = current_steadytime;

		const auto& endpoint = peer_endpoint.has_value() ? *peer_endpoint : m_PeerEndpoint;

		if (listener_send_queue)
		{
			try
			{
				listener_send_queue->WithUniqueLock()->emplace(
					Listener::SendQueueItem{
						.Endpoint = endpoint,
						.Data = msgdata
					});

				return msgdata.GetSize();
			}
			catch (...) {}

			return ResultCode::Failed;
		}
		else
		{
			auto result = m_Socket.SendTo(endpoint, msgdata);
			if (result.Failed())
			{
				if (result.GetErrorCode().category() == std::system_category() &&
					result.GetErrorCode().value() == 10065)
				{
					LogDbg(L"UDP connection: failed to send data on connection %llu (host unreachable)", GetID());

					// Host unreachable error; this may occur when the peer is temporarily
					// not online due to changing IP address or network. In this case
					// we will keep retrying until we get a message from the peer
					// with an updated endpoint. We return success with 0 bytes sent.
					// Eventually the socket will get suspended after enough inactivity.
					return 0;
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

	bool Connection::ReceiveToQueue(const SteadyTime current_steadytime) noexcept
	{
		IPEndpoint endpoint;
		auto& buffer = GetReceiveBuffer();

		if (m_Socket.UpdateIOStatus(0ms))
		{
			if (m_Socket.GetIOStatus().CanRead())
			{
				while (true)
				{
					auto bufspan = BufferSpan(buffer);

					const auto result = m_Socket.ReceiveFrom(endpoint, bufspan);
					if (result.Succeeded())
					{
						if (*result > 0)
						{
							if (m_PeerEndpoint != endpoint)
							{
								// Discard data from unknown endpoints that
								// are not allowed by security configuration
								if (!IsEndpointAllowed(endpoint)) continue;
							}

							bufspan = bufspan.GetFirst(*result);

							if (!ProcessReceivedData(current_steadytime, endpoint, bufspan))
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
			}
			else if (m_Socket.GetIOStatus().HasException())
			{
				LogErr(L"UDP connection: exception on socket for connection %llu (%s)",
						GetID(), GetSysErrorString(m_Socket.GetIOStatus().GetErrorCode()).c_str());

				SetCloseCondition(CloseCondition::ReceiveError, m_Socket.GetIOStatus().GetErrorCode());

				return false;
			}
		}
		else
		{
			LogDbg(L"UDP connection: failed to update socket IOStatus for connection %llu", GetID());

			return false;
		}

		return true;
	}

	bool Connection::ProcessReceivedData(const SteadyTime current_steadytime, const IPEndpoint& endpoint, BufferSpan& buffer) noexcept
	{
		auto success{ false };

		auto read_message = [this](Message& msg, BufferSpan& buf) noexcept -> bool
		{
			assert(!m_SymmetricKeys[0].IsExpired());

			if (msg.Read(buf, m_SymmetricKeys[0]))
			{
				return true;
			}
			else
			{
				if (m_SymmetricKeys[1])
				{
					if (!m_SymmetricKeys[1].IsExpired())
					{
						return msg.Read(buf, m_SymmetricKeys[1]);
					}
					else m_SymmetricKeys[1].Clear();
				}
			}

			return false;
		};

		Message msg(Message::Type::Unknown, Message::Direction::Incoming);
		if (read_message(msg, buffer) && msg.IsValid())
		{
			switch (GetStatus())
			{
				case Status::Handshake:
				{
					success = ProcessReceivedMessageHandshake(endpoint, std::move(msg));
					break;
				}
				case Status::Suspended:
				{
					// Receiving data while suspended, so wake up first
					if (!Resume())
					{
						SetCloseCondition(CloseCondition::GeneralFailure);
						return false;
					}
					[[fallthrough]];
				}
				case Status::Connected:
				{
					success = ProcessReceivedMessageConnected(endpoint, std::move(msg));
					break;
				}
				default:
				{
					// Shouldn't get here
					assert(false);
					break;
				}
			}

			if (success) m_LastReceiveSteadyTime = current_steadytime;
		}
		else
		{
			// Unrecognized message; this is a fatal problem and may be an attack
			UpdateReputation(endpoint, Access::IPReputationUpdate::DeteriorateSevere);

			if (m_PeerEndpoint == endpoint)
			{
				LogErr(L"UDP connection: received invalid message from peer %s on connection %llu",
					   endpoint.GetString().c_str(), GetID());

				SetCloseCondition(CloseCondition::UnknownMessageError);
			}
			else
			{
				LogErr(L"UDP connection: received invalid message from unknown endpoint %s on connection %llu",
					   endpoint.GetString().c_str(), GetID());

				// Might be someone else sending garbage; we just
				// ignore the message and keep the connection alive
				success = true;
			}
		}

		return success;
	}

	bool Connection::ProcessReceivedMessageHandshake(const IPEndpoint& endpoint, Message&& msg) noexcept
	{
		// In handshake state we only accept messages from
		// the same endpoint that we're connecting to
		if (endpoint != m_PeerEndpoint)
		{
			LogErr(L"UDP connection: received handshake response from unexpected endpoint %s on connection %llu",
				   endpoint.GetString().c_str(), GetID());

			UpdateReputation(endpoint, Access::IPReputationUpdate::DeteriorateMinimal);

			// Might be someone else trying to interfere; we just
			// ignore the message and keep the connection alive
			return true;
		}

		if (GetType() == PeerConnectionType::Outbound)
		{
			switch (msg.GetType())
			{
				case Message::Type::Syn:
				{
					auto& syn_data = msg.GetSynData();

					if (syn_data.ProtocolVersionMajor == UDP::ProtocolVersion::Major &&
						syn_data.ProtocolVersionMinor == UDP::ProtocolVersion::Minor)
					{
						if (GetID() == syn_data.ConnectionID)
						{
							m_KeyExchange->SetPeerHandshakeData(std::move(*syn_data.HandshakeDataIn));

							m_LastInOrderReceivedSequenceNumber = msg.GetMessageSequenceNumber();

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
						else LogErr(L"UDP connection: received invalid Syn message from peer %s on connection %llu; unexpected connection ID %llu",
									endpoint.GetString().c_str(), GetID(), syn_data.ConnectionID);
					}
					else LogErr(L"UDP connection: could not accept connection from peer %s on connection %llu; unsupported UDP protocol version",
								endpoint.GetString().c_str(), GetID());
					break;
				}
				case Message::Type::Cookie:
				{
					// Remove previous connect message
					m_SendQueue.Reset();

					// Send connect message again, this time with cookie
					const auto& cookie_data = msg.GetCookieData();
					if (SendOutboundSyn(cookie_data)) return true;
					else
					{
						SetCloseCondition(CloseCondition::GeneralFailure);
					}
					break;
				}
				case Message::Type::Null:
				{
					// Ignored
					return true;
				}
				default:
				{
					LogErr(L"UDP connection: received unexpected message type %u during handshake on connection %llu",
							msg.GetType(), GetID());

					UpdateReputation(endpoint, Access::IPReputationUpdate::DeteriorateModerate);
					break;
				}
			}
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

	bool Connection::ProcessReceivedMessageConnected(const IPEndpoint& endpoint, Message&& msg) noexcept
	{
		auto success = false;
		auto endpoint_check = true;

		switch (msg.GetType())
		{
			case Message::Type::Data:
			case Message::Type::State:
			{
				Dbg(L"UDP connection: received %s message from peer %s (seq# %u) on connection %llu",
					Message::TypeToString(msg.GetType()), endpoint.GetString().c_str(), msg.GetMessageSequenceNumber(), GetID());

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
						m_LastInOrderReceivedSequenceNumber.ResetAcked();
						success = AckReceivedMessage(msg.GetMessageSequenceNumber());
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
				Dbg(L"UDP connection: received %s message from peer %s on connection %llu",
					Message::TypeToString(msg.GetType()), endpoint.GetString().c_str(), GetID());

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
				Dbg(L"UDP connection: received %s message from peer %s on connection %llu",
					Message::TypeToString(msg.GetType()), endpoint.GetString().c_str(), GetID());

				m_ConnectionData->WithUniqueLock()->SetCloseRequest();
				SetCloseCondition(CloseCondition::PeerCloseRequest);
				success = true;
				break;
			}
			case Message::Type::Null:
			{
				Dbg(L"UDP connection: received %s message from peer %s on connection %llu",
					Message::TypeToString(msg.GetType()), endpoint.GetString().c_str(), GetID());

				success = true;
				break;
			}
			case Message::Type::Syn:
			case Message::Type::Cookie:
			{
				Dbg(L"UDP connection: received %s message from peer %s on connection %llu",
					Message::TypeToString(msg.GetType()), endpoint.GetString().c_str(), GetID());

				if (m_PeerEndpoint == endpoint)
				{
					// Should not be receiving these messages in connected state;
					// may have been retransmitted duplicate so we ignore it
				}
				else
				{
					// Might be someone else trying to interfere; we just
					// ignore the message and keep the connection alive
					UpdateReputation(endpoint, Access::IPReputationUpdate::DeteriorateMinimal);
				}

				success = true;
				endpoint_check = false;
				break;
			}
			default:
			{
				UpdateReputation(endpoint, Access::IPReputationUpdate::DeteriorateModerate);

				if (m_PeerEndpoint == endpoint)
				{
					LogErr(L"UDP connection: received unknown message from peer %s on connection %llu",
						   endpoint.GetString().c_str(), GetID());

					SetCloseCondition(CloseCondition::UnknownMessageError);
				}
				else
				{
					LogErr(L"UDP connection: received unknown message from unknown endpoint %s on connection %llu",
						   endpoint.GetString().c_str(), GetID());

					// Might be someone else trying to interfere; we just
					// ignore the message and keep the connection alive
					success = true;
				}

				endpoint_check = false;
				break;
			}
		}

		if (success && endpoint_check) CheckEndpointChange(endpoint);

		return success;
	}

	bool Connection::IsEndpointAllowed(const IPEndpoint& endpoint) noexcept
	{
		const auto result1 = m_AccessManager.GetIPAllowed(endpoint.GetIPAddress(), Access::CheckType::IPFilters);
		const auto result2 = m_AccessManager.GetIPAllowed(endpoint.GetIPAddress(), Access::CheckType::IPReputations);

		return ((result1 && *result1) && (result2 && *result2));
	}

	void Connection::CheckEndpointChange(const IPEndpoint& endpoint) noexcept
	{
		if (GetType() == PeerConnectionType::Outbound && endpoint == m_OriginalPeerEndpoint)
		{
			// Never change back to the listener endpoint
			return;
		}

		if (m_PeerEndpoint != endpoint)
		{
			if (IsEndpointAllowed(endpoint))
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
			}
		}
	}

	Connection::ReceiveWindow Connection::GetMessageSequenceNumberWindow(const Message::SequenceNumber seqnum) noexcept
	{
		if (IsMessageSequenceNumberInCurrentWindow(seqnum, m_LastInOrderReceivedSequenceNumber, m_ReceiveWindowSize))
		{
			return ReceiveWindow::Current;
		}

		if (IsMessageSequenceNumberInPreviousWindow(seqnum, m_LastInOrderReceivedSequenceNumber, m_ReceiveWindowSize))
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
			m_ReceivePendingAcks.emplace_back(seqnum);
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

			const auto maxmsg_size = m_SendQueue.GetMaxMessageSize();
			const Message msg(Message::Type::Data, Message::Direction::Outgoing, maxmsg_size);
			auto sendwnd_bytes = m_SendQueue.GetAvailableSendWindowByteSize();

			while (sendwnd_bytes >= maxmsg_size && connection_data->GetSendBuffer().GetReadSize() > 0)
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

		auto next_itm = m_ReceiveQueue.find(Message::GetNextSequenceNumber(m_LastInOrderReceivedSequenceNumber));
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
				m_LastInOrderReceivedSequenceNumber = msg.GetMessageSequenceNumber();
				m_ReceiveQueue.erase(next_itm);
			}

			next_itm = m_ReceiveQueue.find(Message::GetNextSequenceNumber(m_LastInOrderReceivedSequenceNumber));
		}

		if (rcv_event)
		{
			connection_data->SetRead(true);
			connection_data->SignalReceiveEvent();
		}

		return true;
	}

	void Connection::ProcessSocketEvents(const Settings& settings) noexcept
	{
		auto close_condition = CloseCondition::None;

		{
			auto connection_data = m_ConnectionData->WithSharedLock();

			// Connect requested by socket
			if (GetStatus() == Status::Open && connection_data->HasConnectRequest())
			{
				auto success = true;

				// Update endpoint with the one we should connect to
				m_OriginalPeerEndpoint = connection_data->GetPeerEndpoint();
				m_PeerEndpoint = connection_data->GetPeerEndpoint();

				connection_data.UnlockShared();

				if (Random::GetPseudoRandomNumber(0, 1) == 1)
				{
					SendDecoyMessages(10, std::chrono::milliseconds(100));
				}

				if (settings.UDP.MaxNumDecoyMessages > 0)
				{
					if (Random::GetPseudoRandomNumber(0, 1) == 1)
					{
						SendDecoyMessages(settings.UDP.MaxNumDecoyMessages, settings.UDP.MaxDecoyMessageInterval);
					}
				}

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