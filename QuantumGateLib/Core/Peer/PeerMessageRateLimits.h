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
		struct Type final
		{
			struct Default final {};
			struct ExtenderCommunicationSend final {};
			struct ExtenderCommunicationReceive final {};
			struct NoiseSend final {};
			struct RelayDataSend final {};
			struct RelayDataReceive final {};
		};

		template<typename T>
		[[nodiscard]] constexpr inline bool CanAdd(const Size num) const noexcept
		{
			if constexpr (std::is_same_v<T, Type::ExtenderCommunicationSend>)
			{
				return m_ExtenderCommunicationSend.CanAdd(num);
			}
			else if constexpr (std::is_same_v<T, Type::ExtenderCommunicationReceive>)
			{
				return m_ExtenderCommunicationReceive.CanAdd(num);
			}
			else if constexpr (std::is_same_v<T, Type::NoiseSend>)
			{
				return m_NoiseSend.CanAdd(num);
			}
			else if constexpr (std::is_same_v<T, Type::RelayDataSend>)
			{
				return m_RelayDataSend.CanAdd(num);
			}
			else if constexpr (std::is_same_v<T, Type::RelayDataReceive>)
			{
				return m_RelayDataReceive.CanAdd(num);
			}
			else if constexpr (std::is_same_v<T, Type::Default>)
			{
				return true;
			}
			else
			{
				static_assert(AlwaysFalse<T>, "Unsupported type.");
			}
		}

		template<typename T>
		constexpr inline void Add(const Size num) noexcept
		{
			if constexpr (std::is_same_v<T, Type::ExtenderCommunicationSend>)
			{
				m_ExtenderCommunicationSend.Add(num);
			}
			else if constexpr (std::is_same_v<T, Type::ExtenderCommunicationReceive>)
			{
				m_ExtenderCommunicationReceive.Add(num);
			}
			else if constexpr (std::is_same_v<T, Type::NoiseSend>)
			{
				m_NoiseSend.Add(num);
			}
			else if constexpr (std::is_same_v<T, Type::RelayDataSend>)
			{
				m_RelayDataSend.Add(num);
			}
			else if constexpr (std::is_same_v<T, Type::RelayDataReceive>)
			{
				m_RelayDataReceive.Add(num);
			}
			else if constexpr (std::is_same_v<T, Type::Default>)
			{
			}
			else
			{
				static_assert(AlwaysFalse<T>, "Unsupported type.");
			}
		}

		template<typename T>
		constexpr inline void Subtract(const Size num) noexcept
		{
			if constexpr (std::is_same_v<T, Type::ExtenderCommunicationSend>)
			{
				m_ExtenderCommunicationSend.Subtract(num);
			}
			else if constexpr (std::is_same_v<T, Type::ExtenderCommunicationReceive>)
			{
				m_ExtenderCommunicationReceive.Subtract(num);
			}
			else if constexpr (std::is_same_v<T, Type::NoiseSend>)
			{
				m_NoiseSend.Subtract(num);
			}
			else if constexpr (std::is_same_v<T, Type::RelayDataSend>)
			{
				m_RelayDataSend.Subtract(num);
			}
			else if constexpr (std::is_same_v<T, Type::RelayDataReceive>)
			{
				m_RelayDataReceive.Subtract(num);
			}
			else if constexpr (std::is_same_v<T, Type::Default>)
			{
			}
			else
			{
				static_assert(AlwaysFalse<T>, "Unsupported type.");
			}
		}

		template<typename T>
		[[nodiscard]] constexpr inline Size GetAvailable() const noexcept
		{
			if constexpr (std::is_same_v<T, Type::ExtenderCommunicationSend>)
			{
				return m_ExtenderCommunicationSend.GetAvailable();
			}
			else if constexpr (std::is_same_v<T, Type::ExtenderCommunicationReceive>)
			{
				return m_ExtenderCommunicationReceive.GetAvailable();
			}
			else if constexpr (std::is_same_v<T, Type::NoiseSend>)
			{
				return m_NoiseSend.GetAvailable();
			}
			else if constexpr (std::is_same_v<T, Type::RelayDataSend>)
			{
				return m_RelayDataSend.GetAvailable();
			}
			else if constexpr (std::is_same_v<T, Type::RelayDataReceive>)
			{
				return m_RelayDataReceive.GetAvailable();
			}
			else
			{
				static_assert(AlwaysFalse<T>, "Unsupported type.");
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