// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerTypes.h"
#include "..\MessageTypes.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class MessageDetails final
	{
	public:
		MessageDetails(Peer& peer, const MessageType type, const ExtenderUUID& extuuid, Buffer&& msgdata) noexcept;
		MessageDetails(const MessageDetails&) = delete;
		MessageDetails(MessageDetails&&) noexcept = default;
		~MessageDetails();
		MessageDetails& operator=(const MessageDetails&) = delete;
		MessageDetails& operator=(MessageDetails&&) noexcept = default;

		[[nodiscard]] bool AddToMessageData(const Buffer& data) noexcept;

		inline MessageType GetMessageType() const noexcept { return m_MessageType; }
		inline const ExtenderUUID& GetExtenderUUID() const noexcept { return m_ExtenderUUID; }
		inline const Buffer& GetMessageData() const noexcept { return m_MessageData; }

	private:
		MessageType m_MessageType{ MessageType::Unknown };
		ExtenderUUID m_ExtenderUUID;
		Buffer m_MessageData;
		PeerWeakPointer m_PeerPointer;
	};
}