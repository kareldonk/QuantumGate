// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerMessageProcessor.h"
#include "PeerManager.h"
#include "Peer.h"
#include "..\..\Memory\BufferReader.h"
#include "..\..\Memory\BufferWriter.h"

using namespace QuantumGate::Implementation::Memory;

namespace QuantumGate::Implementation::Core::Peer
{
	bool MessageProcessor::SendBeginHandshake() const noexcept
	{
		Dbg(L"*********** SendBeginHandshake ***********");

		try
		{
			const auto& algorithms = m_Peer.GetSupportedAlgorithms();

			BufferWriter wrt(true);
			if (wrt.WriteWithPreallocation(m_Peer.GetLocalProtocolVersion().first,
										   m_Peer.GetLocalProtocolVersion().second,
										   WithSize(algorithms.Hash, MaxSize::_256B),
										   WithSize(algorithms.PrimaryAsymmetric, MaxSize::_256B),
										   WithSize(algorithms.SecondaryAsymmetric, MaxSize::_256B),
										   WithSize(algorithms.Symmetric, MaxSize::_256B),
										   WithSize(algorithms.Compression, MaxSize::_256B)))
			{
				if (m_Peer.SendWithRandomDelay(MessageType::BeginMetaExchange, wrt.MoveWrittenBytes(),
											   m_Peer.GetHandshakeDelayPerMessage()))
				{
					return true;
				}
				else LogDbg(L"Couldn't send BeginMetaExchange message to peer %s", m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Couldn't prepare BeginMetaExchange message for peer %s", m_Peer.GetPeerName().c_str());
		}
		catch (...) {}

		return false;
	}

	bool MessageProcessor::SendBeginPrimaryKeyExchange() const noexcept
	{
		Dbg(L"*********** SendBeginPrimaryKeyExchange ***********");

		return SendBeginKeyExchange(MessageType::BeginPrimaryKeyExchange);
	}

	bool MessageProcessor::SendBeginPrimaryKeyUpdateExchange() const noexcept
	{
		Dbg(L"*********** SendBeginPrimaryKeyUpdateExchange ***********");

		return SendBeginKeyExchange(MessageType::BeginPrimaryKeyUpdateExchange);
	}

	bool MessageProcessor::SendBeginKeyExchange(const MessageType type) const noexcept
	{
		if (m_Peer.GetKeyExchange().GeneratePrimaryAsymmetricKeys(m_Peer.GetAlgorithms(),
																  Crypto::AsymmetricKeyOwner::Alice))
		{
			const auto& lhsdata = m_Peer.GetKeyExchange().GetPrimaryHandshakeData();

			// Should already have data
			assert(!lhsdata.IsEmpty());

			BufferWriter wrt(true);
			if (wrt.WriteWithPreallocation(WithSize(lhsdata, MaxSize::_2MB)))
			{
				switch (type)
				{
					case MessageType::BeginPrimaryKeyExchange:
					{
						if (m_Peer.SendWithRandomDelay(type, wrt.MoveWrittenBytes(), m_Peer.GetHandshakeDelayPerMessage()))
						{
							return true;
						}

						break;
					}
					case MessageType::BeginPrimaryKeyUpdateExchange:
					{
						if (m_Peer.Send(type, wrt.MoveWrittenBytes()))
						{
							return true;
						}

						break;
					}
					default:
					{
						assert(false);
						break;
					}
				}

				LogDbg(L"Couldn't send BeginPrimaryKey(*)Exchange message to peer %s",
					   m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Couldn't prepare BeginPrimaryKey(*)Exchange message for peer %s",
						m_Peer.GetPeerName().c_str());
		}
		else LogDbg(L"Couldn't generate primary asymmetric keys for peer %s", m_Peer.GetPeerName().c_str());

		return false;
	}

	MessageProcessor::Result MessageProcessor::ProcessMessage(MessageDetails&& msg) const
	{
		switch (m_Peer.GetStatus())
		{
			case Status::MetaExchange:
				return ProcessMessageMetaExchange(std::move(msg));
			case Status::PrimaryKeyExchange:
				return ProcessMessagePrimaryKeyExchange(std::move(msg));
			case Status::SecondaryKeyExchange:
				return ProcessMessageSecondaryKeyExchange(std::move(msg));
			case Status::Authentication:
				return ProcessMessageAuthentication(std::move(msg));
			case Status::SessionInit:
				return ProcessMessageSessionInit(std::move(msg));
			case Status::Ready:
				return ProcessMessageReadyState(std::move(msg));
			default:
				break;
		}

		// Not handled, unsuccessful
		return MessageProcessor::Result{ .Handled = false, .Success = false };
	}

	MessageProcessor::Result MessageProcessor::ProcessMessageMetaExchange(MessageDetails&& msg) const
	{
		MessageProcessor::Result result;

		if (msg.GetMessageType() == MessageType::BeginMetaExchange &&
			m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
		{
			Dbg(L"*********** BeginMetaExchange ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				UInt8 v1{ 0 };
				UInt8 v2{ 0 };
				Vector<Algorithm::Hash> phal;
				Vector<Algorithm::Asymmetric> ppaal;
				Vector<Algorithm::Asymmetric> psaal;
				Vector<Algorithm::Symmetric> psal;
				Vector<Algorithm::Compression> pcal;

				BufferReader rdr(buffer, true);
				if (rdr.Read(v1, v2, WithSize(phal, MaxSize::_256B),
							 WithSize(ppaal, MaxSize::_256B), WithSize(psaal, MaxSize::_256B),
							 WithSize(psal, MaxSize::_256B), WithSize(pcal, MaxSize::_256B)))
				{
					m_Peer.SetPeerProtocolVersion(std::make_pair(v1, v2));

					const auto& algorithms = m_Peer.GetSupportedAlgorithms();

					const auto ha = Crypto::ChooseAlgorithm(algorithms.Hash, phal);
					const auto paa = Crypto::ChooseAlgorithm(algorithms.PrimaryAsymmetric, ppaal);
					const auto saa = Crypto::ChooseAlgorithm(algorithms.SecondaryAsymmetric, psaal);
					const auto sa = Crypto::ChooseAlgorithm(algorithms.Symmetric, psal);
					const auto ca = Crypto::ChooseAlgorithm(algorithms.Compression, pcal);

					Dbg(L"Chosen algorithms - Hash: %s, Primary Asymmetric: %s, Secondary Asymmetric: %s, Symmetric: %s, Compression: %s",
						Crypto::GetAlgorithmName(ha), Crypto::GetAlgorithmName(paa), Crypto::GetAlgorithmName(saa),
						Crypto::GetAlgorithmName(sa), Crypto::GetAlgorithmName(ca));

					if (m_Peer.SetAlgorithms(ha, paa, saa, sa, ca))
					{
						BufferWriter wrt(true);
						if (wrt.WriteWithPreallocation(m_Peer.GetLocalProtocolVersion().first,
													   m_Peer.GetLocalProtocolVersion().second, ha, paa, saa, sa, ca))
						{
							if (m_Peer.SendWithRandomDelay(MessageType::EndMetaExchange, wrt.MoveWrittenBytes(),
														   m_Peer.GetHandshakeDelayPerMessage()))
							{
								result.Success = m_Peer.SetStatus(Status::PrimaryKeyExchange);
							}
							else LogDbg(L"Couldn't send EndMetaExchange message to peer %s", m_Peer.GetPeerName().c_str());
						}
						else LogDbg(L"Couldn't prepare EndMetaExchange message for peer %s", m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Couldn't set algorithms for peer %s", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid BeginMetaExchange message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid BeginMetaExchange message from peer %s; data expected", m_Peer.GetPeerName().c_str());
		}
		else if (msg.GetMessageType() == MessageType::EndMetaExchange &&
				 m_Peer.GetConnectionType() == PeerConnectionType::Inbound)
		{
			Dbg(L"*********** EndMetaExchange ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				UInt8 v1{ 0 };
				UInt8 v2{ 0 };
				auto ha = Algorithm::Hash::Unknown;
				auto paa = Algorithm::Asymmetric::Unknown;
				auto saa = Algorithm::Asymmetric::Unknown;
				auto sa = Algorithm::Symmetric::Unknown;
				auto ca = Algorithm::Compression::Unknown;

				BufferReader rdr(buffer, true);
				if (rdr.Read(v1, v2, ha, paa, saa, sa, ca))
				{
					m_Peer.SetPeerProtocolVersion(std::make_pair(v1, v2));

					Dbg(L"Chosen algorithms - Hash: %s, Primary Asymmetric: %s, Secondary Asymmetric: %s, Symmetric: %s, Compression: %s",
						Crypto::GetAlgorithmName(ha), Crypto::GetAlgorithmName(paa), Crypto::GetAlgorithmName(saa),
						Crypto::GetAlgorithmName(sa), Crypto::GetAlgorithmName(ca));

					if (m_Peer.SetAlgorithms(ha, paa, saa, sa, ca))
					{
						if (SendBeginPrimaryKeyExchange())
						{
							result.Success = m_Peer.SetStatus(Status::PrimaryKeyExchange);
						}
					}
					else LogDbg(L"Couldn't set encryption algorithms for peer %s", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid EndMetaExchange message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid EndMetaExchange message from peer %s; data expected", m_Peer.GetPeerName().c_str());
		}

		return result;
	}

	MessageProcessor::Result MessageProcessor::ProcessMessagePrimaryKeyExchange(MessageDetails&& msg) const
	{
		MessageProcessor::Result result;

		if (msg.GetMessageType() == MessageType::BeginPrimaryKeyExchange ||
			msg.GetMessageType() == MessageType::EndPrimaryKeyExchange)
		{
			result = ProcessKeyExchange(std::move(msg));
			if (result.Handled && result.Success)
			{
				result.Success = m_Peer.SetStatus(Status::SecondaryKeyExchange);
			}
		}

		return result;
	}

	MessageProcessor::Result MessageProcessor::ProcessMessageSecondaryKeyExchange(MessageDetails&& msg) const
	{
		MessageProcessor::Result result;

		if (msg.GetMessageType() == MessageType::BeginSecondaryKeyExchange)
		{
			result = ProcessKeyExchange(std::move(msg));
			if (result.Handled && result.Success)
			{
				result.Success = m_Peer.SetStatus(Status::Authentication);
			}
		}
		else if (msg.GetMessageType() == MessageType::EndSecondaryKeyExchange)
		{
			result = ProcessKeyExchange(std::move(msg));
			if (result.Handled && result.Success)
			{
				result.Success = false;
				auto sent = false;

				Buffer sig;
				if (GetSignature(sig))
				{
					BufferWriter wrt(true);
					if (wrt.WriteWithPreallocation(SerializedUUID{ m_Peer.GetLocalUUID() }, m_Peer.GetLocalSessionID(),
												   WithSize(sig, MaxSize::_UINT16)))
					{
						if (m_Peer.SendWithRandomDelay(MessageType::BeginAuthentication, wrt.MoveWrittenBytes(),
													   m_Peer.GetHandshakeDelayPerMessage()))
						{
							sent = true;
							result.Success = m_Peer.SetStatus(Status::Authentication);
						}
					}
				}

				if (!sent)
				{
					LogDbg(L"Couldn't send BeginSessionInit message to peer %s",
						   m_Peer.GetPeerName().c_str());
				}
			}
		}

		return result;
	}

	MessageProcessor::Result MessageProcessor::ProcessMessageAuthentication(MessageDetails&& msg) const
	{
		MessageProcessor::Result result;

		if (msg.GetMessageType() == MessageType::BeginAuthentication &&
			m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
		{
			Dbg(L"*********** BeginAuthentication ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				SerializedUUID spuuid;
				UInt64 psessionid{ 0 };
				Buffer psig;

				BufferReader rdr(buffer, true);
				if (rdr.Read(spuuid, psessionid, WithSize(psig, MaxSize::_UINT16)))
				{
					const UUID puuid{ spuuid };
					if (puuid.GetType() == UUID::Type::Peer)
					{
						m_Peer.SetPeerUUID(puuid);
						m_Peer.SetPeerSessionID(psessionid);

						if (AuthenticatePeer(psig))
						{
							// From now on we encrypt messages using the
							// secondary symmetric key-pair
							m_Peer.GetKeyExchange().StartUsingSecondarySymmetricKeyPairForEncryption();

							Buffer sig;
							if (GetSignature(sig))
							{
								BufferWriter wrt(true);
								if (wrt.WriteWithPreallocation(SerializedUUID{ m_Peer.GetLocalUUID() },
															   m_Peer.GetLocalSessionID(),
															   WithSize(sig, MaxSize::_UINT16)))
								{
									if (m_Peer.SendWithRandomDelay(MessageType::EndAuthentication, wrt.MoveWrittenBytes(),
																   m_Peer.GetHandshakeDelayPerMessage()))
									{
										result.Success = m_Peer.SetStatus(Status::SessionInit);
									}
								}
							}

							if (!result.Success)
							{
								LogDbg(L"Couldn't send EndAuthentication message to peer %s",
									   m_Peer.GetPeerName().c_str());
							}
						}
						else
						{
							// Peer could not be authenticated; disconnect asap
							m_Peer.SetDisconnectCondition(DisconnectCondition::PeerNotAllowed);
							result.Success = true;
						}
					}
					else LogDbg(L"Invalid BeginAuthentication message from peer %s; invalid UUID",
								m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid BeginAuthentication message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid BeginAuthentication message from peer %s; data expected", m_Peer.GetPeerName().c_str());
		}
		else if (msg.GetMessageType() == MessageType::EndAuthentication &&
				 m_Peer.GetConnectionType() == PeerConnectionType::Inbound)
		{
			Dbg(L"*********** EndAuthentication ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				SerializedUUID spuuid;
				UInt64 psessionid{ 0 };
				Buffer psig;

				BufferReader rdr(buffer, true);
				if (rdr.Read(spuuid, psessionid, WithSize(psig, MaxSize::_UINT16)))
				{
					const UUID puuid{ spuuid };
					if (puuid.GetType() == UUID::Type::Peer)
					{
						m_Peer.SetPeerUUID(puuid);
						m_Peer.SetPeerSessionID(psessionid);

						if (AuthenticatePeer(psig))
						{
							// From now on we start using the messagecounter
							const UInt8 counter = m_Peer.SetLocalMessageCounter();

							const auto& lsextlist = m_Peer.GetLocalExtenderUUIDs().SerializedUUIDs;

							assert(lsextlist.size() <= Extender::Manager::MaximumNumberOfExtenders);

							Dbg(L"NumExt: %u", lsextlist.size());

							BufferWriter wrt(true);
							if (wrt.WriteWithPreallocation(counter,
														   m_Peer.GetPublicIPEndpointToReport(),
														   WithSize(lsextlist, MaxSize::_UINT16)))
							{
								if (m_Peer.Send(MessageType::BeginSessionInit, wrt.MoveWrittenBytes()))
								{
									result.Success = m_Peer.SetStatus(Status::SessionInit);
								}
							}

							if (!result.Success)
							{
								LogDbg(L"Couldn't send BeginSessionInit message to peer %s",
									   m_Peer.GetPeerName().c_str());
							}
						}
						else
						{
							// Peer could not be authenticated; disconnect asap
							m_Peer.SetDisconnectCondition(DisconnectCondition::PeerNotAllowed);
							result.Success = true;
						}
					}
					else LogDbg(L"Invalid EndAuthentication message from peer %s; invalid UUID",
								m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid EndAuthentication message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid EndAuthentication message from peer %s; data expected", m_Peer.GetPeerName().c_str());
		}

		return result;
	}

	MessageProcessor::Result MessageProcessor::ProcessMessageSessionInit(MessageDetails&& msg) const
	{
		MessageProcessor::Result result;

		if (msg.GetMessageType() == MessageType::BeginSessionInit &&
			m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
		{
			Dbg(L"*********** BeginSessionInit ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				UInt8 pcounter{ 0 };
				Network::SerializedIPEndpoint pub_endp;
				Vector<SerializedUUID> psextlist;

				BufferReader rdr(buffer, true);
				if (rdr.Read(pcounter, pub_endp, WithSize(psextlist, MaxSize::_UINT16)))
				{
					m_Peer.SetPeerMessageCounter(pcounter);

					if (m_Peer.AddReportedPublicIPEndpoint(pub_endp))
					{
						if (auto pextlist = ValidateExtenderUUIDs(psextlist); pextlist.has_value())
						{
							if (m_Peer.ProcessPeerExtenderUpdate(std::move(*pextlist)))
							{
								const auto& lsextlist = m_Peer.GetLocalExtenderUUIDs().SerializedUUIDs;

								assert(lsextlist.size() <= Extender::Manager::MaximumNumberOfExtenders);

								Dbg(L"NumExt: %u", lsextlist.size());

								// From now on we start using the messagecounter
								const auto counter = m_Peer.SetLocalMessageCounter();

								BufferWriter wrt(true);
								if (wrt.WriteWithPreallocation(counter,
															   m_Peer.GetPublicIPEndpointToReport(),
															   WithSize(lsextlist, MaxSize::_UINT16)))
								{
									if (m_Peer.Send(MessageType::EndSessionInit, wrt.MoveWrittenBytes()))
									{
										result.Success = m_Peer.SetStatus(Status::Ready);
									}
								}

								if (!result.Success)
								{
									LogDbg(L"Couldn't send EndSessionInit message to peer %s",
										   m_Peer.GetPeerName().c_str());
								}
							}
						}
						else LogDbg(L"Invalid BeginSessionInit message from peer %s; invalid extender UUID(s)",
									m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Invalid BeginSessionInit message from peer %s; invalid public IP endpoint",
								m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid BeginSessionInit message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid BeginSessionInit message from peer %s; data expected", m_Peer.GetPeerName().c_str());
		}
		else if (msg.GetMessageType() == MessageType::EndSessionInit &&
				 m_Peer.GetConnectionType() == PeerConnectionType::Inbound)
		{
			Dbg(L"*********** EndSessionInit ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				UInt8 pcounter{ 0 };
				Network::SerializedIPEndpoint pub_endp;
				Vector<SerializedUUID> psextlist;

				BufferReader rdr(buffer, true);
				if (rdr.Read(pcounter, pub_endp, WithSize(psextlist, MaxSize::_UINT16)))
				{
					m_Peer.SetPeerMessageCounter(pcounter);

					if (m_Peer.AddReportedPublicIPEndpoint(pub_endp))
					{
						if (auto pextlist = ValidateExtenderUUIDs(psextlist); pextlist.has_value())
						{
							if (m_Peer.ProcessPeerExtenderUpdate(std::move(*pextlist)))
							{
								result.Success = m_Peer.SetStatus(Status::Ready);
							}
						}
						else LogDbg(L"Invalid EndSessionInit message from peer %s; invalid extender UUID(s)",
									m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Invalid EndSessionInit message from peer %s; invalid public IP endpoint",
								m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid EndSessionInit message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid EndSessionInit message from peer %s; data expected", m_Peer.GetPeerName().c_str());
		}

		return result;
	}

	bool MessageProcessor::GetSignature(Buffer& sig) const
	{
		// If we have a local private key we make a signature
		// otherwise we send an empty signature to try and establish
		// unauthenticated communication if the peer allows it
		if (!m_Peer.GetLocalPrivateKey().IsEmpty())
		{
			if (!MakeSignature(m_Peer.GetLocalUUID(), m_Peer.GetLocalSessionID(),
							   m_Peer.GetLocalPrivateKey(), m_Peer.GetAlgorithms().Hash, sig))
			{
				LogErr(L"Couldn't make signature for authentication with peer %s",
					   m_Peer.GetPeerName().c_str());
				return false;
			}
		}

		return true;
	}

	bool MessageProcessor::MakeSignature(const UUID& uuid, const UInt64 sessionid, const BufferView& priv_key,
										 const Algorithm::Hash ha, Buffer& sig) const
	{
		auto salg = Algorithm::Asymmetric::Unknown;
		switch (uuid.GetSignAlgorithm())
		{
			case UUID::SignAlgorithm::EDDSA_ED25519:
				salg = Algorithm::Asymmetric::EDDSA_ED25519;
				break;
			case UUID::SignAlgorithm::EDDSA_ED448:
				salg = Algorithm::Asymmetric::EDDSA_ED448;
				break;
			default:
				assert(false);
				return false;
		}

		const SerializedUUID suuid{ uuid };

		ProtectedBuffer sigdata;
		sigdata += BufferView(reinterpret_cast<const Byte*>(&suuid), sizeof(SerializedUUID));
		sigdata += BufferView(reinterpret_cast<const Byte*>(&sessionid), sizeof(sessionid));

		if (!m_Peer.GetKeyExchange().AddKeyExchangeData(sigdata)) return false;

		return Crypto::HashAndSign(sigdata, salg, priv_key, sig, ha);
	}

	bool MessageProcessor::AuthenticatePeer(const Buffer& psig) const
	{
		// Should have a peer UUID by now
		assert(m_Peer.GetPeerUUID().IsValid());

		if (const auto allowed = m_Peer.GetAccessManager().GetPeerAllowed(m_Peer.GetPeerUUID()); allowed && *allowed)
		{
			const auto authenticated = VerifySignature(psig);
			if (authenticated || (!authenticated && !m_Peer.GetSettings().Local.RequireAuthentication))
			{
				m_Peer.SetAuthenticated(authenticated);
				return true;
			}
			else
			{
				LogErr(L"Peer %s (UUID %s) could not be authenticated; will disconnect",
					   m_Peer.GetPeerName().c_str(), m_Peer.GetPeerUUID().GetString().c_str());
			}
		}
		else
		{
			LogWarn(L"Peer %s (UUID %s) is not allowed; will disconnect",
					m_Peer.GetPeerName().c_str(), m_Peer.GetPeerUUID().GetString().c_str());
		}

		return false;
	}

	bool MessageProcessor::VerifySignature(const Buffer& psig) const
	{
		// Peers may send empty signatures to try
		// unauthenticated communications
		if (!psig.IsEmpty())
		{
			// Do we have a public key for the peer?
			const auto pub_key = m_Peer.GetPeerPublicKey();
			if (pub_key != nullptr)
			{
				// If we have a public key for the peer, verify
				// that it corresponds to the UUID of the peer,
				// and then verify the signature we received
				if (m_Peer.GetPeerUUID().Verify(*pub_key))
				{
					if (VerifySignature(m_Peer.GetPeerUUID(), m_Peer.GetPeerSessionID(),
										*pub_key, m_Peer.GetAlgorithms().Hash, psig))
					{
						return true;
					}
					else LogWarn(L"Authentication signature could not be verified for peer %s using public key for UUID %s",
								 m_Peer.GetPeerName().c_str(), m_Peer.GetPeerUUID().GetString().c_str());
				}
				else LogWarn(L"UUID %s could not be verified with peer public key for peer %s",
							 m_Peer.GetPeerUUID().GetString().c_str(), m_Peer.GetPeerName().c_str());
			}
			else LogInfo(L"No public key found to verify authentication signature from peer %s (UUID %s)",
						 m_Peer.GetPeerName().c_str(), m_Peer.GetPeerUUID().GetString().c_str());
		}
		else LogInfo(L"Peer %s (UUID %s) sent an empty signature to attempt unauthenticated communication",
					 m_Peer.GetPeerName().c_str(), m_Peer.GetPeerUUID().GetString().c_str());

		return false;
	}

	bool MessageProcessor::VerifySignature(const UUID& uuid, const UInt64 sessionid, const BufferView& pub_key,
										   const Algorithm::Hash ha, const Buffer& psig) const
	{
		auto salg = Algorithm::Asymmetric::Unknown;
		switch (uuid.GetSignAlgorithm())
		{
			case UUID::SignAlgorithm::EDDSA_ED25519:
				salg = Algorithm::Asymmetric::EDDSA_ED25519;
				break;
			case UUID::SignAlgorithm::EDDSA_ED448:
				salg = Algorithm::Asymmetric::EDDSA_ED448;
				break;
			default:
				assert(false);
				return false;
		}

		const SerializedUUID suuid{ uuid };

		ProtectedBuffer sigdata;
		sigdata += BufferView(reinterpret_cast<const Byte*>(&suuid), sizeof(SerializedUUID));
		sigdata += BufferView(reinterpret_cast<const Byte*>(&sessionid), sizeof(sessionid));

		if (!m_Peer.GetKeyExchange().AddKeyExchangeData(sigdata)) return false;

		return Crypto::HashAndVerify(sigdata, salg, pub_key, psig, ha);
	}

	std::optional<Vector<ExtenderUUID>> MessageProcessor::ValidateExtenderUUIDs(const Vector<SerializedUUID>& sextlist) const noexcept
	{
		try
		{
			Vector<ExtenderUUID> extlist;
			extlist.reserve(sextlist.size());

			// Check if the UUIDs are valid
			for (const auto& suuid : sextlist)
			{
				const ExtenderUUID uuid{ suuid };
				if (uuid.GetType() == UUID::Type::Extender)
				{
					extlist.emplace_back(uuid);
				}
				else return std::nullopt;
			}

			if (Util::RemoveDuplicates(extlist))
			{
				return { std::move(extlist) };
			}
		}
		catch (...) {}

		return std::nullopt;
	}

	MessageProcessor::Result MessageProcessor::ProcessKeyExchange(MessageDetails&& msg) const
	{
		MessageProcessor::Result result;

		if ((msg.GetMessageType() == MessageType::BeginPrimaryKeyExchange ||
			 msg.GetMessageType() == MessageType::BeginPrimaryKeyUpdateExchange) &&
			m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
		{
			Dbg(L"*********** BeginPrimaryKey(*)Exchange ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				ProtectedBuffer phsdata;

				BufferReader rdr(buffer, true);
				if (rdr.Read(WithSize(phsdata, MaxSize::_2MB)))
				{
					if (Crypto::ValidateBuffer(phsdata))
					{
						if (m_Peer.GetKeyExchange().GeneratePrimaryAsymmetricKeys(m_Peer.GetAlgorithms(),
																				  Crypto::AsymmetricKeyOwner::Bob))
						{
							m_Peer.GetKeyExchange().SetPeerPrimaryHandshakeData(std::move(phsdata));

							if (m_Peer.GetKeyExchange().GeneratePrimarySymmetricKeyPair(m_Peer.GetGlobalSharedSecret(),
																						m_Peer.GetAlgorithms(),
																						m_Peer.GetConnectionType()))
							{
								if (m_Peer.GetKeys().AddSymmetricKeyPair(m_Peer.GetKeyExchange().GetPrimarySymmetricKeyPair()))
								{
									const auto& lhsdata = m_Peer.GetKeyExchange().GetPrimaryHandshakeData();

									// Should already have data
									assert(!lhsdata.IsEmpty());

									BufferWriter wrt(true);
									if (wrt.WriteWithPreallocation(WithSize(lhsdata, MaxSize::_2MB)))
									{
										if (msg.GetMessageType() == MessageType::BeginPrimaryKeyExchange)
										{
											result.Success = m_Peer.SendWithRandomDelay(MessageType::EndPrimaryKeyExchange,
																						wrt.MoveWrittenBytes(),
																						m_Peer.GetHandshakeDelayPerMessage()).Succeeded();
										}
										else result.Success = m_Peer.Send(MessageType::EndPrimaryKeyUpdateExchange,
																		  wrt.MoveWrittenBytes()).Succeeded();

										if (!result.Success)
										{
											LogDbg(L"Couldn't send EndPrimaryKey(*)Exchange message to peer %s",
												   m_Peer.GetPeerName().c_str());
										}
									}
									else LogDbg(L"Couldn't prepare EndPrimaryKey(*)Exchange message for peer %s",
												m_Peer.GetPeerName().c_str());
								}
								else LogDbg(L"Couldn't add symmetric keys for peer %s", m_Peer.GetPeerName().c_str());
							}
							else LogDbg(L"Couldn't generate symmetric keys for peer %s", m_Peer.GetPeerName().c_str());
						}
						else LogDbg(L"Couldn't generate primary asymmetric keys for peer %s", m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Couldn't validate primary handshake data for peer %s", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid BeginPrimaryKey(*)Exchange message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid BeginPrimaryKey(*)Exchange message from peer %s; data expected",
						m_Peer.GetPeerName().c_str());
		}
		else if ((msg.GetMessageType() == MessageType::EndPrimaryKeyExchange ||
				  msg.GetMessageType() == MessageType::EndPrimaryKeyUpdateExchange) &&
				 m_Peer.GetConnectionType() == PeerConnectionType::Inbound)
		{
			Dbg(L"*********** EndPrimaryKey(*)Exchange ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				ProtectedBuffer phsdata;

				BufferReader rdr(buffer, true);
				if (rdr.Read(WithSize(phsdata, MaxSize::_2MB)))
				{
					if (Crypto::ValidateBuffer(phsdata))
					{
						m_Peer.GetKeyExchange().SetPeerPrimaryHandshakeData(std::move(phsdata));

						if (m_Peer.GetKeyExchange().GeneratePrimarySymmetricKeyPair(m_Peer.GetGlobalSharedSecret(),
																					m_Peer.GetAlgorithms(),
																					m_Peer.GetConnectionType()))
						{
							m_Peer.GetKeyExchange().StartUsingPrimarySymmetricKeyPairForEncryption();

							if (m_Peer.GetKeys().AddSymmetricKeyPair(m_Peer.GetKeyExchange().GetPrimarySymmetricKeyPair()))
							{
								if (m_Peer.GetKeyExchange().GenerateSecondaryAsymmetricKeys(m_Peer.GetAlgorithms(),
																							Crypto::AsymmetricKeyOwner::Alice))
								{
									const auto& lhsdata = m_Peer.GetKeyExchange().GetSecondaryHandshakeData();

									// Should already have a public key
									assert(!lhsdata.IsEmpty());

									BufferWriter wrt(true);
									if (wrt.WriteWithPreallocation(WithSize(lhsdata, MaxSize::_2MB)))
									{
										if (msg.GetMessageType() == MessageType::EndPrimaryKeyExchange)
										{
											result.Success = m_Peer.SendWithRandomDelay(MessageType::BeginSecondaryKeyExchange,
																						wrt.MoveWrittenBytes(),
																						m_Peer.GetHandshakeDelayPerMessage()).Succeeded();
										}
										else result.Success = m_Peer.Send(MessageType::BeginSecondaryKeyUpdateExchange,
																		  wrt.MoveWrittenBytes()).Succeeded();

										if (!result.Success)
										{
											LogDbg(L"Couldn't send BeginSecondaryKey(*)Exchange message to peer %s",
												   m_Peer.GetPeerName().c_str());
										}
									}
									else LogDbg(L"Couldn't prepare BeginSecondaryKey(*)Exchange message for peer %s",
												m_Peer.GetPeerName().c_str());
								}
								else LogDbg(L"Couldn't generate secondary asymmetric keys for peer %s",
											m_Peer.GetPeerName().c_str());
							}
							else LogDbg(L"Couldn't add symmetric keys for peer %s", m_Peer.GetPeerName().c_str());
						}
						else LogDbg(L"Couldn't generate symmetric keys for peer %s", m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Couldn't validate primary handshake data for peer %s", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid EndPrimaryKey(*)Exchange message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid EndPrimaryKey(*)Exchange message from peer %s; data expected", m_Peer.GetPeerName().c_str());
		}
		else if ((msg.GetMessageType() == MessageType::BeginSecondaryKeyExchange ||
				  msg.GetMessageType() == MessageType::BeginSecondaryKeyUpdateExchange) &&
				 m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
		{
			Dbg(L"*********** BeginSecondaryKey(*)Exchange ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				ProtectedBuffer shsdata;

				BufferReader rdr(buffer, true);
				if (rdr.Read(WithSize(shsdata, MaxSize::_2MB)))
				{
					if (Crypto::ValidateBuffer(shsdata))
					{
						m_Peer.GetKeyExchange().StartUsingPrimarySymmetricKeyPairForEncryption();

						if (m_Peer.GetKeyExchange().GenerateSecondaryAsymmetricKeys(m_Peer.GetAlgorithms(),
																					Crypto::AsymmetricKeyOwner::Bob))
						{
							m_Peer.GetKeyExchange().SetPeerSecondaryHandshakeData(std::move(shsdata));

							if (m_Peer.GetKeyExchange().GenerateSecondarySymmetricKeyPair(m_Peer.GetGlobalSharedSecret(),
																						  m_Peer.GetAlgorithms(),
																						  m_Peer.GetConnectionType()))
							{
								if (m_Peer.GetKeys().AddSymmetricKeyPair(m_Peer.GetKeyExchange().GetSecondarySymmetricKeyPair()))
								{
									const auto& lhsdata = m_Peer.GetKeyExchange().GetSecondaryHandshakeData();

									// Should already have a public key
									assert(!lhsdata.IsEmpty());

									BufferWriter wrt(true);
									if (wrt.WriteWithPreallocation(WithSize(lhsdata, MaxSize::_2MB)))
									{
										if (msg.GetMessageType() == MessageType::BeginSecondaryKeyExchange)
										{
											result.Success = m_Peer.SendWithRandomDelay(MessageType::EndSecondaryKeyExchange,
																						wrt.MoveWrittenBytes(),
																						m_Peer.GetHandshakeDelayPerMessage()).Succeeded();
										}
										else result.Success = m_Peer.Send(MessageType::EndSecondaryKeyUpdateExchange,
																		  wrt.MoveWrittenBytes()).Succeeded();

										if (!result.Success)
										{
											LogDbg(L"Couldn't send EndSecondaryKey(*)Exchange message to peer %s",
												   m_Peer.GetPeerName().c_str());
										}
									}
									else LogDbg(L"Couldn't prepare EndSecondaryKey(*)Exchange message for peer %s",
												m_Peer.GetPeerName().c_str());
								}
								else LogDbg(L"Couldn't add symmetric keys for peer %s", m_Peer.GetPeerName().c_str());
							}
							else LogDbg(L"Couldn't generate symmetric keys for peer %s", m_Peer.GetPeerName().c_str());
						}
						else LogDbg(L"Couldn't generate secondary asymmetric keys for peer %s", m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Couldn't validate secondary handshake data for peer %s", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid BeginSecondaryKey(*)Exchange message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid BeginSecondaryKey(*)Exchange message from peer %s; data expected",
						m_Peer.GetPeerName().c_str());
		}
		else if ((msg.GetMessageType() == MessageType::EndSecondaryKeyExchange ||
				  msg.GetMessageType() == MessageType::EndSecondaryKeyUpdateExchange) &&
				 m_Peer.GetConnectionType() == PeerConnectionType::Inbound)
		{
			Dbg(L"*********** EndSecondaryKey(*)Exchange ***********");

			result.Handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				ProtectedBuffer shsdata;

				BufferReader rdr(buffer, true);
				if (rdr.Read(WithSize(shsdata, MaxSize::_2MB)))
				{
					if (Crypto::ValidateBuffer(shsdata))
					{
						m_Peer.GetKeyExchange().SetPeerSecondaryHandshakeData(std::move(shsdata));

						if (m_Peer.GetKeyExchange().GenerateSecondarySymmetricKeyPair(m_Peer.GetGlobalSharedSecret(),
																					  m_Peer.GetAlgorithms(),
																					  m_Peer.GetConnectionType()))
						{
							if (m_Peer.GetKeys().AddSymmetricKeyPair(m_Peer.GetKeyExchange().GetSecondarySymmetricKeyPair()))
							{

								// From now on we encrypt messages using the secondary
								// symmetric key-pair, which the other peer already has
								m_Peer.GetKeyExchange().StartUsingSecondarySymmetricKeyPairForEncryption();

								result.Success = true;
							}
							else LogDbg(L"Couldn't add symmetric keys for peer %s", m_Peer.GetPeerName().c_str());
						}
						else LogDbg(L"Couldn't generate symmetric keys for peer %s", m_Peer.GetPeerName().c_str());
					}
					else LogDbg(L"Couldn't validate secondary handshake data for peer %s", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid EndSecondaryKey(*)Exchange message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid EndSecondaryKey(*)Exchange message from peer %s; data expected",
						m_Peer.GetPeerName().c_str());
		}

		return result;
	}
}