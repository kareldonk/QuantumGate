// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerMessageProcessor.h"
#include "PeerManager.h"
#include "Peer.h"
#include "..\..\Memory\BufferReader.h"
#include "..\..\Memory\BufferWriter.h"

using namespace QuantumGate::Implementation::Memory;
using namespace std::literals;

namespace QuantumGate::Implementation::Core::Peer
{
	bool MessageProcessor::SendBeginRelay(const RelayPort rport, const IPEndpoint& endpoint,
										  const RelayHop hops) const noexcept
	{
		Dbg(L"*********** SendBeginRelay ***********");

		BufferWriter wrt(true);
		if (wrt.WriteWithPreallocation(rport, endpoint.GetProtocol(),
									   Network::SerializedBinaryIPAddress{ endpoint.GetIPAddress().GetBinary() },
									   endpoint.GetPort(), hops))
		{
			if (m_Peer.Send(MessageType::RelayCreate, wrt.MoveWrittenBytes()))
			{
				return true;
			}
			else LogDbg(L"Couldn't send RelayCreate message to peer %s", m_Peer.GetPeerName().c_str());
		}
		else LogDbg(L"Couldn't prepare RelayCreate message for peer %s", m_Peer.GetPeerName().c_str());

		return false;
	}

	bool MessageProcessor::SendEndRelay(const RelayPort rport) const noexcept
	{
		Dbg(L"*********** SendEndRelay ***********");

		return SendRelayStatus(rport, RelayStatusUpdate::Disconnected);
	}

	bool MessageProcessor::SendRelayStatus(const RelayPort rport, const RelayStatusUpdate status) const noexcept
	{
		Dbg(L"*********** SendRelayStatus ***********");

		LogDbg(L"Sending relay status %u to peer %s", status, m_Peer.GetPeerName().c_str());

		BufferWriter wrt(true);
		if (wrt.WriteWithPreallocation(rport, status))
		{
			if (m_Peer.Send(MessageType::RelayStatus, wrt.MoveWrittenBytes()))
			{
				return true;
			}
			else LogDbg(L"Couldn't send RelayStatus message to peer %s", m_Peer.GetPeerName().c_str());
		}
		else LogDbg(L"Couldn't prepare RelayStatus message for peer %s", m_Peer.GetPeerName().c_str());

		return false;
	}

	QuantumGate::Result<> MessageProcessor::SendRelayData(const RelayDataMessage& msg) const noexcept
	{
		if (m_Peer.GetAvailableRelayDataSendBufferSize() < msg.GetSize())
		{
			LogDbg(L"Couldn't send RelayData message to peer %s for relay port %llu; peer buffer full",
				   m_Peer.GetPeerName().c_str(), msg.Port);

			return ResultCode::PeerSendBufferFull;
		}

		BufferWriter wrt(true);
		if (wrt.WriteWithPreallocation(msg.Port, msg.ID, WithSize(msg.Data, MaxSize::_2MB)))
		{
			// Note that relayed data doesn't get compressed (again) because
			// it is mostly encrypted and random looking so it wouldn't compress well
			auto result = m_Peer.Send(MessageType::RelayData, wrt.MoveWrittenBytes(),
									  SendParameters::PriorityOption::Normal, 0ms, false);
			if (!result)
			{
				LogDbg(L"Couldn't send RelayData message to peer %s for relay port %llu",
					   m_Peer.GetPeerName().c_str(), msg.Port);
			}

			return result;
		}
		else
		{
			LogDbg(L"Couldn't prepare RelayData message to peer %s for relay port %llu",
				   m_Peer.GetPeerName().c_str(), msg.Port);
		}

		return ResultCode::Failed;
	}

