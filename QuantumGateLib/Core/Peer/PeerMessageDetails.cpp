// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerMessageDetails.h"
#include "Peer.h"

namespace QuantumGate::Implementation::Core::Peer
{
	MessageDetails::MessageDetails(Peer& peer, const MessageType type, const ExtenderUUID& extuuid, Buffer&& msgdata) noexcept :
		m_MessageType(type), m_ExtenderUUID(extuuid), m_MessageData(std::move(msgdata)), m_PeerPointer(peer.GetWeakPointer())
	{
		peer.GetMessageRateLimits().Add<MessageRateLimits::Type::ExtenderCommunicationReceive>(m_MessageData.GetSize());
	}

	MessageDetails::~MessageDetails()
	{
		auto peer_ths = m_PeerPointer.lock();
		if (peer_ths)
		{
			peer_ths->WithUniqueLock()->GetMessageRateLimits().Subtract<MessageRateLimits::Type::ExtenderCommunicationReceive>(m_MessageData.GetSize());
		}
	}

	bool MessageDetails::AddToMessageData(const Buffer& data) noexcept
	{
		try
		{
			m_MessageData += data;

			auto peer_ths = m_PeerPointer.lock();
			if (peer_ths)
			{
				peer_ths->WithUniqueLock()->GetMessageRateLimits().Add<MessageRateLimits::Type::ExtenderCommunicationReceive>(data.GetSize());
			}

			return true;
		}
		catch (...) {}

		return false;
	}
}