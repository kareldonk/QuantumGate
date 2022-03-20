// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IPEndpoint.h"
#include "BTHEndpoint.h"
#include "IMFEndpoint.h"

namespace QuantumGate::Implementation::Network
{
	class Export Endpoint
	{
	public:
		enum class Type : UInt8 { Unspecified, IP, BTH, IMF };
		
		using AddressFamily = Network::AddressFamily;
		using Protocol = Network::Protocol;

		constexpr Endpoint() noexcept :
			m_Type(Type::Unspecified), m_Dummy(0)
		{}

		constexpr Endpoint(const Endpoint& other) noexcept(noexcept(CopyConstruct(other))) :
			m_Type(CopyConstruct(other))
		{}

		constexpr Endpoint(Endpoint&& other) noexcept(noexcept(MoveConstruct(std::move(other)))) :
			m_Type(MoveConstruct(std::move(other)))
		{}

		template<typename T> requires (std::is_same_v<std::decay_t<T>, IPEndpoint> ||
									   std::is_same_v<std::decay_t<T>, BTHEndpoint> ||
									   std::is_same_v<std::decay_t<T>, IMFEndpoint>)
		constexpr Endpoint(T&& endpoint) noexcept(noexcept(Construct(std::forward<T>(endpoint)))) :
			m_Type(Construct(std::forward<T>(endpoint)))
		{}

		constexpr ~Endpoint()
		{
			Destroy();
		}

		constexpr Endpoint& operator=(const Endpoint& other) noexcept(noexcept(CopyAssign(other)))
		{
			// Check for same object
			if (this == &other) return *this;

			m_Type = CopyAssign(other);

			return *this;
		}

		constexpr Endpoint& operator=(Endpoint&& other) noexcept(noexcept(MoveAssign(std::move(other))))
		{
			// Check for same object
			if (this == &other) return *this;

			m_Type = MoveAssign(std::move(other));

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
					case Type::IMF:
						return (m_IMFEndpoint == other.m_IMFEndpoint);
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
				case Type::IMF:
					return IMF::AddressFamilyToNetwork(m_IMFEndpoint.GetIMFAddress().GetFamily());
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
				case Type::IMF:
					return IMF::ProtocolToNetwork(m_IMFEndpoint.GetProtocol());
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

		[[nodiscard]] constexpr const IMFEndpoint& GetIMFEndpoint() const noexcept
		{
			assert(m_Type == Type::IMF);

			return m_IMFEndpoint;
		}

		[[nodiscard]] constexpr RelayPort GetRelayPort() const noexcept
		{
			switch (m_Type)
			{
				case Type::IP:
					return m_IPEndpoint.GetRelayPort();
				case Type::BTH:
					return m_BTHEndpoint.GetRelayPort();
				case Type::IMF:
					return m_IMFEndpoint.GetRelayPort();
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
				case Type::IMF:
					return m_IMFEndpoint.GetRelayHop();
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
		constexpr void Destroy() noexcept
		{
			switch (m_Type)
			{
				case Type::IP:
					Destroy(&m_IPEndpoint);
					break;
				case Type::BTH:
					Destroy(&m_BTHEndpoint);
					break;
				case Type::IMF:
					Destroy(&m_IMFEndpoint);
					break;
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}
		}

		template<typename T>
		constexpr inline void Destroy(T* v) noexcept
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				std::destroy_at(v);
			}
		}

		[[nodiscard]] constexpr Type CopyConstruct(const Endpoint& other)
		{
			switch (other.m_Type)
			{
				case Type::IP:
					return Construct(other.m_IPEndpoint);
				case Type::BTH:
					return Construct(other.m_BTHEndpoint);
				case Type::IMF:
					return Construct(other.m_IMFEndpoint);
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Dummy = 0;

			return Type::Unspecified;
		}

		[[nodiscard]] constexpr Type CopyAssign(const Endpoint& other)
		{
			if (m_Type != other.m_Type)
			{
				Destroy();

				return CopyConstruct(other);
			}

			switch (other.m_Type)
			{
				case Type::IP:
					m_IPEndpoint = other.m_IPEndpoint;
					return other.m_Type;
				case Type::BTH:
					m_BTHEndpoint = other.m_BTHEndpoint;
					return other.m_Type;
				case Type::IMF:
					m_IMFEndpoint = other.m_IMFEndpoint;
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

		[[nodiscard]] constexpr Type MoveConstruct(Endpoint&& other) noexcept
		{
			const auto type = std::exchange(other.m_Type, Type::Unspecified);

			switch (type)
			{
				case Type::IP:
					return Construct(std::move(other.m_IPEndpoint));
				case Type::BTH:
					return Construct(std::move(other.m_BTHEndpoint));
				case Type::IMF:
					return Construct(std::move(other.m_IMFEndpoint));
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Dummy = 0;

			return Type::Unspecified;
		}

		[[nodiscard]] constexpr Type MoveAssign(Endpoint&& other) noexcept
		{
			if (m_Type != other.m_Type)
			{
				Destroy();

				return MoveConstruct(std::move(other));
			}

			const auto type = std::exchange(other.m_Type, Type::Unspecified);

			switch (type)
			{
				case Type::IP:
					m_IPEndpoint = std::move(other.m_IPEndpoint);
					return type;
				case Type::BTH:
					m_BTHEndpoint = std::move(other.m_BTHEndpoint);
					return type;
				case Type::IMF:
					m_IMFEndpoint = std::move(other.m_IMFEndpoint);
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
		[[nodiscard]] constexpr Type Construct(T&& other) noexcept
		{
			std::construct_at(std::addressof(m_IPEndpoint), std::forward<T>(other));
			
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
		[[nodiscard]] constexpr Type Construct(T&& other) noexcept
		{
			std::construct_at(std::addressof(m_BTHEndpoint), std::forward<T>(other));

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

		template<typename T> requires (std::is_same_v<std::decay_t<T>, IMFEndpoint>)
		[[nodiscard]] constexpr Type Construct(T&& other)
		{
			std::construct_at(std::addressof(m_IMFEndpoint), std::forward<T>(other));

			switch (m_IMFEndpoint.GetProtocol())
			{
				case IMFEndpoint::Protocol::IMF:
					return Type::IMF;
				case IMFEndpoint::Protocol::Unspecified:
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
			IMFEndpoint m_IMFEndpoint;
		};
	};
}