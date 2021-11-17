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

		constexpr Type GetType() const noexcept { return m_Type; }

		constexpr const IPEndpoint& GetIPEndpoint() const noexcept
		{
			assert(m_Type == Type::IP);

			return m_IPEndpoint;
		}

		constexpr const BTHEndpoint& GetBTHEndpoint() const noexcept
		{
			assert(m_Type == Type::BTH);

			return m_BTHEndpoint;
		}

		constexpr RelayHop GetRelayHop() const noexcept
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

		String GetString() const noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const Endpoint& endpoint);
		friend Export std::wostream& operator<<(std::wostream& stream, const Endpoint& endpoint);

	private:
		constexpr Type Copy(const Endpoint& other) noexcept
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

		constexpr Type Move(Endpoint&& other) noexcept
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
		constexpr Type CopyOrMove(T&& other) noexcept
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
		constexpr Type CopyOrMove(T&& other) noexcept
		{
			m_BTHEndpoint = std::forward<T>(other);

			switch (m_BTHEndpoint.GetProtocol())
			{
				case BTHEndpoint::Protocol::BTH:
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

#pragma pack(push, 1) // Disable padding bytes
	struct SerializedEndpoint final
	{
		Endpoint::Type Type{ Endpoint::Type::Unspecified };
		union
		{
			Byte Dummy[(std::max)(sizeof(SerializedIPEndpoint), sizeof(SerializedBTHEndpoint))]{ Byte{ 0 } };
			SerializedIPEndpoint IPEndpoint;
			SerializedBTHEndpoint BTHEndpoint;
		};

		SerializedEndpoint() noexcept {}
		SerializedEndpoint(const Endpoint& endpoint) noexcept { *this = endpoint; }

		SerializedEndpoint& operator=(const Endpoint& endpoint) noexcept
		{
			Type = endpoint.GetType();
			switch (Type)
			{
				case Endpoint::Type::IP:
					IPEndpoint = endpoint.GetIPEndpoint();
					break;
				case Endpoint::Type::BTH:
					BTHEndpoint = endpoint.GetBTHEndpoint();
					break;
				case Endpoint::Type::Unspecified:
					std::memset(&Dummy, 0, sizeof(Dummy));
					break;
				default:
					assert(false);
					break;
			}
			return *this;
		}

		operator Endpoint() const noexcept
		{
			switch (Type)
			{
				case Endpoint::Type::IP:
					return Network::IPEndpoint(IPEndpoint);
				case Endpoint::Type::BTH:
					return Network::BTHEndpoint(BTHEndpoint);
				case Endpoint::Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}
			return {};
		}

		bool operator==(const SerializedEndpoint& other) const noexcept
		{
			if (Type == other.Type)
			{
				switch (Type)
				{
					case Endpoint::Type::IP:
						return (IPEndpoint == other.IPEndpoint);
					case Endpoint::Type::BTH:
						return (BTHEndpoint == other.BTHEndpoint);
					case Endpoint::Type::Unspecified:
						return (std::memcmp(Dummy, other.Dummy, sizeof(Dummy)) == 0);
					default:
						assert(false);
						break;
				}
			}
			return false;
		}

		bool operator!=(const SerializedEndpoint& other) const noexcept
		{
			return !(*this == other);
		}
	};
#pragma pack(pop)
}