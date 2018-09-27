// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Message.h"
#include "..\Relay\RelaySocket.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Peer;

	class MessageProcessor final
	{
	public:
		MessageProcessor() = delete;
		MessageProcessor(Peer& peer) noexcept : m_Peer(peer) {}
		MessageProcessor(const MessageProcessor&) = delete;
		MessageProcessor(MessageProcessor&&) = default;
		~MessageProcessor() = default;
		MessageProcessor& operator=(const MessageProcessor&) = delete;
		MessageProcessor& operator=(MessageProcessor&&) = default;

		const bool SendBeginHandshake() const noexcept;

		const bool SendBeginPrimaryKeyUpdateExchange() const noexcept;

		const bool SendBeginRelay(const RelayPort rport, const IPEndpoint& endpoint,
								  const RelayHop hops) const noexcept;
		const bool SendRelayStatus(const RelayPort rport, const RelayStatusUpdate status) const noexcept;
		const bool SendRelayData(const RelayPort rport, const Buffer& buffer) const noexcept;
		const bool SendEndRelay(const RelayPort rport) const noexcept;

		const std::pair<bool, bool> ProcessMessage(const MessageDetails& msg) const;

	private:
		const bool SendBeginPrimaryKeyExchange() const noexcept;
		const bool SendBeginKeyExchange(const MessageType type) const noexcept;

		const std::pair<bool, bool> ProcessMessageMetaExchange(const MessageDetails& msg) const;
		const std::pair<bool, bool> ProcessMessagePrimaryKeyExchange(const MessageDetails& msg) const;
		const std::pair<bool, bool> ProcessMessageSecondaryKeyExchange(const MessageDetails& msg) const;
		const std::pair<bool, bool> ProcessMessageAuthentication(const MessageDetails& msg) const;
		const std::pair<bool, bool> ProcessMessageSessionInit(const MessageDetails& msg) const;
		const std::pair<bool, bool> ProcessMessageReadyState(const MessageDetails& msg) const;

		const std::pair<bool, bool> ProcessKeyExchange(const MessageDetails& msg) const;

		const bool GetSignature(Buffer& sig) const;

		const bool MakeSignature(const UUID& uuid, const UInt64 sessionid, const BufferView& priv_key,
								 const Algorithm::Hash ha, Buffer& sig) const;

		const bool AuthenticatePeer(const Buffer& psig) const;

		const bool VerifySignature(const Buffer& psig) const;

		const bool VerifySignature(const UUID& uuid, const UInt64 sessionid, const BufferView& pub_key,
								   const Algorithm::Hash ha, const Buffer& psig) const;

		std::optional<Vector<ExtenderUUID>> ValidateExtenderUUIDs(const Vector<SerializedUUID>& sextlist) const noexcept;

	private:
		Peer& m_Peer;
	};
}

