// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\RateLimit.h"
#include "..\Message.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class MessageRateLimits final
	{
		// Enough space to hold 5 full size messages (and more smaller ones)
		using ExtenderCommunicationRateLimit = RateLimit<Size, 0, 5 * Message::MaxMessageDataSize>;

		// Enough space to hold 1 full size message (and more smaller ones)
		using NoiseRateLimit = RateLimit<Size, 0, 1 * Message::MaxMessageDataSize>;

		// Enough space to hold 5 full size messages (and more smaller ones)
		using RelayDataRateLimit = RateLimit<Size, 0, 5 * Message::MaxMessageDataSize>;

	public:
		enum Type
		{
			Default,
			ExtenderCommunication,
			Noise,
			RelayData
		};

		template<Type type>
		[[nodiscard]] constexpr inline bool CanAdd(const Size num) const noexcept
		{
			if constexpr (type == Type::ExtenderCommunication)
			{
				return m_ExtenderCommunication.CanAdd(num);
			}
			else if constexpr (type == Type::Noise)
			{
				return m_Noise.CanAdd(num);
			}
			else if constexpr (type == Type::RelayData)
			{
				return m_RelayData.CanAdd(num);
			}
			else if constexpr (type == Type::Default)
			{
				return true;
			}
			else
			{
				static_assert(false, "No match for message type.");
			}
		}

		template<Type type>
		constexpr inline void Add(const Size num) noexcept
		{
			if constexpr (type == Type::ExtenderCommunication)
			{
				m_ExtenderCommunication.Add(num);
			}
			else if constexpr (type == Type::Noise)
			{
				m_Noise.Add(num);
			}
			else if constexpr (type == Type::RelayData)
			{
				m_RelayData.Add(num);
			}
			else if constexpr (type == Type::Default)
			{
			}
			else
			{
				static_assert(false, "No match for message type.");
			}
		}

		template<Type type>
		constexpr inline void Subtract(const Size num) noexcept
		{
			if constexpr (type == Type::ExtenderCommunication)
			{
				m_ExtenderCommunication.Subtract(num);
			}
			else if constexpr (type == Type::Noise)
			{
				m_Noise.Subtract(num);
			}
			else if constexpr (type == Type::RelayData)
			{
				m_RelayData.Subtract(num);
			}
			else if constexpr (type == Type::Default)
			{
			}
			else
			{
				static_assert(false, "No match for message type.");
			}
		}

		template<Type type>
		[[nodiscard]] constexpr inline Size GetAvailable() const noexcept
		{
			if constexpr (type == Type::ExtenderCommunication)
			{
				return m_ExtenderCommunication.GetAvailable();
			}
			else if constexpr (type == Type::Noise)
			{
				return m_Noise.GetAvailable();
			}
			else if constexpr (type == Type::RelayData)
			{
				return m_RelayData.GetAvailable();
			}
			else
			{
				static_assert(false, "No match for message type.");
			}
		}

	private:
		ExtenderCommunicationRateLimit m_ExtenderCommunication;
		NoiseRateLimit m_Noise;
		RelayDataRateLimit m_RelayData;
	};
}