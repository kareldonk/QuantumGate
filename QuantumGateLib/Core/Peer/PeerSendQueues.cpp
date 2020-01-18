// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerSendQueues.h"
#include "Peer.h"

namespace QuantumGate::Implementation::Core::Peer
{
	Result<> PeerSendQueues::AddMessage(Message&& msg, const SendParameters::PriorityOption priority,
										const std::chrono::milliseconds delay, SendCallback&& callback) noexcept
	{
		switch (msg.GetMessageType())
		{
			case MessageType::ExtenderCommunication:
				return AddMessageImpl<MessageRateLimits::Type::ExtenderCommunication>(std::move(msg), priority, delay, std::move(callback));
			case MessageType::Noise:
				return AddMessageImpl<MessageRateLimits::Type::Noise>(std::move(msg), priority, delay, std::move(callback));
			case MessageType::RelayData:
				return AddMessageImpl<MessageRateLimits::Type::RelayData>(std::move(msg), priority, delay, std::move(callback));
			default:
				return AddMessageImpl<MessageRateLimits::Type::Default>(std::move(msg), priority, delay, std::move(callback));
		}
	}

	template<MessageRateLimits::Type type>
	Result<> PeerSendQueues::AddMessageImpl(Message&& msg, const SendParameters::PriorityOption priority,
											const std::chrono::milliseconds delay, SendCallback&& callback) noexcept
	{
		const auto msg_size = msg.GetMessageData().GetSize();

		if (!m_RateLimits.CanAdd<type>(msg_size))
		{
			return ResultCode::PeerSendBufferFull;
		}

		try
		{
			switch (priority)
			{
				case SendParameters::PriorityOption::Normal:
					m_NormalQueue.Push(DefaultMessage{ std::move(msg), std::move(callback) });
					break;
				case SendParameters::PriorityOption::Expedited:
					m_ExpeditedQueue.Push(DefaultMessage{ std::move(msg), std::move(callback) });
					break;
				case SendParameters::PriorityOption::Delayed:
					m_DelayedQueue.Push(DelayedMessage{ std::move(msg), Util::GetCurrentSteadyTime(), delay, std::move(callback) });
					break;
				default:
					// Shouldn't get here
					assert(false);
					break;
			}

			m_RateLimits.Add<type>(msg_size);
		}
		catch (...)
		{
			// Likely out of memory
			return ResultCode::OutOfMemory;
		}

		return ResultCode::Succeeded;
	}

	template<typename T>
	void PeerSendQueues::RemoveMessage(T& queue) noexcept
	{
		auto result = std::invoke([&]()
		{
			if constexpr (std::is_same_v<T, MessageQueue> || std::is_same_v<T, DelayedMessageQueue>)
			{
				return std::make_tuple(queue.Front().Message.GetMessageType(),
									   queue.Front().Message.GetMessageData().GetSize(),
									   std::move(queue.Front().SendCallback));
			}
			else
			{
				static_assert(false, "No match for queue type.");
			}
		});

		queue.Pop();

		auto& [message_type, data_size, send_callback] = result;

		switch (message_type)
		{
			case MessageType::ExtenderCommunication:
				m_RateLimits.Subtract<MessageRateLimits::Type::ExtenderCommunication>(data_size);
				break;
			case MessageType::Noise:
				m_RateLimits.Subtract<MessageRateLimits::Type::Noise>(data_size);
				break;
			case MessageType::RelayData:
				m_RateLimits.Subtract<MessageRateLimits::Type::RelayData>(data_size);
				break;
			default:
				break;
		}

		if (send_callback) m_Peer.ScheduleCallback(std::move(send_callback));
	}

	std::pair<bool, Size> PeerSendQueues::GetMessages(Buffer& buffer, const Crypto::SymmetricKeyData& symkey,
													  const bool concatenate)
	{
		// Expedited queue messages always go first
		if (!m_ExpeditedQueue.Empty())
		{
			return GetExpeditedMessages(buffer, symkey);
		}

		auto success = true;
		auto stop = false;
		Size num{ 0 };

		Buffer tempbuf;

		// We keep filling the message transport buffer as much as possible
		// for efficiency when allowed; note that priority is given to
		// normal messages and delayed messages (noise etc.) get sent when
		// there's room left in the message transport buffer. This is to
		// give priority and bandwidth to real traffic when it's busy

		while (!m_NormalQueue.Empty())
		{
			auto& msg = m_NormalQueue.Front();
			if (msg.Message.Write(tempbuf, symkey))
			{
				if (buffer.GetSize() + tempbuf.GetSize() <= MessageTransport::MaxMessageDataSize)
				{
					buffer += tempbuf;
					RemoveMessage(m_NormalQueue);

					++num;

					// Only one message gets written if we shouldn't
					// concatenate messages (yet)
					if (!concatenate)
					{
						stop = true;
						break;
					}
				}
				else
				{
					// Message buffer is full
					stop = true;
					break;
				}
			}
			else
			{
				// Write error
				success = false;
				break;
			}
		}

		if (success && !stop)
		{
			while (!m_DelayedQueue.Empty())
			{
				auto& dmsg = m_DelayedQueue.Front();
				if (dmsg.IsTime())
				{
					if (dmsg.Message.Write(tempbuf, symkey))
					{
						if (buffer.GetSize() + tempbuf.GetSize() <= MessageTransport::MaxMessageDataSize)
						{
							buffer += tempbuf;
							RemoveMessage(m_DelayedQueue);

							++num;

							// Only one message gets written if we shouldn't
							// concatenate messages (yet)
							if (!concatenate)
							{
								break;
							}
						}
						else
						{
							// Message buffer is full
							break;
						}
					}
					else
					{
						// Write error
						success = false;
						break;
					}
				}
				else
				{
					// It's not time yet to send delayed message;
					// we'll come back later
					break;
				}
			}
		}

		DbgInvoke([&]()
		{
			if (num > 1)
			{
				LogDbg(L"Sent %zu messages in one transport", num);
			}
		});

		return std::make_pair(success, num);
	}

	std::pair<bool, Size> PeerSendQueues::GetExpeditedMessages(Buffer& buffer,
															   const Crypto::SymmetricKeyData& symkey) noexcept
	{
		assert(!m_ExpeditedQueue.Empty());

		auto success = true;
		Size num{ 0 };

		// Note that we only send one message with every message transport
		// and don't concatenate messages in order to minimize delays both
		// in processing and in transmission. Obviously less efficient but
		// this is a tradeoff when speed is needed such as in real-time
		// communications

		auto& msg = m_ExpeditedQueue.Front();
		if (msg.Message.Write(buffer, symkey))
		{
			RemoveMessage(m_ExpeditedQueue);

			++num;
		}
		else
		{
			success = false;
		}

		return std::make_pair(success, num);
	}
}