// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Socks5Connection.h"
#include "Socks5Extender.h"

#include "Console.h"
#include "Common\Util.h"
#include "Common\Endian.h"

using namespace QuantumGate::Implementation;
using namespace QuantumGate::Implementation::Network;
using namespace std::literals;

namespace QuantumGate::Socks5Extender
{
	Connection::Connection(Extender& extender, const PeerLUID pluid, Socket&& socket) noexcept :
		m_PeerLUID(pluid), m_Socket(std::move(socket)), m_Extender(extender)
	{
		m_ID = Util::GetPseudoRandomNumber();
		m_Key = MakeKey(m_PeerLUID, m_ID);
		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();

		m_Type = Type::Incoming;
		SetStatus(Status::Handshake);
	}

	Connection::Connection(Extender& extender, const PeerLUID pluid, const Connection::ID cid,
						   const SocksProtocolVersion socks_version, Socket&& socket) noexcept :
		m_ProtocolVersion(socks_version), m_ID(cid), m_PeerLUID(pluid), m_Socket(std::move(socket)), m_Extender(extender)
	{
		m_Key = MakeKey(m_PeerLUID, m_ID);
		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();

		m_Type = Type::Outgoing;
		SetStatus(Status::Connecting);
	}

	void Connection::Disconnect()
	{
		assert(GetStatus() < Status::Disconnecting);

		LogInfo(L"%s: disconnecting connection %llu from peer %llu", m_Extender.GetName().c_str(), GetID(), GetPeerLUID());

		if (m_Socket.GetIOStatus().IsOpen()) m_Socket.Close();

		if (IsPeerConnected())
		{
			// Let peer know we're going away
			if (m_Extender.SendDisconnect(GetPeerLUID(), GetID()))
			{
				// We'll wait for DisconnectAck
				SetStatus(Status::Disconnecting);
			}
			else
			{
				// Couldn't send disconnect message;
				// peer might already be gone
				SetPeerConnected(false);
				SetStatus(Status::Disconnected);
			}
		}
		else SetStatus(Status::Disconnected);
	}

	bool Connection::IsTimedOut() const noexcept
	{
		// Timeout is shorter in handshake state
		const auto current_time = Util::GetCurrentSteadyTime();
		return ((IsInHandshake() && current_time - m_Socket.GetConnectedSteadyTime() > 30s) ||
			(IsDisconnected() && current_time - GetLastActiveSteadyTime() > 30s) ||
				(current_time - GetLastActiveSteadyTime() > 600s));
	}

	void Connection::SetStatus(Status status) noexcept
	{
		auto success = true;
		const auto prev_status = m_Status;

		switch (status)
		{
			case Status::Handshake:
				assert(prev_status == Status::Unknown);
				if (prev_status == Status::Unknown) m_Status = status;
				else success = false;
				break;
			case Status::Authenticating:
				assert(prev_status == Status::Handshake);
				if (prev_status == Status::Handshake) m_Status = status;
				else success = false;
				break;
			case Status::Connecting:
				assert(prev_status == Status::Unknown || prev_status == Status::Handshake
					   || prev_status == Status::Authenticating);
				if (prev_status == Status::Unknown || prev_status == Status::Handshake
					|| prev_status == Status::Authenticating) m_Status = status;
				else success = false;
				break;
			case Status::Connected:
				assert(prev_status == Status::Connecting);
				if (prev_status == Status::Connecting) m_Status = status;
				else success = false;
				break;
			case Status::Ready:
				assert(prev_status == Status::Connected);
				if (prev_status == Status::Connected) m_Status = status;
				else success = false;
				break;
			case Status::Disconnecting:
				assert(prev_status != Status::Disconnecting);
				if (prev_status != Status::Disconnecting) m_Status = status;
				else success = false;
				break;
			case Status::Disconnected:
				assert(prev_status != Status::Disconnected && !IsPeerConnected() && !m_Socket.GetIOStatus().IsOpen());
				if (prev_status != Status::Disconnected && !IsPeerConnected() && !m_Socket.GetIOStatus().IsOpen())
				{
					m_Status = status;
				}
				else success = false;
				break;
			default:
				assert(false);
				break;
		}

		if (success)
		{
			m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();
		}
		else
		{
			LogErr(L"Failed to change status for connection %llu to %d", GetID(), status);
			SetDisconnectCondition();
		}
	}

	bool Connection::SendSocks4Reply(const Socks4Protocol::Replies reply)
	{
		return SendSocks4Reply(reply, BufferView(), 0);
	}

