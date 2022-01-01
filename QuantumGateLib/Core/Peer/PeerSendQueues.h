// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerMessageRateLimits.h"
#include "..\..\Concurrency\Queue.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Peer;

	class PeerSendQueues final
	{
		struct DefaultMessage final
		{
			Message Message;
			SendCallback SendCallback{ nullptr };
		};

		struct DelayedMessage final
		{
			Message Message;
			SteadyTime ScheduleSteadyTime;
			std::chrono::milliseconds Delay{ 0 };
			SendCallback SendCallback{ nullptr };

			[[nodiscard]] inline bool IsTime() const noexcept
			{
				if ((Util::GetCurrentSteadyTime() - ScheduleSteadyTime) >= Delay) return true;

				return false;
			}
		};

		using MessageQueue = Containers::Queue<DefaultMessage>;
		using DelayedMessageQueue = Containers::Queue<DelayedMessage>;

	public:
		PeerSendQueues(Peer& peer) noexcept : m_Peer(peer) {}
		PeerSendQueues(const PeerSendQueues&) = delete;
		PeerSendQueues(PeerSendQueues&&) noexcept = default;
		~PeerSendQueues() = default;
		PeerSendQueues& operator=(const PeerSendQueues&) = delete;
		PeerSendQueues& operator=(PeerSendQueues&&) noexcept = default;

		[[nodiscard]] inline bool HaveMessages() const noexcept
		{
			return (!m_NormalQueue.empty() || !m_ExpeditedQueue.empty() ||
				(!m_DelayedQueue.empty() && m_DelayedQueue.front().IsTime()));
		}

		Result<> AddMessage(Message&& msg, const SendParameters::PriorityOption priority,
							const std::chrono::milliseconds delay, SendCallback&& callback) noexcept;

		[[nodiscard]] std::pair<bool, Size> GetMessages(Buffer& buffer, const Crypto::SymmetricKeyData& symkey,
														const bool concatenate) noexcept;

		[[nodiscard]] Size GetAvailableExtenderCommunicationBufferSize() const noexcept;
		[[nodiscard]] Size GetAvailableRelayDataBufferSize() const noexcept;
		[[nodiscard]] Size GetAvailableNoiseBufferSize() const noexcept;

	private:
		template<typename T>
		Result<> AddMessageImpl(Message&& msg, const SendParameters::PriorityOption priority,
								const std::chrono::milliseconds delay, SendCallback&& callback) noexcept;

		template<typename T>
		void RemoveMessage(T& queue) noexcept;

		[[nodiscard]] std::pair<bool, Size> GetExpeditedMessages(Buffer& buffer,
																 const Crypto::SymmetricKeyData& symkey) noexcept;

	private:
		Peer& m_Peer;
		MessageQueue m_NormalQueue;
		MessageQueue m_ExpeditedQueue;
		DelayedMessageQueue m_DelayedQueue;
	};
}