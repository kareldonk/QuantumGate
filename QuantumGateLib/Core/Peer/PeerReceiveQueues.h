// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerMessageRateLimits.h"
#include "..\Message.h"
#include "..\..\Concurrency\Queue.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Peer;

	class PeerReceiveQueues final
	{
		using MessageQueue = Concurrency::Queue<Message>;

	public:
		PeerReceiveQueues(Peer& peer) noexcept : m_Peer(peer) {}
		PeerReceiveQueues(const PeerReceiveQueues&) = delete;
		PeerReceiveQueues(PeerReceiveQueues&&) noexcept = default;
		~PeerReceiveQueues() = default;
		PeerReceiveQueues& operator=(const PeerReceiveQueues&) = delete;
		PeerReceiveQueues& operator=(PeerReceiveQueues&&) noexcept = default;

		[[nodiscard]] bool ShouldDeferMessage(const Message& msg) const noexcept;

		[[nodiscard]] bool CanProcessNextDeferredMessage() const noexcept;

		[[nodiscard]] inline bool HaveMessages() const noexcept
		{
			return !m_DeferredQueue.Empty();
		}

		[[nodiscard]] bool DeferMessage(Message&& msg) noexcept
		{
			try
			{
				m_DeferredQueue.Push(std::move(msg));
				return true;
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] Message GetDeferredMessage() noexcept
		{
			assert(!m_DeferredQueue.Empty());

			auto msg = std::move(m_DeferredQueue.Front());
			m_DeferredQueue.Pop();
			return msg;
		}

		void AddMessageRate(const MessageType type, const Size msg_size);
		void SubtractMessageRate(const MessageType type, const Size msg_size);

	private:
		Peer& m_Peer;
		MessageQueue m_DeferredQueue;
	};
}