	bool Connection::SendSocks4Reply(const Socks4Protocol::Replies reply,
									 const BufferView& address, const UInt16 port)
	{
		Socks4Protocol::ReplyMsg msg;
		msg.Reserved = 0;
		msg.Reply = static_cast<UInt8>(reply);
		msg.DestinationPort = port;

		if (address.GetSize() >= sizeof(msg.DestinationIP))
		{
			std::memcpy(&msg.DestinationIP, address.GetBytes(), sizeof(msg.DestinationIP));
		}

		return Send(BufferView(reinterpret_cast<Byte*>(&msg), sizeof(msg)));
	}

	bool Connection::SendSocks5Reply(const Socks5Protocol::Replies reply)
	{
		return SendSocks5Reply(reply, Socks5Protocol::AddressTypes::DomainName, BufferView(), 0);
	}

	bool Connection::SendSocks5Reply(const Socks5Protocol::Replies reply,
									 const Socks5Protocol::AddressTypes atype,
									 const BufferView& address, const UInt16 port)
	{
		Socks5Protocol::ReplyMsg msg;
		msg.Reply = static_cast<UInt8>(reply);
		msg.AddressType = static_cast<UInt8>(atype);

		Buffer sndbuf{ BufferView(reinterpret_cast<Byte*>(&msg), sizeof(msg)) };

		switch (atype)
		{
			case Socks5Protocol::AddressTypes::IPv4:
			{
				assert(address.GetSize() >= 4);

				Socks5Protocol::IPv4Address addr;
				std::memcpy(&addr.Address, address.GetBytes(), 4);
				addr.Port = Endian::ToNetworkByteOrder(port);
				sndbuf += BufferView(reinterpret_cast<Byte*>(&addr), sizeof(addr));

				break;
			}
			case Socks5Protocol::AddressTypes::IPv6:
			{
				assert(address.GetSize() >= 16);

				Socks5Protocol::IPv6Address addr;
				std::memcpy(&addr.Address, address.GetBytes(), 16);
				addr.Port = Endian::ToNetworkByteOrder(port);
				sndbuf += BufferView(reinterpret_cast<Byte*>(&addr), sizeof(addr));

				break;
			}
			case Socks5Protocol::AddressTypes::DomainName:
			{
				assert(address.GetSize() <= 255);

				const Byte size = static_cast<Byte>(address.GetSize());
				const UInt16 nport = Endian::ToNetworkByteOrder(port);

				sndbuf += BufferView(&size, 1);
				if (!address.IsEmpty()) sndbuf += BufferView(address.GetBytes(), address.GetSize());
				sndbuf += BufferView(reinterpret_cast<const Byte*>(&nport), 2);

				break;
			}
			default:
			{
				assert(false);
				return false;
			}
		}

		return Send(std::move(sndbuf));
	}

	bool Connection::SendRelayedData(Buffer&& data)
	{
		if (!ShouldDisconnect()) return Send(std::move(data));

		return true;
	}

	void Connection::ProcessEvents(bool& didwork)
	{
		assert(!IsDisconnecting() && !IsDisconnected());

		bool dw{ false };

		if (SendAndReceive(dw))
		{
			if (IsInHandshake())
			{
				const auto spv = GetSocksProtocolVersion();
				if (spv == SocksProtocolVersion::Unknown)
				{
					if (!DetermineProtocolVersion())
					{
						SetDisconnectCondition();
					}
				}

				if (spv != SocksProtocolVersion::Unknown && !ShouldDisconnect())
				{
					switch (spv)
					{
						case SocksProtocolVersion::Socks4:
						{
							if (!HandleReceivedSocks4Messages())
							{
								SetDisconnectCondition();
							}
							break;
						}
						case SocksProtocolVersion::Socks5:
						{
							if (!HandleReceivedSocks5Messages())
							{
								SetDisconnectCondition();
							}
							break;
						}
						default:
						{
							// Shouldn't get here
							assert(false);
							SetDisconnectCondition();
							break;
						}
					}
				}

				if (!ShouldDisconnect())
				{
					if (!SendAndReceive(dw))
					{
						SetDisconnectCondition();
					}
				}
			}
		}
		else
		{
			LogDbg(L"%s: send/receive failed on connection %llu",
				   m_Extender.GetName().c_str(), GetID());

			SetDisconnectCondition();
		}

		if (ShouldDisconnect())
		{
			// If we have trouble sending or receiving
			// we can disconnect immediately
			LogDbg(L"%s: will remove connection %llu marked for disconnection",
				   m_Extender.GetName().c_str(), GetID());

			// Attempt to write the last bits of data
			// we have if possible before disconnecting
			FlushBuffers();

			Disconnect();

			dw = true;
		}

		if (dw)
		{
			m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();
			didwork = true;
		}
	}

	void Connection::ProcessRelayEvents(bool& didwork, const Size max_send, Size& sent)
	{
		assert(!IsDisconnecting() && !IsDisconnected());

		if (!ShouldDisconnect())
		{
			if (IsReady())
			{
				if (!RelayReceivedData(max_send, sent))
				{
					SetDisconnectCondition();
				}

				if (sent > 0)
				{
					m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();
					didwork = true;
				}
			}
		}
	}

