// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerMessageDetails.h"
#include "..\Relay\RelaySocket.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Peer;

	class MessageProcessor final
	{
	public:
		struct Result
		{
			bool Handled{ false };
			bool Success{ false };
		};

		MessageProcessor() = delete;
		MessageProcessor(Peer& peer) noexcept : m_Peer(peer) {}
		MessageProcessor(const MessageProcessor&) = delete;
		MessageProcessor(MessageProcessor&&) noexcept = default;
		~MessageProcessor() = default;
		MessageProcessor& operator=(const MessageProcessor&) = delete;
		MessageProcessor& operator=(MessageProcessor&&) noexcept = default;

		bool SendBeginHandshake() const noexcept;

		bool SendBeginPrimaryKeyUpdateExchange() const noexcept;

		bool SendBeginRelay(const RelayPort rport, const IPEndpoint& endpoint, const RelayHop hops) const noexcept;
		bool SendRelayStatus(const RelayPort rport, const RelayStatusUpdate status) const noexcept;
		QuantumGate::Result<> SendRelayData(const RelayDataMessage& msg) const noexcept;
		bool SendRelayDataAck(const RelayDataAckMessage& msg) const noexcept;
		bool SendEndRelay(const RelayPort rport) const noexcept;

		Result ProcessMessage(MessageDetails&& msg) const;

	private:
		bool SendBeginPrimaryKeyExchange() const noexcept;
		bool SendBeginKeyExchange(const MessageType type) const noexcept;

		Result ProcessMessageMetaExchange(MessageDetails&& msg) const;
		Result ProcessMessagePrimaryKeyExchange(MessageDetails&& msg) const;
		Result ProcessMessageSecondaryKeyExchange(MessageDetails&& msg) const;
		Result ProcessMessageAuthentication(MessageDetails&& msg) const;
		Result ProcessMessageSessionInit(MessageDetails&& msg) const;
		Result ProcessMessageReadyState(MessageDetails&& msg) const;
		
		Result ProcessKeyExchange(MessageDetails&& msg) const;

		bool GetSignature(Buffer& sig) const;

		bool MakeSignature(const UUID& uuid, const UInt64 sessionid, const BufferView& priv_key,
						   const Algorithm::Hash ha, Buffer& sig) const;

		bool AuthenticatePeer(const Buffer& psig) const;

		bool VerifySignature(const Buffer& psig) const;

		bool VerifySignature(const UUID& uuid, const UInt64 sessionid, const BufferView& pub_key,
							 const Algorithm::Hash ha, const Buffer& psig) const;

		std::optional<Vector<ExtenderUUID>> ValidateExtenderUUIDs(const Vector<SerializedUUID>& sextlist) const noexcept;

	private:
		Peer& m_Peer;
	};
}

