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
		PeerReceiveQueues() noexcept = default;
		PeerReceiveQueues(const PeerReceiveQueues&) = delete;
		PeerReceiveQueues(PeerReceiveQueues&&) noexcept = default;
		~PeerReceiveQueues() = default;
		PeerReceiveQueues& operator=(const PeerReceiveQueues&) = delete;
		PeerReceiveQueues& operator=(PeerReceiveQueues&&) noexcept = default;

		[[nodiscard]] inline bool HaveMessages() const noexcept
		{
			return !m_NormalQueue.Empty();
		}

		[[nodiscard]] bool AddMessage(Message&& msg) noexcept
		{
			try
			{
				m_NormalQueue.Push(std::move(msg));
				return true;
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] Message GetMessage() noexcept
		{
			assert(!m_NormalQueue.Empty());

			auto msg = std::move(m_NormalQueue.Front());
			m_NormalQueue.Pop();
			return msg;
		}

		[[nodiscard]] Size GetNextMessageSize() noexcept
		{
			assert(!m_NormalQueue.Empty());
			return m_NormalQueue.Front().GetMessageData().GetSize();
		}

	private:
		MessageQueue m_NormalQueue;
	};
}