	void Connection::FlushBuffers()
	{
		if (IsInHandshake() && !m_SendBuffer.IsEmpty())
		{
			// This should send any remaining data such as
			// Socks5 (error) replies
			if (m_Socket.UpdateIOStatus(0ms))
			{
				if (!m_Socket.GetIOStatus().HasException() && m_Socket.GetIOStatus().CanWrite())
				{
					while (m_Socket.Send(m_SendBuffer))
					{
						if (m_SendBuffer.IsEmpty()) break;
						else std::this_thread::sleep_for(1ms);
					}
				}
			}
		}
		else if (IsReady() && !m_ReceiveBuffer.IsEmpty())
		{
			Size sent{ 0 };

			// This should send any remaining requested
			// data for this connection to the peer
			while (RelayReceivedData(m_Extender.GetMaxDataRelayDataSize(), sent))
			{
				if (m_ReceiveBuffer.IsEmpty()) break;
				else std::this_thread::sleep_for(1ms);
			}
		}
	}

	UInt64 Connection::MakeKey(const PeerLUID pluid, const Connection::ID cid) noexcept
	{
		return Util::GetNonPersistentHash(Util::FormatString(L"%llu:%llu", pluid, cid));
	}

	bool Connection::SendAndReceive(bool& didwork)
	{
		auto success = true;

		if (m_Socket.UpdateIOStatus(0ms))
		{
			if (m_Socket.GetIOStatus().IsConnecting())
			{
				// Peer might have left already when we get here
				// because of closed connection; in that case no use
				// checking if connection succeeded because we'll close
				// it soon anyway
				if (IsPeerConnected())
				{
					if (m_Socket.GetIOStatus().HasException())
					{
						switch (GetSocksProtocolVersion())
						{
							case SocksProtocolVersion::Socks4:
								DiscardReturnValue(m_Extender.SendSocks4Reply(GetPeerLUID(), GetID(),
																			  m_Extender.TranslateWSAErrorToSocks4(m_Socket.GetIOStatus().GetErrorCode())));
								break;
							case SocksProtocolVersion::Socks5:
								DiscardReturnValue(m_Extender.SendSocks5Reply(GetPeerLUID(), GetID(),
																			  m_Extender.TranslateWSAErrorToSocks5(m_Socket.GetIOStatus().GetErrorCode())));
								break;
							default:
								assert(false);
								break;
						}

						LogErr(L"%s: got exception on socket %s (%s)",
							   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str(),
							   GetSysErrorString(m_Socket.GetIOStatus().GetErrorCode()).c_str());

						success = false;
						didwork = true;
					}
					else if (m_Socket.GetIOStatus().CanWrite())
					{
						didwork = true;

						// If a connection attempt was locally started and the socket becomes
						// writable then the connection succeeded; complete the connection attempt
						if (m_Socket.CompleteConnect())
						{
							SetStatus(Status::Connected);

							LogInfo(L"%s: connected to %s for connection %llu",
									m_Extender.GetName().c_str(), m_Socket.GetPeerName().c_str(), GetID());

							// Let peer know connection succeeded
							switch (GetSocksProtocolVersion())
							{
								case SocksProtocolVersion::Socks4:
								{
									if (m_Extender.SendSocks4Reply(GetPeerLUID(), GetID(), Socks4Protocol::Replies::Succeeded,
																   m_Socket.GetLocalEndpoint().GetIPAddress().GetBinary(),
																   m_Socket.GetLocalEndpoint().GetPort()))
									{
										SetStatus(Status::Ready);
									}
									else
									{
										LogErr(L"%s: could not send Socks4 Succeeded reply to peer %llu for connection %llu",
											   m_Extender.GetName().c_str(), GetPeerLUID(), GetID());
										success = false;
									}

									break;
								}
								case SocksProtocolVersion::Socks5:
								{
									Socks5Protocol::AddressTypes atype{ Socks5Protocol::AddressTypes::IPv4 };
									if (m_Socket.GetLocalEndpoint().GetIPAddress().GetFamily() == IPAddress::Family::IPv6)
									{
										atype = Socks5Protocol::AddressTypes::IPv6;
									}

									if (m_Extender.SendSocks5Reply(GetPeerLUID(), GetID(), Socks5Protocol::Replies::Succeeded, atype,
																   m_Socket.GetLocalEndpoint().GetIPAddress().GetBinary(),
																   m_Socket.GetLocalEndpoint().GetPort()))
									{
										SetStatus(Status::Ready);
									}
									else
									{
										LogErr(L"%s: could not send Socks5 Succeeded reply to peer %llu for connection %llu",
											   m_Extender.GetName().c_str(), GetPeerLUID(), GetID());
										success = false;
									}

									break;
								}
								default:
								{
									assert(false);
									success = false;
									break;
								}
							}
						}
						else
						{
							switch (GetSocksProtocolVersion())
							{
								case SocksProtocolVersion::Socks4:
									DiscardReturnValue(m_Extender.SendSocks4Reply(GetPeerLUID(), GetID(),
																				  Socks4Protocol::Replies::FailedOrRejected));
									break;
								case SocksProtocolVersion::Socks5:
									DiscardReturnValue(m_Extender.SendSocks5Reply(GetPeerLUID(), GetID(),
																				  Socks5Protocol::Replies::GeneralFailure));
									break;
								default:
									assert(false);
									break;
							}

							LogErr(L"%s: CompleteConnect failed for socket %s",
								   m_Extender.GetName().c_str(), m_Socket.GetPeerName().c_str());
							success = false;
						}
					}
				}
			}
			else
			{
				if (m_Socket.GetIOStatus().HasException())
				{
					LogErr(L"%s: got exception on socket %s (%s)",
						   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str(),
						   GetSysErrorString(m_Socket.GetIOStatus().GetErrorCode()).c_str());
					success = false;
					didwork = true;
				}
				else
				{
					if (m_Socket.GetIOStatus().CanRead() && m_ReceiveBuffer.GetSize() < MaxReceiveBufferSize)
					{
						// Get as much data as possible at once for efficiency
						while (m_Socket.GetIOStatus().CanRead() && success && m_ReceiveBuffer.GetSize() < MaxReceiveBufferSize)
						{
							success = m_Socket.Receive(m_ReceiveBuffer, MaxReceiveBufferSize - m_ReceiveBuffer.GetSize());
							if (success)
							{
								success = m_Socket.UpdateIOStatus(0ms);
								if (success)
								{
									if (m_Socket.GetIOStatus().HasException())
									{
										LogErr(L"%s: got exception on socket %s (%s)",
											   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str(),
											   GetSysErrorString(m_Socket.GetIOStatus().GetErrorCode()).c_str());
										success = false;
										break;
									}
								}
							}
						}

						if (!m_ReceiveBuffer.IsEmpty()) m_Extender.SetConnectionReceiveEvent();

						didwork = true;
					}

					if (m_Socket.GetIOStatus().CanWrite() && !m_SendBuffer.IsEmpty())
					{
						success = m_Socket.Send(m_SendBuffer);
						
						if (!m_SendBuffer.IsEmpty()) m_Extender.SetConnectionSendEvent();

						didwork = true;
					}
				}
			}
		}
		else success = false;

		return success;
	}

