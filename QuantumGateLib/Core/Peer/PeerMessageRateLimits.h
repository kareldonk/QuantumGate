// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\RateLimit.h"
#include "..\Message.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class MessageRateLimits final
	{
		// Enough space to hold 1 full size message (and more smaller ones)
		using ExtenderCommunicationSendRateLimit = RateLimit<Size, 0, Message::MaxMessageDataSize>;

		// Enough space to hold 1 full size message (and more smaller ones)
		using ExtenderCommunicationReceiveRateLimit = RateLimit<Size, 0, 2 * Message::MaxMessageDataSize>;

		// Enough space to hold 1 full size message (and more smaller ones)
		using NoiseSendRateLimit = RateLimit<Size, 0, Message::MaxMessageDataSize>;

		// Enough space to hold 1 full size message (and more smaller ones)
		using RelayDataSendRateLimit = RateLimit<Size, 0, Message::MaxMessageDataSize>;

	public:
		enum Type
		{
			Default,
			ExtenderCommunicationSend,
			ExtenderCommunicationReceive,
			NoiseSend,
			RelayDataSend
		};

		template<Type type>
		[[nodiscard]] constexpr inline bool CanAdd(const Size num) const noexcept
		{
			if constexpr (type == Type::ExtenderCommunicationSend)
			{
				return m_ExtenderCommunicationSend.CanAdd(num);
			}
			else if constexpr (type == Type::ExtenderCommunicationReceive)
			{
				return m_ExtenderCommunicationReceive.CanAdd(num);
			}
			else if constexpr (type == Type::NoiseSend)
			{
				return m_NoiseSend.CanAdd(num);
			}
			else if constexpr (type == Type::RelayDataSend)
			{
				return m_RelayDataSend.CanAdd(num);
			}
			else if constexpr (type == Type::Default)
			{
				return true;
			}
			else
			{
				assert(false);
			}
		}

		template<Type type>
		constexpr inline void Add(const Size num) noexcept
		{
			if constexpr (type == Type::ExtenderCommunicationSend)
			{
				m_ExtenderCommunicationSend.Add(num);
			}
			else if constexpr (type == Type::ExtenderCommunicationReceive)
			{
				m_ExtenderCommunicationReceive.Add(num);
			}
			else if constexpr (type == Type::NoiseSend)
			{
				m_NoiseSend.Add(num);
			}
			else if constexpr (type == Type::RelayDataSend)
			{
				m_RelayDataSend.Add(num);
			}
			else if constexpr (type == Type::Default)
			{
			}
			else
			{
				assert(false);
			}
		}

		template<Type type>
		constexpr inline void Subtract(const Size num) noexcept
		{
			if constexpr (type == Type::ExtenderCommunicationSend)
			{
				m_ExtenderCommunicationSend.Subtract(num);
			}
			else if constexpr (type == Type::ExtenderCommunicationReceive)
			{
				m_ExtenderCommunicationReceive.Subtract(num);
			}
			else if constexpr (type == Type::NoiseSend)
			{
				m_NoiseSend.Subtract(num);
			}
			else if constexpr (type == Type::RelayDataSend)
			{
				m_RelayDataSend.Subtract(num);
			}
			else if constexpr (type == Type::Default)
			{
			}
			else
			{
				assert(false);
			}
		}

		template<Type type>
		[[nodiscard]] constexpr inline Size GetAvailable() const noexcept
		{
			if constexpr (type == Type::ExtenderCommunicationSend)
			{
				return m_ExtenderCommunicationSend.GetAvailable();
			}
			else if constexpr (type == Type::ExtenderCommunicationReceive)
			{
				return m_ExtenderCommunicationReceive.GetAvailable();
			}
			else if constexpr (type == Type::NoiseSend)
			{
				return m_NoiseSend.GetAvailable();
			}
			else if constexpr (type == Type::RelayDataSend)
			{
				return m_RelayDataSend.GetAvailable();
			}
			else
			{
				assert(false);
			}
		}

	private:
		ExtenderCommunicationSendRateLimit m_ExtenderCommunicationSend;
		ExtenderCommunicationReceiveRateLimit m_ExtenderCommunicationReceive;
		NoiseSendRateLimit m_NoiseSend;
		RelayDataSendRateLimit m_RelayDataSend;
	};
}