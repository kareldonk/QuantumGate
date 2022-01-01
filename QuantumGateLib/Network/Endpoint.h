// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IPEndpoint.h"
#include "BTHEndpoint.h"

namespace QuantumGate::Implementation::Network
{
	class Export Endpoint
	{
	public:
		enum class Type : UInt8 { Unspecified, IP, BTH };
		
		using AddressFamily = Network::AddressFamily;
		using Protocol = Network::Protocol;

		constexpr Endpoint() noexcept :
			m_Type(Type::Unspecified), m_Dummy(0)
		{}

		constexpr Endpoint(const Endpoint& other) noexcept :
			m_Type(Copy(other))
		{}

		constexpr Endpoint(Endpoint&& other) noexcept :
			m_Type(Move(std::move(other)))
		{}

		template<typename T> requires (std::is_same_v<std::decay_t<T>, IPEndpoint> || std::is_same_v<std::decay_t<T>, BTHEndpoint>)
		constexpr Endpoint(T&& endpoint) noexcept :
			m_Type(CopyOrMove(std::forward<T>(endpoint)))
		{}

		~Endpoint() = default; // Defaulted since union member destructors don't have to be called

		constexpr Endpoint& operator=(const Endpoint& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Type = Copy(other);

			return *this;
		}

		constexpr Endpoint& operator=(Endpoint&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Type = Move(std::move(other));

			return *this;
		}

		constexpr bool operator==(const Endpoint& other) const noexcept
		{
			if (m_Type == other.m_Type)
			{
				switch (m_Type)
				{
					case Type::IP:
						return (m_IPEndpoint == other.m_IPEndpoint);
					case Type::BTH:
						return (m_BTHEndpoint == other.m_BTHEndpoint);
					case Type::Unspecified:
						return true;
					default:
						assert(false);
						break;
				}
			}

			return false;
		}

		constexpr bool operator!=(const Endpoint& other) const noexcept
		{
			return !(*this == other);
		}

		[[nodiscard]] constexpr Type GetType() const noexcept { return m_Type; }

		[[nodiscard]] constexpr AddressFamily GetAddressFamily() const noexcept
		{
			switch (m_Type)
			{
				case Type::IP:
					return IP::AddressFamilyToNetwork(m_IPEndpoint.GetIPAddress().GetFamily());
				case Type::BTH:
					return BTH::AddressFamilyToNetwork(m_BTHEndpoint.GetBTHAddress().GetFamily());
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			return AddressFamily::Unspecified;
		}

		[[nodiscard]] constexpr Protocol GetProtocol() const noexcept
		{
			switch (m_Type)
			{
				case Type::IP:
					return IP::ProtocolToNetwork(m_IPEndpoint.GetProtocol());
				case Type::BTH:
					return BTH::ProtocolToNetwork(m_BTHEndpoint.GetProtocol());
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			return Protocol::Unspecified;
		}

		[[nodiscard]] constexpr const IPEndpoint& GetIPEndpoint() const noexcept
		{
			assert(m_Type == Type::IP);

			return m_IPEndpoint;
		}

		[[nodiscard]] constexpr const BTHEndpoint& GetBTHEndpoint() const noexcept
		{
			assert(m_Type == Type::BTH);

			return m_BTHEndpoint;
		}

		[[nodiscard]] constexpr RelayPort GetRelayPort() const noexcept
		{
			switch (m_Type)
			{
				case Type::IP:
					return m_IPEndpoint.GetRelayPort();
				case Type::BTH:
					return m_BTHEndpoint.GetRelayPort();
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			return 0;
		}

		[[nodiscard]] constexpr RelayHop GetRelayHop() const noexcept
		{
			switch (m_Type)
			{
				case Type::IP:
					return m_IPEndpoint.GetRelayHop();
				case Type::BTH:
					return m_BTHEndpoint.GetRelayHop();
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			return 0;
		}

		[[nodiscard]] String GetString() const noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const Endpoint& endpoint);
		friend Export std::wostream& operator<<(std::wostream& stream, const Endpoint& endpoint);

	private:
		[[nodiscard]] constexpr Type Copy(const Endpoint& other) noexcept
		{
			switch (other.m_Type)
			{
				case Type::IP:
					m_IPEndpoint = other.m_IPEndpoint;
					return other.m_Type;
				case Type::BTH:
					m_BTHEndpoint = other.m_BTHEndpoint;
					return other.m_Type;
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Dummy = 0;

			return Type::Unspecified;
		}

		[[nodiscard]] constexpr Type Move(Endpoint&& other) noexcept
		{
			const auto type = std::exchange(other.m_Type, Type::Unspecified);

			switch (type)
			{
				case Type::IP:
					m_IPEndpoint = std::move(other.m_IPEndpoint);
					return type;
				case Type::BTH:
					m_BTHEndpoint = std::move(other.m_BTHEndpoint);
					return type;
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Dummy = 0;

			return Type::Unspecified;
		}

		template<typename T> requires (std::is_same_v<std::decay_t<T>, IPEndpoint>)
		[[nodiscard]] constexpr Type CopyOrMove(T&& other) noexcept
		{
			m_IPEndpoint = std::forward<T>(other);
			
			switch (m_IPEndpoint.GetProtocol())
			{
				case IPEndpoint::Protocol::TCP:
				case IPEndpoint::Protocol::UDP:
				case IPEndpoint::Protocol::ICMP:
					return Type::IP;
				case IPEndpoint::Protocol::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Dummy = 0;

			return Type::Unspecified;
		}

		template<typename T> requires (std::is_same_v<std::decay_t<T>, BTHEndpoint>)
		[[nodiscard]] constexpr Type CopyOrMove(T&& other) noexcept
		{
			m_BTHEndpoint = std::forward<T>(other);

			switch (m_BTHEndpoint.GetProtocol())
			{
				case BTHEndpoint::Protocol::RFCOMM:
					return Type::BTH;
				case BTHEndpoint::Protocol::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Dummy = 0;

			return Type::Unspecified;
		}

	private:
		Type m_Type{ Type::Unspecified };
		union
		{
			int m_Dummy;
			IPEndpoint m_IPEndpoint;
			BTHEndpoint m_BTHEndpoint;
		};
	};
}