	bool Connection::DetermineProtocolVersion() noexcept
	{
		assert(IsInHandshake());

		// Try to determine Socks protocol version based on first byte
		if (m_ReceiveBuffer.GetSize() >= 1u)
		{
			const auto byte = m_ReceiveBuffer[0];

			if (byte == Byte{ 0x04 })
			{
				m_ProtocolVersion = SocksProtocolVersion::Socks4;
			}
			else if (byte == Byte{ 0x05 })
			{
				m_ProtocolVersion = SocksProtocolVersion::Socks5;
			}
			else
			{
				LogErr(L"%s: received incorrect version %u on socket %s",
					   m_Extender.GetName().c_str(), byte, m_Socket.GetPeerEndpoint().GetString().c_str());
				return false;
			}
		}

		return true;
	}

	bool Connection::HandleReceivedSocks4Messages() noexcept
	{
		assert(IsInHandshake());

		auto success = true;

		switch (GetStatus())
		{
			case Status::Handshake:
				SetStatus(Status::Connecting);
				[[fallthrough]];
			case Status::Connecting:
				success = ProcessSocks4ConnectMessages();
				break;
			default:
				break;
		}

		return success;
	}

	bool Connection::ProcessSocks4ConnectMessages() noexcept
	{
		auto success = true;

		if (m_ReceiveBuffer.GetSize() >= sizeof(Socks4Protocol::RequestMsg))
		{
			const Socks4Protocol::RequestMsg msg =
				*reinterpret_cast<Socks4Protocol::RequestMsg*>(m_ReceiveBuffer.GetBytes());

			Dbg(L"Socks4 RequestMsg: v:%u, c:%u", msg.Version, msg.Command);

			if (msg.Version == 0x04)
			{
				const UInt8 domain_ip[4]{ 0, 0, 0, 1 };

				if (msg.Command == static_cast<UInt8>(Socks4Protocol::Commands::Connect) &&
					std::memcmp(&msg.DestinationIP, &domain_ip, sizeof(msg.DestinationIP)) != 0)
				{
					success = ProcessSocks4IPv4ConnectMessage();
				}
				else if (msg.Command == static_cast<UInt8>(Socks4Protocol::Commands::Connect) &&
						 std::memcmp(&msg.DestinationIP, &domain_ip, sizeof(msg.DestinationIP)) == 0)
				{
					success = ProcessSocks4DomainConnectMessage();
				}
				else
				{
					DiscardReturnValue(SendSocks4Reply(Socks4Protocol::Replies::FailedOrRejected));

					LogErr(L"%s: received incorrect command on socket %s",
						   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
					success = false;
				}
			}
			else
			{
				DiscardReturnValue(SendSocks4Reply(Socks4Protocol::Replies::FailedOrRejected));

				LogErr(L"%s: received incorrect request on socket %s",
					   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
				success = false;
			}
		}

		return success;
	}

	bool Connection::ProcessSocks4DomainConnectMessage()
	{
		auto success = true;

		const Socks4Protocol::RequestMsg msg =
			*reinterpret_cast<Socks4Protocol::RequestMsg*>(m_ReceiveBuffer.GetBytes());

		BufferView buffer(m_ReceiveBuffer);
		buffer.RemoveFirst(sizeof(Socks4Protocol::RequestMsg));

		// If we have enough data for the userid read it out
		// otherwise we come back later
		if (buffer.GetSize() >= 1)
		{
			// Try to determine how long the userid is;
			// it should end in null character
			const auto posu = GetNullPosition(buffer);
			if (posu != -1)
			{
				const int userid_len = posu + 1;

				buffer.RemoveFirst(userid_len);

				// If we have enough data for the domain read it out
				// otherwise we come back later
				if (buffer.GetSize() >= 1)
				{
					// Try to determine how long the domain is;
					// it should end in null character
					const auto posd = GetNullPosition(buffer);
					if (posd != -1)
					{
						// Domain should not be empty
						if (posd > 0)
						{
							const int domain_len = posd + 1;

							// Read domain name
							std::string domain;
							domain.resize(posd);
							std::memcpy(domain.data(), buffer.GetBytes(), posd);

							// Read port and convert from network byte order
							const auto port = Endian::FromNetworkByteOrder(msg.DestinationPort);

							// Remove what we already processed from the buffer
							m_ReceiveBuffer.RemoveFirst(sizeof(Socks4Protocol::RequestMsg) + userid_len + domain_len);

							Dbg(L"Socks4 RequestMsg: d:%s, p:%u", Util::ToStringW(domain).c_str(), port);

							if (m_Extender.SendConnectDomain(GetPeerLUID(), GetID(), SocksProtocolVersion::Socks4,
															 Util::ToStringW(domain), port))
							{
								SetPeerConnected(true);
								SetStatus(Status::Connected);
							}
							else success = false;
						}
						else
						{
							LogErr(L"%s: received connect request with empty domain on socket %s",
								   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
							success = false;
						}

						if (!success)
						{
							DiscardReturnValue(SendSocks4Reply(Socks4Protocol::Replies::FailedOrRejected));
						}
					}
				}
			}
		}

		return success;
	}

	int Connection::GetNullPosition(const BufferView& buffer) const noexcept
	{
		for (BufferView::SizeType x = 0u; x < buffer.GetSize(); ++x)
		{
			if (buffer[x] == Byte{ '\0' })
			{
				return static_cast<int>(x);
			}
		}

		return -1;
	}

	bool Connection::ProcessSocks4IPv4ConnectMessage()
	{
		auto success = true;

		const Socks4Protocol::RequestMsg msg =
			*reinterpret_cast<Socks4Protocol::RequestMsg*>(m_ReceiveBuffer.GetBytes());

		BufferView buffer(m_ReceiveBuffer);
		buffer.RemoveFirst(sizeof(Socks4Protocol::RequestMsg));

		// If we have enough data for the userid read it out
		// otherwise we come back later
		if (buffer.GetSize() >= 1)
		{
			// Try to determine how long the userid is;
			// it should end in null character
			const auto pos = GetNullPosition(buffer);
			if (pos != -1)
			{
				const int userid_len = pos + 1;

				// Read IPv4 address
				BinaryIPAddress ip;
				ip.AddressFamily = BinaryIPAddress::Family::IPv4;
				std::memcpy(&ip.Bytes, msg.DestinationIP, 4);

				// Read port and convert from network byte order
				const auto port = Endian::FromNetworkByteOrder(msg.DestinationPort);

				// Remove what we already processed from the buffer
				m_ReceiveBuffer.RemoveFirst(sizeof(Socks4Protocol::RequestMsg) + userid_len);

				Dbg(L"Socks4 RequestMsg: ip:%s, p:%u", IPAddress(ip).GetString().c_str(), port);

				if (m_Extender.SendConnectIP(GetPeerLUID(), GetID(), SocksProtocolVersion::Socks4, ip, port))
				{
					SetPeerConnected(true);
					SetStatus(Status::Connected);
				}
				else
				{
					DiscardReturnValue(SendSocks4Reply(Socks4Protocol::Replies::FailedOrRejected));
					success = false;
				}
			}
		}

		return success;
	}

	bool Connection::HandleReceivedSocks5Messages()
	{
		assert(IsInHandshake());

		auto success = true;

		switch (GetStatus())
		{
			case Status::Handshake:
				success = ProcessSocks5MethodIdentificationMessage();
				break;
			case Status::Authenticating:
				success = ProcessSocks5AuthenticationMessages();
				break;
			case Status::Connecting:
				success = ProcessSocks5ConnectMessages();
				break;
			default:
				break;
		}

		return success;
	}

	bool Connection::ProcessSocks5MethodIdentificationMessage()
	{
		auto success = true;

		// First message from client should be MethodIdentificationMsg;
		// if we have enough data for it read it out otherwise we come back later
		if (m_ReceiveBuffer.GetSize() >= sizeof(Socks5Protocol::MethodIdentificationMsg))
		{
			const Socks5Protocol::MethodIdentificationMsg msg =
				*reinterpret_cast<Socks5Protocol::MethodIdentificationMsg*>(m_ReceiveBuffer.GetBytes());

			auto chosen_method = Socks5Protocol::AuthMethods::NoAcceptableMethods;

			if (msg.Version == 0x05)
			{
				if (msg.NumMethods > 0)
				{
					Dbg(L"Socks5 MethodIdentificationMsg: v:%u, nm:%u", msg.Version, msg.NumMethods);

					BufferView buffer(m_ReceiveBuffer);
					buffer.RemoveFirst(sizeof(Socks5Protocol::MethodIdentificationMsg));

					// Do we have enough data for the methods?
					// If not we'll come back later
					if (buffer.GetSize() >= msg.NumMethods)
					{
						for (UInt8 x = 0; x < msg.NumMethods; ++x)
						{
							const auto method = static_cast<Socks5Protocol::AuthMethods>(buffer[x]);

							Dbg(L"Supported Socks5 AuthMethod sent by client: %u", method);

							if (m_Extender.IsAuthenticationRequired())
							{
								if (method == Socks5Protocol::AuthMethods::UsernamePassword)
								{
									chosen_method = method;
									break;
								}
							}
							else
							{
								if (method == Socks5Protocol::AuthMethods::NoAuthenticationRequired)
								{
									chosen_method = method;
									break;
								}
							}
						}

						// Remove what we already processed from the buffer
						m_ReceiveBuffer.RemoveFirst(sizeof(Socks5Protocol::MethodIdentificationMsg) + msg.NumMethods);
					}
				}
				else
				{
					LogErr(L"%s: received invalid number of AuthMethods on socket %s",
						   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
				}

				// Send chosen method to client
				Socks5Protocol::MethodSelectionMsg smsg;
				smsg.Method = static_cast<UInt8>(chosen_method);

				if (Send(BufferView(reinterpret_cast<Byte*>(&smsg), sizeof(smsg))))
				{
					// If chosen method was valid we go to the next step in the handshake
					// otherwise we'll close the connection
					if (chosen_method != Socks5Protocol::AuthMethods::NoAcceptableMethods)
					{
						if (m_Extender.IsAuthenticationRequired())
						{
							SetStatus(Status::Authenticating);
						}
						else SetStatus(Status::Connecting);
					}
					else
					{
						LogErr(L"%s: did not receive any supported AuthMethods on socket %s",
							   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
						success = false;
					}
				}
				else
				{
					LogErr(L"%s: could not send authorization method on socket %s",
						   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
					success = false;
				}
			}
			else
			{
				LogErr(L"%s: received incorrect version on socket %s",
					   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
				success = false;
			}
		}

		return success;
	}

	bool Connection::ProcessSocks5AuthenticationMessages()
	{
		auto success = true;

		if (m_ReceiveBuffer.GetSize() >= 5)
		{
			BufferView buffer(m_ReceiveBuffer);

			if (buffer[0] == Byte{ 0x01 })
			{
				auto reply = Socks5Protocol::Replies::GeneralFailure;

				auto usrlen = std::to_integer<UInt8>(buffer[1]);
				buffer.RemoveFirst(2);

				if (usrlen > 0 && buffer.GetSize() >= usrlen)
				{
					// Read username
					const auto username = buffer.GetFirst(usrlen);
					buffer.RemoveFirst(usrlen);

					auto pwdlen = std::to_integer<UInt8>(buffer[0]);
					buffer.RemoveFirst(1);

					if (pwdlen > 0 && buffer.GetSize() >= pwdlen)
					{
						// Read password
						const auto password = buffer.GetFirst(pwdlen);

						if (m_Extender.CheckCredentials(username, password))
						{
							reply = Socks5Protocol::Replies::Succeeded;
							SetStatus(Status::Connecting);
						}
						else
						{
							LogErr(L"%s: received invalid Authentication credentials on socket %s",
								   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());

							reply = Socks5Protocol::Replies::ConnectionRefused;
							success = false;
						}

						// Remove what we already processed from the buffer
						m_ReceiveBuffer.RemoveFirst(3 + static_cast<Size>(usrlen) + static_cast<Size>(pwdlen));
					}
					else if (pwdlen == 0) success = false;
				}
				else if (usrlen == 0) success = false;

				if (!success)
				{
					LogErr(L"%s: received invalid Socks5 Authentication message on socket %s",
						   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
				}

				// Send authentication reply message to client
				Socks5Protocol::AuthReplyMsg msg;
				msg.Reply = static_cast<UInt8>(reply);

				success = Send(BufferView(reinterpret_cast<Byte*>(&msg), sizeof(msg)));
			}
			else
			{
				LogErr(L"%s: received incorrect Authentication message version on socket %s",
					   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
				success = false;
			}
		}

		return success;
	}

	bool Connection::ProcessSocks5ConnectMessages()
	{
		auto success = true;

		if (m_ReceiveBuffer.GetSize() >= sizeof(Socks5Protocol::RequestMsg))
		{
			const Socks5Protocol::RequestMsg msg =
				*reinterpret_cast<Socks5Protocol::RequestMsg*>(m_ReceiveBuffer.GetBytes());

			Dbg(L"Socks5 RequestMsg: v:%u, c:%u, at:%u", msg.Version, msg.Command, msg.AddressType);

			if (msg.Version == 0x05 && msg.Reserved == 0x0)
			{
				BufferView buffer(m_ReceiveBuffer);
				buffer.RemoveFirst(sizeof(Socks5Protocol::RequestMsg));

				if (msg.Command == static_cast<UInt8>(Socks5Protocol::Commands::Connect))
				{
					switch (static_cast<Socks5Protocol::AddressTypes>(msg.AddressType))
					{
						case Socks5Protocol::AddressTypes::DomainName:
						{
							success = ProcessSocks5DomainConnectMessage(buffer);
							break;
						}
						case Socks5Protocol::AddressTypes::IPv4:
						{
							success = ProcessSocks5IPv4ConnectMessage(buffer);
							break;
						}
						case Socks5Protocol::AddressTypes::IPv6:
						{
							success = ProcessSocks5IPv6ConnectMessage(buffer);
							break;
						}
						default:
						{
							DiscardReturnValue(SendSocks5Reply(Socks5Protocol::Replies::UnsupportedAddressType));

							LogErr(L"%s: received unsupported address type on socket %s",
								   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
							success = false;
							break;
						}
					}
				}
				else
				{
					DiscardReturnValue(SendSocks5Reply(Socks5Protocol::Replies::UnsupportedCommand));

					LogErr(L"%s: received incorrect command on socket %s",
						   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
					success = false;
				}
			}
			else
			{
				DiscardReturnValue(SendSocks5Reply(Socks5Protocol::Replies::GeneralFailure));

				LogErr(L"%s: received incorrect request on socket %s",
					   m_Extender.GetName().c_str(), m_Socket.GetPeerEndpoint().GetString().c_str());
				success = false;
			}
		}

		return success;
	}

	bool Connection::ProcessSocks5DomainConnectMessage(BufferView buffer)
	{
		auto success = true;

		// If we have enough data for the message read it out
		// otherwise we come back later
		if (!buffer.IsEmpty())
		{
			const auto numchars = static_cast<Size>(buffer[0]);
			buffer.RemoveFirst(1);

			if (buffer.GetSize() >= numchars)
			{
				// Read domain name
				std::string domain;
				domain.resize(numchars);
				std::memcpy(domain.data(), buffer.GetBytes(), numchars);

				buffer.RemoveFirst(numchars);

				if (buffer.GetSize() >= 2)
				{
					// Read port and convert from network byte order
					auto port = Endian::FromNetworkByteOrder(*reinterpret_cast<const UInt16*>(buffer.GetBytes()));

					// Remove what we already processed from the buffer
					m_ReceiveBuffer.RemoveFirst(sizeof(Socks5Protocol::RequestMsg) + 1 + numchars + 2);

					Dbg(L"Socks5 RequestMsg: d:%s, p:%u", Util::ToStringW(domain).c_str(), port);

					if (m_Extender.SendConnectDomain(GetPeerLUID(), GetID(), SocksProtocolVersion::Socks5,
													 Util::ToStringW(domain), port))
					{
						SetPeerConnected(true);
						SetStatus(Status::Connected);
					}
					else
					{
						DiscardReturnValue(SendSocks5Reply(Socks5Protocol::Replies::GeneralFailure));
						success = false;
					}
				}
			}
		}

		return success;
	}

	bool Connection::ProcessSocks5IPv4ConnectMessage(BufferView buffer)
	{
		auto success = true;

		// If we have enough data for the message read it out
		// otherwise we come back later
		if (buffer.GetSize() >= 6)
		{
			// Read IPv4 address (4 bytes)
			BinaryIPAddress ip;
			ip.AddressFamily = BinaryIPAddress::Family::IPv4;
			std::memcpy(&ip.Bytes, buffer.GetBytes(), 4);

			buffer.RemoveFirst(4);

			// Read port and convert from network byte order
			auto port = Endian::FromNetworkByteOrder(*reinterpret_cast<const UInt16*>(buffer.GetBytes()));

			// Remove what we already processed from the buffer
			m_ReceiveBuffer.RemoveFirst(sizeof(Socks5Protocol::RequestMsg) + 6);

			Dbg(L"Socks5 RequestMsg: ip:%s, p:%u", IPAddress(ip).GetString().c_str(), port);

			if (m_Extender.SendConnectIP(GetPeerLUID(), GetID(), SocksProtocolVersion::Socks5, ip, port))
			{
				SetPeerConnected(true);
				SetStatus(Status::Connected);
			}
			else
			{
				DiscardReturnValue(SendSocks5Reply(Socks5Protocol::Replies::GeneralFailure));
				success = false;
			}
		}

		return success;
	}

	bool Connection::ProcessSocks5IPv6ConnectMessage(BufferView buffer)
	{
		auto success = true;

		// If we have enough data for the message read it out
		// otherwise we come back later
		if (buffer.GetSize() >= 18)
		{
			// Read IPv6 address (16 bytes)
			BinaryIPAddress ip;
			ip.AddressFamily = BinaryIPAddress::Family::IPv6;
			std::memcpy(&ip.Bytes, buffer.GetBytes(), 16);

			buffer.RemoveFirst(16);

			// Read port and convert from network byte order
			auto port = Endian::FromNetworkByteOrder(*reinterpret_cast<const UInt16*>(buffer.GetBytes()));

			// Remove what we already processed from the buffer
			m_ReceiveBuffer.RemoveFirst(sizeof(Socks5Protocol::RequestMsg) + 18);

			Dbg(L"Socks5 RequestMsg: ip:%s, p:%u", IPAddress(ip).GetString().c_str(), port);

			if (m_Extender.SendConnectIP(GetPeerLUID(), GetID(), SocksProtocolVersion::Socks5, ip, port))
			{
				SetPeerConnected(true);
				SetStatus(Status::Connected);
			}
			else
			{
				DiscardReturnValue(SendSocks5Reply(Socks5Protocol::Replies::GeneralFailure));
				success = false;
			}
		}

		return success;
	}

	bool Connection::Send(const BufferView buffer)
	{
		auto success = true;

		m_SendBuffer += buffer;

		if (m_Socket.GetIOStatus().CanWrite())
		{
			success = m_Socket.Send(m_SendBuffer);
		}

		// Any remaining data will be sent later
		if (success && !m_SendBuffer.IsEmpty())
		{
			m_Extender.SetConnectionSendEvent();
		}

		return success;
	}

	bool Connection::Send(Buffer&& buffer)
	{
		auto success = true;

		if (m_Socket.GetIOStatus().CanWrite())
		{
			success = m_Socket.Send(buffer);
		}

		// Add any remaining data to be sent later
		if (success && !buffer.IsEmpty())
		{
			m_SendBuffer += buffer;
			m_Extender.SetConnectionSendEvent();
		}

		return success;
	}

	bool Connection::RelayReceivedData(const Size max_send, Size& sent)
	{
		assert(IsReady());

		auto success = true;

		if (!m_ReceiveBuffer.IsEmpty())
		{
			if (IsPeerConnected())
			{
				BufferView buffer(m_ReceiveBuffer);

				const auto size = (std::min)(buffer.GetSize(), max_send);

				const auto result = m_Extender.SendDataRelay(GetPeerLUID(), GetID(), buffer.GetFirst(size));
				if (result.Succeeded())
				{
					sent = size;

					if (m_ReceiveBuffer.GetSize() == size)
					{
						m_ReceiveBuffer.Clear();
					}
					else
					{
						m_ReceiveBuffer.RemoveFirst(size);
						// We'll come back later to send the rest
					}
				}
				else if (result == ResultCode::PeerSendBufferFull)
				{
					// Peer send buffer is currently full;
					// we'll come back later to send the rest
				}
				else
				{
					LogErr(L"%s: could not send data relay message to peer %llu for connection %llu",
						   m_Extender.GetName().c_str(), GetPeerLUID(), GetID());

					success = false;
				}
			}
			else success = false;
		}

		if (success && !m_ReceiveBuffer.IsEmpty()) m_Extender.SetConnectionReceiveEvent();

		return success;
	}
}