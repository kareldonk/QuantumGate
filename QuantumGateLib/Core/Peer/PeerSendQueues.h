// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerMessageRateLimits.h"
#include "..\..\Concurrency\Queue.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class PeerSendQueues final
	{
		struct DefaultMessage final
		{
			Message Message;
			SendCallback SendCallback{ nullptr };
		};

		struct DelayedMessage final
		{
			[[nodiscard]] inline bool IsTime() const noexcept
			{
				if ((Util::GetCurrentSteadyTime() - ScheduleSteadyTime) >= Delay) return true;

				return false;
			}

			Message Message;
			SteadyTime ScheduleSteadyTime;
			std::chrono::milliseconds Delay{ 0 };
			SendCallback SendCallback{ nullptr };
		};

		using MessageQueue = Concurrency::Queue<DefaultMessage>;
		using DelayedMessageQueue = Concurrency::Queue<DelayedMessage>;

	public:
		PeerSendQueues() noexcept = default;
		PeerSendQueues(const PeerSendQueues&) = delete;
		PeerSendQueues(PeerSendQueues&&) noexcept = default;
		~PeerSendQueues() = default;
		PeerSendQueues& operator=(const PeerSendQueues&) = delete;
		PeerSendQueues& operator=(PeerSendQueues&&) noexcept = default;

		[[nodiscard]] inline bool HaveMessages() const noexcept
		{
			return (!m_NormalQueue.Empty() || !m_ExpeditedQueue.Empty() ||
				(!m_DelayedQueue.Empty() && m_DelayedQueue.Front().IsTime()));
		}

		Result<> AddMessage(Message&& msg, const SendParameters::PriorityOption priority,
							const std::chrono::milliseconds delay, SendCallback&& callback) noexcept;

		[[nodiscard]] std::pair<bool, Size> GetMessages(Buffer& buffer, const Crypto::SymmetricKeyData& symkey,
														const bool concatenate);

		[[nodiscard]] inline Size GetAvailableNoiseSendBufferSize() const noexcept
		{
			return m_RateLimits.GetAvailable<MessageRateLimits::Type::Noise>();
		}

		[[nodiscard]] inline Size GetAvailableRelayDataSendBufferSize() const noexcept
		{
			return m_RateLimits.GetAvailable<MessageRateLimits::Type::RelayData>();
		}

		[[nodiscard]] inline Size GetAvailableExtenderCommunicationSendBufferSize() const noexcept
		{
			return m_RateLimits.GetAvailable<MessageRateLimits::Type::ExtenderCommunication>();
		}

	private:
		template<MessageRateLimits::Type type>
		Result<> AddMessageImpl(Message&& msg, const SendParameters::PriorityOption priority,
								const std::chrono::milliseconds delay, SendCallback&& callback) noexcept;

		template<typename T>
		void RemoveMessage(T& queue) noexcept;

		[[nodiscard]] std::pair<bool, Size> GetExpeditedMessages(Buffer& buffer,
																 const Crypto::SymmetricKeyData& symkey) noexcept;

	private:
		MessageQueue m_NormalQueue;
		MessageQueue m_ExpeditedQueue;
		DelayedMessageQueue m_DelayedQueue;
		MessageRateLimits m_RateLimits;
	};
}