// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerMessageProcessor.h"
#include "PeerManager.h"
#include "Peer.h"
#include "..\..\Memory\BufferReader.h"
#include "..\..\Memory\BufferWriter.h"

using namespace QuantumGate::Implementation::Memory;

namespace QuantumGate::Implementation::Core::Peer
{
	const bool MessageProcessor::SendBeginHandshake() const noexcept
	{
		Dbg(L"*********** SendBeginHandshake ***********");

		try
		{
			const auto& algorithms = m_Peer.GetSupportedAlgorithms();

			// TODO: optimize this?
			const auto lhal = Crypto::MakeAlgorithmVector(algorithms.Hash);
			const auto lpaal = Crypto::MakeAlgorithmVector(algorithms.PrimaryAsymmetric);
			const auto lsaal = Crypto::MakeAlgorithmVector(algorithms.SecondaryAsymmetric);
			const auto lsal = Crypto::MakeAlgorithmVector(algorithms.Symmetric);
			const auto lcal = Crypto::MakeAlgorithmVector(algorithms.Compression);

			BufferWriter wrt(true);
			if (wrt.WriteWithPreallocation(m_Peer.GetLocalProtocolVersion().first, m_Peer.GetLocalProtocolVersion().second,
										   WithSize(lhal, MaxSize::_256B), WithSize(lpaal, MaxSize::_256B),
										   WithSize(lsaal, MaxSize::_256B), WithSize(lsal, MaxSize::_256B),
										   WithSize(lcal, MaxSize::_256B)))
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

	const bool MessageProcessor::SendBeginPrimaryKeyExchange() const noexcept
	{
		Dbg(L"*********** SendBeginPrimaryKeyExchange ***********");

		return SendBeginKeyExchange(MessageType::BeginPrimaryKeyExchange);
	}

	const bool MessageProcessor::SendBeginPrimaryKeyUpdateExchange() const noexcept
	{
		Dbg(L"*********** SendBeginPrimaryKeyUpdateExchange ***********");

		return SendBeginKeyExchange(MessageType::BeginPrimaryKeyUpdateExchange);
	}

	const bool MessageProcessor::SendBeginKeyExchange(const MessageType type) const noexcept
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
						if (m_Peer.SendWithRandomDelay(type, wrt.MoveWrittenBytes(),
													   m_Peer.GetHandshakeDelayPerMessage())) return true;
						break;
					}
					case MessageType::BeginPrimaryKeyUpdateExchange:
					{
						if (m_Peer.Send(type, wrt.MoveWrittenBytes())) return true;
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

	const std::pair<bool, bool> MessageProcessor::ProcessMessage(const MessageDetails& msg) const
	{
		switch (m_Peer.GetStatus())
		{
			case Status::MetaExchange:
				return ProcessMessageMetaExchange(msg);
			case Status::PrimaryKeyExchange:
				return ProcessMessagePrimaryKeyExchange(msg);
			case Status::SecondaryKeyExchange:
				return ProcessMessageSecondaryKeyExchange(msg);
			case Status::Authentication:
				return ProcessMessageAuthentication(msg);
			case Status::SessionInit:
				return ProcessMessageSessionInit(msg);
			case Status::Ready:
				return ProcessMessageReadyState(msg);
			default:
				break;
		}

		// Not handled, unsuccessful
		return std::make_pair(false, false);
	}

	const std::pair<bool, bool> MessageProcessor::ProcessMessageMetaExchange(const MessageDetails& msg) const
	{
		auto handled = false;
		auto success = false;

		if (msg.GetMessageType() == MessageType::BeginMetaExchange &&
			m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
		{
			Dbg(L"*********** BeginMetaExchange ***********");

			handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				UInt8 v1{ 0 };
				UInt8 v2{ 0 };
				std::vector<Algorithm::Hash> phal;
				std::vector<Algorithm::Asymmetric> ppaal;
				std::vector<Algorithm::Asymmetric> psaal;
				std::vector<Algorithm::Symmetric> psal;
				std::vector<Algorithm::Compression> pcal;

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
								success = m_Peer.SetStatus(Status::PrimaryKeyExchange);
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

			handled = true;

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
							success = m_Peer.SetStatus(Status::PrimaryKeyExchange);
						}
					}
					else LogDbg(L"Couldn't set encryption algorithms for peer %s", m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid EndMetaExchange message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid EndMetaExchange message from peer %s; data expected", m_Peer.GetPeerName().c_str());
		}

		return std::make_pair(handled, success);
	}

	const std::pair<bool, bool> MessageProcessor::ProcessMessagePrimaryKeyExchange(const MessageDetails& msg) const
	{
		auto retval = std::make_pair(false, false);

		if (msg.GetMessageType() == MessageType::BeginPrimaryKeyExchange ||
			msg.GetMessageType() == MessageType::EndPrimaryKeyExchange)
		{
			retval = ProcessKeyExchange(msg);
			if (retval.first && retval.second)
			{
				retval.second = m_Peer.SetStatus(Status::SecondaryKeyExchange);
			}
		}

		return retval;
	}

	const std::pair<bool, bool> MessageProcessor::ProcessMessageSecondaryKeyExchange(const MessageDetails& msg) const
	{
		auto retval = std::make_pair(false, false);

		if (msg.GetMessageType() == MessageType::BeginSecondaryKeyExchange)
		{
			retval = ProcessKeyExchange(msg);
			if (retval.first && retval.second)
			{
				retval.second = m_Peer.SetStatus(Status::Authentication);
			}
		}
		else if (msg.GetMessageType() == MessageType::EndSecondaryKeyExchange)
		{
			retval = ProcessKeyExchange(msg);
			if (retval.first && retval.second)
			{
				retval.second = false;
				auto sent = false;

				Buffer sig;
				if (GetSignature(sig))
				{
					BufferWriter wrt(true);
					if (wrt.WriteWithPreallocation(SerializedUUID{ m_Peer.GetLocalUUID() }, m_Peer.GetLocalSessionID(),
												   WithSize(sig, MaxSize::UInt16)))
					{
						if (m_Peer.SendWithRandomDelay(MessageType::BeginAuthentication, wrt.MoveWrittenBytes(),
													   m_Peer.GetHandshakeDelayPerMessage()))
						{
							sent = true;
							retval.second = m_Peer.SetStatus(Status::Authentication);
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

		return retval;
	}

	const std::pair<bool, bool> MessageProcessor::ProcessMessageAuthentication(const MessageDetails& msg) const
	{
		auto handled = false;
		auto success = false;

		if (msg.GetMessageType() == MessageType::BeginAuthentication &&
			m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
		{
			Dbg(L"*********** BeginAuthentication ***********");

			handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				SerializedUUID spuuid;
				UInt64 psessionid{ 0 };
				Buffer psig;

				BufferReader rdr(buffer, true);
				if (rdr.Read(spuuid, psessionid, WithSize(psig, MaxSize::UInt16)))
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
															   WithSize(sig, MaxSize::UInt16)))
								{
									if (m_Peer.SendWithRandomDelay(MessageType::EndAuthentication, wrt.MoveWrittenBytes(),
																   m_Peer.GetHandshakeDelayPerMessage()))
									{
										success = m_Peer.SetStatus(Status::SessionInit);
									}
								}
							}

							if (!success)
							{
								LogDbg(L"Couldn't send EndAuthentication message to peer %s",
									   m_Peer.GetPeerName().c_str());
							}
						}
						else
						{
							// Peer could not be authenticated; disconnect asap
							m_Peer.SetDisconnectCondition(DisconnectCondition::PeerNotAllowed);
							success = true;
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

			handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				SerializedUUID spuuid;
				UInt64 psessionid{ 0 };
				Buffer psig;

				BufferReader rdr(buffer, true);
				if (rdr.Read(spuuid, psessionid, WithSize(psig, MaxSize::UInt16)))
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
														   Network::SerializedIPEndpoint{ m_Peer.GetPeerEndpoint() },
														   WithSize(lsextlist, MaxSize::UInt16)))
							{
								if (m_Peer.Send(MessageType::BeginSessionInit, wrt.MoveWrittenBytes()))
								{
									success = m_Peer.SetStatus(Status::SessionInit);
								}
							}

							if (!success)
							{
								LogDbg(L"Couldn't send BeginSessionInit message to peer %s",
									   m_Peer.GetPeerName().c_str());
							}
						}
						else
						{
							// Peer could not be authenticated; disconnect asap
							m_Peer.SetDisconnectCondition(DisconnectCondition::PeerNotAllowed);
							success = true;
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

		return std::make_pair(handled, success);
	}

	const std::pair<bool, bool> MessageProcessor::ProcessMessageSessionInit(const MessageDetails& msg) const
	{
		auto handled = false;
		auto success = false;

		if (msg.GetMessageType() == MessageType::BeginSessionInit &&
			m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
		{
			Dbg(L"*********** BeginSessionInit ***********");

			handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				UInt8 pcounter{ 0 };
				Network::SerializedIPEndpoint pub_endp;
				std::vector<SerializedUUID> psextlist;

				BufferReader rdr(buffer, true);
				if (rdr.Read(pcounter, pub_endp, WithSize(psextlist, MaxSize::UInt16)))
				{
					m_Peer.SetPeerMessageCounter(pcounter);

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
														   Network::SerializedIPEndpoint{ m_Peer.GetPeerEndpoint() },
														   WithSize(lsextlist, MaxSize::UInt16)))
							{
								if (m_Peer.Send(MessageType::EndSessionInit, wrt.MoveWrittenBytes()))
								{
									success = m_Peer.SetStatus(Status::Ready);
								}
							}

							if (!success)
							{
								LogDbg(L"Couldn't send EndSessionInit message to peer %s",
									   m_Peer.GetPeerName().c_str());
							}
						}
					}
					else LogDbg(L"Invalid BeginSessionInit message from peer %s; invalid extender UUID(s)",
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

			handled = true;

			if (auto& buffer = msg.GetMessageData(); !buffer.IsEmpty())
			{
				UInt8 pcounter{ 0 };
				Network::SerializedIPEndpoint pub_endp;
				std::vector<SerializedUUID> psextlist;

				BufferReader rdr(buffer, true);
				if (rdr.Read(pcounter, pub_endp, WithSize(psextlist, MaxSize::UInt16)))
				{
					m_Peer.SetPeerMessageCounter(pcounter);

					if (auto pextlist = ValidateExtenderUUIDs(psextlist); pextlist.has_value())
					{
						if (m_Peer.ProcessPeerExtenderUpdate(std::move(*pextlist)))
						{
							success = m_Peer.SetStatus(Status::Ready);
						}
					}
					else LogDbg(L"Invalid EndSessionInit message from peer %s; invalid extender UUID(s)",
								m_Peer.GetPeerName().c_str());
				}
				else LogDbg(L"Invalid EndSessionInit message from peer %s; couldn't read message data",
							m_Peer.GetPeerName().c_str());
			}
			else LogDbg(L"Invalid EndSessionInit message from peer %s; data expected", m_Peer.GetPeerName().c_str());
		}

		return std::make_pair(handled, success);
	}

	const bool MessageProcessor::GetSignature(Buffer& sig) const
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

	const bool MessageProcessor::MakeSignature(const UUID& uuid, const UInt64 sessionid, const BufferView& priv_key,
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

		SerializedUUID suuid{ uuid };

		ProtectedBuffer sigdata;
		sigdata += BufferView(reinterpret_cast<const Byte*>(&suuid), sizeof(SerializedUUID));
		sigdata += BufferView(reinterpret_cast<const Byte*>(&sessionid), sizeof(sessionid));

		if (!m_Peer.GetKeyExchange().AddKeyExchangeData(sigdata)) return false;

		return Crypto::HashAndSign(sigdata, salg, priv_key, sig, ha);
	}

	const bool MessageProcessor::AuthenticatePeer(const Buffer& psig) const
	{
		// Should have a peer UUID by now
		assert(m_Peer.GetPeerUUID().IsValid());

		if (const auto allowed = m_Peer.GetAccessManager().IsPeerAllowed(m_Peer.GetPeerUUID()); allowed && *allowed)
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

	const bool MessageProcessor::VerifySignature(const Buffer& psig) const
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

	const bool MessageProcessor::VerifySignature(const UUID& uuid, const UInt64 sessionid, const BufferView& pub_key,
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

		SerializedUUID suuid{ uuid };

		ProtectedBuffer sigdata;
		sigdata += BufferView(reinterpret_cast<const Byte*>(&suuid), sizeof(SerializedUUID));
		sigdata += BufferView(reinterpret_cast<const Byte*>(&sessionid), sizeof(sessionid));

		if (!m_Peer.GetKeyExchange().AddKeyExchangeData(sigdata)) return false;

		return Crypto::HashAndVerify(sigdata, salg, pub_key, psig, ha);
	}

	std::optional<std::vector<ExtenderUUID>> MessageProcessor::ValidateExtenderUUIDs(const std::vector<SerializedUUID>& sextlist) const noexcept
	{
		try
		{
			std::vector<ExtenderUUID> extlist;

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

			return { std::move(extlist) };
		}
		catch (...) {}

		return std::nullopt;
	}

	const std::pair<bool, bool> MessageProcessor::ProcessKeyExchange(const MessageDetails& msg) const
	{
		auto handled = false;
		auto success = false;

		if ((msg.GetMessageType() == MessageType::BeginPrimaryKeyExchange ||
			 msg.GetMessageType() == MessageType::BeginPrimaryKeyUpdateExchange) &&
			m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
		{
			Dbg(L"*********** BeginPrimaryKey(*)Exchange ***********");

			handled = true;

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
											success = m_Peer.SendWithRandomDelay(MessageType::EndPrimaryKeyExchange,
																				 wrt.MoveWrittenBytes(),
																				 m_Peer.GetHandshakeDelayPerMessage());
										}
										else success = m_Peer.Send(MessageType::EndPrimaryKeyUpdateExchange,
																   wrt.MoveWrittenBytes());

										if (!success)
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

			handled = true;

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
											success = m_Peer.SendWithRandomDelay(MessageType::BeginSecondaryKeyExchange,
																				 wrt.MoveWrittenBytes(),
																				 m_Peer.GetHandshakeDelayPerMessage());
										}
										else success = m_Peer.Send(MessageType::BeginSecondaryKeyUpdateExchange,
																   wrt.MoveWrittenBytes());

										if (!success)
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

			handled = true;

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
											success = m_Peer.SendWithRandomDelay(MessageType::EndSecondaryKeyExchange,
																				 wrt.MoveWrittenBytes(),
																				 m_Peer.GetHandshakeDelayPerMessage());
										}
										else success = m_Peer.Send(MessageType::EndSecondaryKeyUpdateExchange,
																   wrt.MoveWrittenBytes());

										if (!success)
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

			handled = true;

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

								success = true;
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

		return std::make_pair(handled, success);
	}
}