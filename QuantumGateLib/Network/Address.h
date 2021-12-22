// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IPAddress.h"
#include "BTHAddress.h"

namespace QuantumGate::Implementation::Network
{
	class Endpoint;

	class Export Address
	{
	public:
		enum class Type : UInt8 { Unspecified, IP, BTH };

		using Family = Network::AddressFamily;

		constexpr Address() noexcept :
			m_Type(Type::Unspecified), m_Dummy(0)
		{}

		constexpr Address(const Address& other) noexcept :
			m_Type(Copy(other))
		{}

		constexpr Address(Address&& other) noexcept :
			m_Type(Move(std::move(other)))
		{}

		template<typename T> requires (std::is_same_v<std::decay_t<T>, IPAddress> || std::is_same_v<std::decay_t<T>, BTHAddress>)
		constexpr Address(T&& Address) noexcept :
			m_Type(CopyOrMove(std::forward<T>(Address)))
		{}

		constexpr Address(const Endpoint& ep) noexcept
		{
			switch (ep.GetType())
			{
				case Endpoint::Type::IP:
					m_Type = Type::IP;
					m_IPAddress = ep.GetIPEndpoint().GetIPAddress();
					break;
				case Endpoint::Type::BTH:
					m_Type = Type::BTH;
					m_BTHAddress = ep.GetBTHEndpoint().GetBTHAddress();
					break;
				case Endpoint::Type::Unspecified:
					m_Type = Type::Unspecified;
					m_Dummy = 0;
					break;
				default:
					assert(false);
					break;
			}
		}

		~Address() = default; // Defaulted since union member destructors don't have to be called

		constexpr Address& operator=(const Address& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Type = Copy(other);

			return *this;
		}

		constexpr Address& operator=(Address&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Type = Move(std::move(other));

			return *this;
		}

		constexpr bool operator==(const Address& other) const noexcept
		{
			if (m_Type == other.m_Type)
			{
				switch (m_Type)
				{
					case Type::IP:
						return (m_IPAddress == other.m_IPAddress);
					case Type::BTH:
						return (m_BTHAddress == other.m_BTHAddress);
					case Type::Unspecified:
						return true;
					default:
						assert(false);
						break;
				}
			}

			return false;
		}

		constexpr bool operator!=(const Address& other) const noexcept
		{
			return !(*this == other);
		}

		[[nodiscard]] constexpr Type GetType() const noexcept { return m_Type; }

		[[nodiscard]] constexpr Address::Family GetFamily() const noexcept
		{
			switch (m_Type)
			{
				case Type::IP:
					return IP::AddressFamilyToNetwork(m_IPAddress.GetFamily());
				case Type::BTH:
					return BTH::AddressFamilyToNetwork(m_BTHAddress.GetFamily());
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			return Family::Unspecified;
		}

		[[nodiscard]] constexpr const IPAddress& GetIPAddress() const noexcept
		{
			assert(m_Type == Type::IP);

			return m_IPAddress;
		}

		[[nodiscard]] constexpr const BTHAddress& GetBTHAddress() const noexcept
		{
			assert(m_Type == Type::BTH);

			return m_BTHAddress;
		}

		[[nodiscard]] std::size_t GetHash() const noexcept;

		[[nodiscard]] String GetString() const noexcept;

		[[nodiscard]] static bool TryParse(const WChar* addr_str, Address& addr) noexcept;
		[[nodiscard]] static bool TryParse(const String& addr_str, Address& addr) noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const Address& addr);
		friend Export std::wostream& operator<<(std::wostream& stream, const Address& addr);

	private:
		[[nodiscard]] constexpr Type Copy(const Address& other) noexcept
		{
			switch (other.m_Type)
			{
				case Type::IP:
					m_IPAddress = other.m_IPAddress;
					return other.m_Type;
				case Type::BTH:
					m_BTHAddress = other.m_BTHAddress;
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

		[[nodiscard]] constexpr Type Move(Address&& other) noexcept
		{
			const auto type = std::exchange(other.m_Type, Type::Unspecified);

			switch (type)
			{
				case Type::IP:
					m_IPAddress = std::move(other.m_IPAddress);
					return type;
				case Type::BTH:
					m_BTHAddress = std::move(other.m_BTHAddress);
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

		template<typename T> requires (std::is_same_v<std::decay_t<T>, IPAddress>)
		[[nodiscard]] constexpr Type CopyOrMove(T&& other) noexcept
		{
			m_IPAddress = std::forward<T>(other);

			switch (m_IPAddress.GetFamily())
			{
				case IPAddress::Family::IPv4:
				case IPAddress::Family::IPv6:
					return Type::IP;
				case IPAddress::Family::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Dummy = 0;

			return Type::Unspecified;
		}

		template<typename T> requires (std::is_same_v<std::decay_t<T>, BTHAddress>)
		[[nodiscard]] constexpr Type CopyOrMove(T&& other) noexcept
		{
			m_BTHAddress = std::forward<T>(other);

			switch (m_BTHAddress.GetFamily())
			{
				case BTHAddress::Family::BTH:
					return Type::BTH;
				case BTHAddress::Family::Unspecified:
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
			IPAddress m_IPAddress;
			BTHAddress m_BTHAddress;
		};
	};
}

namespace std
{
	// Specialization for standard hash function for Address
	template<> struct hash<QuantumGate::Implementation::Network::Address>
	{
		std::size_t operator()(const QuantumGate::Implementation::Network::Address& addr) const noexcept
		{
			return addr.GetHash();
		}
	};
}