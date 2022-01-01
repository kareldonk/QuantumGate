// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerMessageDetails.h"
#include "..\Relay\RelaySocket.h"
#include "..\..\Network\SerializedEndpoint.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Peer;
	class KeyUpdate;

	class MessageProcessor final
	{
		friend KeyUpdate;

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

		[[nodiscard]] bool SendBeginHandshake() const noexcept;

		[[nodiscard]] bool SendBeginRelay(const RelayPort rport, const Endpoint& endpoint, const RelayHop hops) const noexcept;
		QuantumGate::Result<> SendRelayStatus(const RelayPort rport, const RelayStatusUpdate status) const noexcept;
		QuantumGate::Result<> SendRelayData(const RelayDataMessage& msg) const noexcept;
		[[nodiscard]] bool SendRelayDataAck(const RelayDataAckMessage& msg) const noexcept;

		[[nodiscard]] Result ProcessMessage(MessageDetails&& msg) const noexcept;

	private:
		[[nodiscard]] bool SendBeginPrimaryKeyExchange() const noexcept;
		[[nodiscard]] bool SendBeginKeyExchange(const MessageType type) const noexcept;
		[[nodiscard]] bool SendBeginPrimaryKeyUpdateExchange() const noexcept;

		[[nodiscard]] Result ProcessMessageMetaExchange(const MessageDetails&& msg) const noexcept;
		[[nodiscard]] Result ProcessMessagePrimaryKeyExchange(MessageDetails&& msg) const noexcept;
		[[nodiscard]] Result ProcessMessageSecondaryKeyExchange(MessageDetails&& msg) const noexcept;
		[[nodiscard]] Result ProcessMessageAuthentication(const MessageDetails&& msg) const noexcept;
		[[nodiscard]] Result ProcessMessageSessionInit(const MessageDetails&& msg) const noexcept;
		[[nodiscard]] Result ProcessMessageReadyState(MessageDetails&& msg) const noexcept;
		
		[[nodiscard]] Result ProcessKeyExchange(const MessageDetails&& msg) const noexcept;

		[[nodiscard]] bool GetSignature(Buffer& sig) const noexcept;

		[[nodiscard]] bool MakeSignature(const UUID& uuid, const UInt64 sessionid, const BufferView& priv_key,
										 const Algorithm::Hash ha, Buffer& sig) const noexcept;

		[[nodiscard]] bool AuthenticatePeer(const Buffer& psig) const noexcept;

		[[nodiscard]] bool VerifySignature(const Buffer& psig) const noexcept;

		[[nodiscard]] bool VerifySignature(const UUID& uuid, const UInt64 sessionid, const BufferView& pub_key,
										   const Algorithm::Hash ha, const Buffer& psig) const noexcept;

		[[nodiscard]] std::optional<Vector<ExtenderUUID>> ValidateExtenderUUIDs(const Vector<SerializedUUID>& sextlist) const noexcept;

	private:
		Peer& m_Peer;
	};
}

