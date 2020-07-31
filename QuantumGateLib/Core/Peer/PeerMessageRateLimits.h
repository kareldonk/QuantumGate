// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\RateLimit.h"
#include "..\Message.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class MessageRateLimits final
	{
		// Rate limits should be large enough to hold at least one full size message
		// and can be larger to buffer more data at the cost of using more memory
		// per peer connection, and the increased risk of out of memory attacks
		using ExtenderCommunicationSendRateLimit = RateLimit<Size, 0, Message::MaxMessageDataSize>;
		using ExtenderCommunicationReceiveRateLimit = RateLimit<Size, 0, Message::MaxMessageDataSize>;

		using NoiseSendRateLimit = RateLimit<Size, 0, Message::MaxMessageDataSize>;
		
		using RelayDataSendRateLimit = RateLimit<Size, 0, Message::MaxMessageDataSize>;
		using RelayDataReceiveRateLimit = RateLimit<Size, 0, Message::MaxMessageDataSize>;

	public:
		enum Type
		{
			Default,
			ExtenderCommunicationSend,
			ExtenderCommunicationReceive,
			NoiseSend,
			RelayDataSend,
			RelayDataReceive
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
			else if constexpr (type == Type::RelayDataReceive)
			{
				return m_RelayDataReceive.CanAdd(num);
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
			else if constexpr (type == Type::RelayDataReceive)
			{
				m_RelayDataReceive.Add(num);
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
			else if constexpr (type == Type::RelayDataReceive)
			{
				m_RelayDataReceive.Subtract(num);
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
			else if constexpr (type == Type::RelayDataReceive)
			{
				return m_RelayDataReceive.GetAvailable();
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
		RelayDataReceiveRateLimit m_RelayDataReceive;
	};
}