// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerReceiveQueues.h"
#include "Peer.h"

namespace QuantumGate::Implementation::Core::Peer
{
	bool PeerReceiveQueues::ShouldDeferMessage(const Message& msg) const noexcept
	{
		auto canadd = false;

		switch (msg.GetMessageType())
		{
			case MessageType::ExtenderCommunication:
				canadd = m_Peer.GetMessageRateLimits().CanAdd<MessageRateLimits::Type::ExtenderCommunicationReceive>(
					msg.GetMessageData().GetSize());
				break;
			case MessageType::RelayData:
				canadd = m_Peer.GetMessageRateLimits().CanAdd<MessageRateLimits::Type::RelayDataReceive>(
					msg.GetMessageData().GetSize());
				break;
			default:
				canadd = m_Peer.GetMessageRateLimits().CanAdd<MessageRateLimits::Type::Default>(msg.GetMessageData().GetSize());
				break;
		}

		// Messages have to be processed in the order in which they are received, so
		// if the queues aren't empty then the messages need to go to the back
		// of the queue even if there's room in the receive rate limit
		return (HaveMessages() || !canadd);
	}

	bool PeerReceiveQueues::CanProcessNextDeferredMessage() const noexcept
	{
		assert(!m_DeferredQueue.empty());

		switch (m_DeferredQueue.front().GetMessageType())
		{
			case MessageType::ExtenderCommunication:
				return m_Peer.GetMessageRateLimits().CanAdd<MessageRateLimits::Type::ExtenderCommunicationReceive>(
					m_DeferredQueue.front().GetMessageData().GetSize());
			case MessageType::RelayData:
				return m_Peer.GetMessageRateLimits().CanAdd<MessageRateLimits::Type::RelayDataReceive>(
					m_DeferredQueue.front().GetMessageData().GetSize());
			default:
				return m_Peer.GetMessageRateLimits().CanAdd<MessageRateLimits::Type::Default>(
					m_DeferredQueue.front().GetMessageData().GetSize());
		}

		return false;
	}

	void PeerReceiveQueues::AddMessageRate(const MessageType type, const Size msg_size)
	{
		switch (type)
		{
			case MessageType::ExtenderCommunication:
				m_Peer.GetMessageRateLimits().Add<MessageRateLimits::Type::ExtenderCommunicationReceive>(msg_size);
				break;
			case MessageType::RelayData:
				m_Peer.GetMessageRateLimits().Add<MessageRateLimits::Type::RelayDataReceive>(msg_size);
				break;
			default:
				m_Peer.GetMessageRateLimits().Add<MessageRateLimits::Type::Default>(msg_size);
				break;
		}
	}

	void PeerReceiveQueues::SubtractMessageRate(const MessageType type, const Size msg_size)
	{
		switch (type)
		{
			case MessageType::ExtenderCommunication:
				m_Peer.GetMessageRateLimits().Subtract<MessageRateLimits::Type::ExtenderCommunicationReceive>(msg_size);
				break;
			case MessageType::RelayData:
				m_Peer.GetMessageRateLimits().Subtract<MessageRateLimits::Type::RelayDataReceive>(msg_size);
				break;
			default:
				m_Peer.GetMessageRateLimits().Subtract<MessageRateLimits::Type::Default>(msg_size);
				break;
		}
	}
}