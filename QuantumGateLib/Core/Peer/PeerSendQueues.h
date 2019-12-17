// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Concurrency\Queue.h"
#include "..\..\Common\RateLimit.h"
#include "..\Message.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class PeerSendQueues final
	{
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
		};

		using MessageQueue = Concurrency::Queue<Message>;
		using DelayedMessageQueue = Concurrency::Queue<DelayedMessage>;

		struct RateLimits final
		{
			// Enough space to hold 5 full size messages (and more smaller ones)
			using ExtenderCommunicationRateLimit = RateLimit<Size, 0, 5 * Message::MaxMessageDataSize>;
			using NoiseRateLimit = RateLimit<Size, 0, 5 * Message::MaxMessageDataSize>;
			using RelayDataRateLimit = RateLimit<Size, 0, 5 * Message::MaxMessageDataSize>;

			ExtenderCommunicationRateLimit ExtenderCommunication;
			NoiseRateLimit Noise;
			RelayDataRateLimit RelayData;
		};

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
							const std::chrono::milliseconds delay) noexcept;

		[[nodiscard]] std::pair<bool, Size> GetMessages(Buffer& buffer, const Crypto::SymmetricKeyData& symkey,
														const bool concatenate);

		[[nodiscard]] inline Size GetAvailableNoiseSendBufferSize() const noexcept
		{
			return m_RateLimits.Noise.GetAvailable();
		}

		[[nodiscard]] inline Size GetAvailableRelayDataSendBufferSize() const noexcept
		{
			return m_RateLimits.RelayData.GetAvailable();
		}

	private:
		template<MessageType MsgType>
		Result<> AddMessageImpl(Message&& msg, const SendParameters::PriorityOption priority,
								const std::chrono::milliseconds delay) noexcept;

		template<typename T>
		void RemoveMessage(T& queue) noexcept;

		[[nodiscard]] std::pair<bool, Size> GetExpeditedMessages(Buffer& buffer,
																 const Crypto::SymmetricKeyData& symkey) noexcept;

	private:
		MessageQueue m_NormalQueue;
		MessageQueue m_ExpeditedQueue;
		DelayedMessageQueue m_DelayedQueue;
		RateLimits m_RateLimits;
	};
}