	bool MessageProcessor::SendRelayDataAck(const RelayDataAckMessage& msg) const noexcept
	{
		BufferWriter wrt(true);
		if (wrt.WriteWithPreallocation(msg.Port, msg.ID))
		{
			auto result = m_Peer.Send(MessageType::RelayDataAck, wrt.MoveWrittenBytes(),
									  SendParameters::PriorityOption::Normal, 0ms, false);
			if (result.Succeeded()) return true;
			else
			{
				LogDbg(L"Couldn't send RelayDataAck message to peer %s for relay port %llu",
					   m_Peer.GetPeerName().c_str(), msg.Port);
			}
		}
		else
		{
			LogDbg(L"Couldn't prepare RelayDataAck message to peer %s for relay port %llu",
				   m_Peer.GetPeerName().c_str(), msg.Port);
		}

		return false;
	}

	MessageProcessor::Result MessageProcessor::ProcessMessageReadyState(MessageDetails&& msg) const
	{
		MessageProcessor::Result result;

		switch (msg.GetMessageType())
		{
			case MessageType::ExtenderUpdate:
			{
				Dbg(L"*********** ExtenderUpdate ***********");

				result.Handled = true;

				if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
				{
					Vector<SerializedUUID> psextlist;

					BufferReader rdr(buffer, true);
					if (rdr.Read(WithSize(psextlist, MaxSize::_65KB)))
					{
						Dbg(L"ExtenderUpdate: %u extenders", psextlist.size());

						if (auto pextlist = ValidateExtenderUUIDs(psextlist); pextlist.has_value())
						{
							result.Success = m_Peer.ProcessPeerExtenderUpdate(std::move(*pextlist));
						}
						else LogDbg(L"Invalid ExtenderUpdate message from peer %s; invalid UUID(s)",
									m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Invalid ExtenderUpdate message from peer %s; couldn't read message data",
								m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid ExtenderUpdate message from peer %s; data expected", m_Peer.GetPeerName().c_str());

				break;
			}
			case MessageType::RelayCreate:
			{
				Dbg(L"*********** RelayCreate ***********");

				result.Handled = true;

				if (m_Peer.GetRelayManager().IsRunning())
				{
					if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
					{
						RelayPort rport{ 0 };
						IP::Protocol protocol{ IPEndpoint::Protocol::Unspecified };
						SerializedBinaryIPAddress ip;
						UInt16 port{ 0 };
						RelayHop hop{ 0 };

						BufferReader rdr(buffer, true);
						if (rdr.Read(rport, protocol, ip, port, hop))
						{
							if (protocol == IP::Protocol::UDP || protocol == IP::Protocol::TCP)
							{
								if (ip.AddressFamily == IP::AddressFamily::IPv4 || ip.AddressFamily == IP::AddressFamily::IPv6)
								{
									Relay::Events::Connect rce;
									rce.Port = rport;
									rce.Endpoint = IPEndpoint(protocol, IPAddress{ ip }, port);
									rce.Hop = hop;
									rce.Origin.PeerLUID = m_Peer.GetLUID();
									rce.Origin.LocalEndpoint = m_Peer.GetLocalEndpoint();
									rce.Origin.PeerEndpoint = m_Peer.GetPeerEndpoint();

									if (!m_Peer.GetRelayManager().AddRelayEvent(rport, std::move(rce)))
									{
										// Let the peer know we couldn't accept
										SendRelayStatus(rport, RelayStatusUpdate::GeneralFailure);
									}

									result.Success = true;
								}
								else LogDbg(L"Invalid RelayCreate message from peer %s; unsupported internetwork address family",
											m_Peer.GetPeerName().c_str());
							}
							else LogDbg(L"Invalid RelayCreate message from peer %s; unsupported internetwork protocol",
										m_Peer.GetPeerName().c_str());
						}
						else LogDbg(L"Invalid RelayCreate message from peer %s; couldn't read message data",
									m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Invalid RelayCreate message from peer %s; data expected", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Received RelayCreate message from peer %s, but relays are not enabled",
							m_Peer.GetPeerName().c_str());

				break;
			}
			case MessageType::RelayStatus:
			{
				Dbg(L"*********** RelayStatus ***********");

				result.Handled = true;

				if (m_Peer.GetRelayManager().IsRunning())
				{
					if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
					{
						RelayPort rport{ 0 };
						RelayStatusUpdate status{ RelayStatusUpdate::GeneralFailure };

						BufferReader rdr(buffer, true);
						if (rdr.Read(rport, status))
						{
							LogDbg(L"Received relay peer status %u for port %llu", status, rport);

							Relay::Events::StatusUpdate resu;
							resu.Port = rport;
							resu.Status = status;
							resu.Origin.PeerLUID = m_Peer.GetLUID();

							if (!m_Peer.GetRelayManager().AddRelayEvent(rport, std::move(resu)))
							{
								LogErr(L"Could not add relay event for port %llu", rport);
							}

							result.Success = true;
						}
						else LogDbg(L"Invalid RelayStatus message from peer %s; couldn't read message data",
									m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Invalid RelayStatus message from peer %s; data expected", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Received RelayStatus message from peer %s, but relays are not enabled",
							m_Peer.GetPeerName().c_str());

				break;
			}
			case MessageType::RelayData:
			{
				Dbg(L"*********** RelayData ***********");

				result.Handled = true;

				if (m_Peer.GetRelayManager().IsRunning())
				{
					if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
					{
						RelayPort rport{ 0 };
						RelayMessageID msgid{ 0 };
						Buffer data;

						BufferReader rdr(buffer, true);
						if (rdr.Read(rport, msgid, WithSize(data, MaxSize::_2MB)))
						{
							Relay::Events::RelayData red;
							red.Port = rport;
							red.MessageID = msgid;
							red.Data = std::move(data);
							red.Origin.PeerLUID = m_Peer.GetLUID();

							// Take ownership of rate management for this message;
							// this will keep this message size in the total rate
							// count until the relay data actually gets processed
							red.MessageRate = msg.MoveMessageRate();

							if (!m_Peer.GetRelayManager().AddRelayEvent(rport, std::move(red)))
							{
								LogErr(L"Could not add relay event for port %llu", rport);
							}

							result.Success = true;
						}
						else LogDbg(L"Invalid RelayData message from peer %s; couldn't read message data",
									m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Invalid RelayData message from peer %s; data expected", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Received RelayData message from peer %s, but relays are not enabled",
							m_Peer.GetPeerName().c_str());

				break;
			}
			case MessageType::RelayDataAck:
			{
				Dbg(L"*********** RelayDataAck ***********");

				result.Handled = true;

				if (m_Peer.GetRelayManager().IsRunning())
				{
					if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
					{
						RelayPort rport{ 0 };
						RelayMessageID msgid{ 0 };

						BufferReader rdr(buffer, true);
						if (rdr.Read(rport, msgid))
						{
							Relay::Events::RelayDataAck rda;
							rda.Port = rport;
							rda.MessageID = msgid;
							rda.Origin.PeerLUID = m_Peer.GetLUID();

							if (!m_Peer.GetRelayManager().AddRelayEvent(rport, std::move(rda)))
							{
								LogErr(L"Could not add relay event for port %llu", rport);
							}

							result.Success = true;
						}
						else LogDbg(L"Invalid RelayDataAck message from peer %s; couldn't read message data",
									m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Invalid RelayDataAck message from peer %s; data expected", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Received RelayDataAck message from peer %s, but relays are not enabled",
							m_Peer.GetPeerName().c_str());

				break;
			}
			case MessageType::BeginPrimaryKeyUpdateExchange:
			case MessageType::EndPrimaryKeyUpdateExchange:
			case MessageType::BeginSecondaryKeyUpdateExchange:
			case MessageType::EndSecondaryKeyUpdateExchange:
			case MessageType::KeyUpdateReady:
			{
				result = m_Peer.GetKeyUpdate().ProcessKeyUpdateMessage(std::move(msg));
				break;
			}
			default:
			{
				assert(false);
				break;
			}
		}

		return result;
	}
}