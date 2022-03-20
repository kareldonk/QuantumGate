// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IPAddress.h"
#include "BTHAddress.h"
#include "IMFAddress.h"

namespace QuantumGate::Implementation::Network
{
	class Endpoint;

	class Export Address
	{
	public:
		enum class Type : UInt8 { Unspecified, IP, BTH, IMF };

		using Family = Network::AddressFamily;

		constexpr Address() noexcept :
			m_Type(Type::Unspecified), m_Dummy(0)
		{}

		constexpr Address(const Address& other) noexcept(noexcept(CopyConstruct(other))) :
			m_Type(CopyConstruct(other))
		{}

		constexpr Address(Address&& other) noexcept(noexcept(MoveConstruct(std::move(other)))) :
			m_Type(MoveConstruct(std::move(other)))
		{}

		template<typename T> requires (std::is_same_v<std::decay_t<T>, IPAddress> ||
									   std::is_same_v<std::decay_t<T>, BTHAddress> ||
									   std::is_same_v<std::decay_t<T>, IMFAddress>)
		constexpr Address(T&& addr) noexcept(noexcept(Construct(std::forward<T>(addr)))) :
			m_Type(Construct(std::forward<T>(addr)))
		{}

		constexpr Address(const Endpoint& ep)
		{
			switch (ep.GetType())
			{
				case Endpoint::Type::IP:
					m_Type = Construct(ep.GetIPEndpoint().GetIPAddress());
					return;
				case Endpoint::Type::BTH:
					m_Type = Construct(ep.GetBTHEndpoint().GetBTHAddress());
					return;
				case Endpoint::Type::IMF:
					m_Type = Construct(ep.GetIMFEndpoint().GetIMFAddress());
					return;
				case Endpoint::Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Type = Type::Unspecified;
			m_Dummy = 0;
		}

		constexpr ~Address()
		{
			Destroy();
		}

		constexpr Address& operator=(const Address& other) noexcept(noexcept(CopyAssign(other)))
		{
			// Check for same object
			if (this == &other) return *this;

			m_Type = CopyAssign(other);

			return *this;
		}

		constexpr Address& operator=(Address&& other) noexcept(noexcept(MoveAssign(std::move(other))))
		{
			// Check for same object
			if (this == &other) return *this;

			m_Type = MoveAssign(std::move(other));

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
					case Type::IMF:
						return (m_IMFAddress == other.m_IMFAddress);
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
				case Type::IMF:
					return IMF::AddressFamilyToNetwork(m_IMFAddress.GetFamily());
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

		[[nodiscard]] constexpr const IMFAddress& GetIMFAddress() const noexcept
		{
			assert(m_Type == Type::IMF);

			return m_IMFAddress;
		}

		[[nodiscard]] std::size_t GetHash() const noexcept;

		[[nodiscard]] String GetString() const noexcept;

		[[nodiscard]] static bool TryParse(const WChar* addr_str, Address& addr) noexcept;
		[[nodiscard]] static bool TryParse(const String& addr_str, Address& addr) noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const Address& addr);
		friend Export std::wostream& operator<<(std::wostream& stream, const Address& addr);

	private:
		constexpr void Destroy() noexcept
		{
			switch (m_Type)
			{
				case Type::IP:
					Destroy(&m_IPAddress);
					break;
				case Type::BTH:
					Destroy(&m_BTHAddress);
					break;
				case Type::IMF:
					Destroy(&m_IMFAddress);
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

		[[nodiscard]] constexpr Type CopyConstruct(const Address& other)
		{
			switch (other.m_Type)
			{
				case Type::IP:
					return Construct(other.m_IPAddress);
				case Type::BTH:
					return Construct(other.m_BTHAddress);
				case Type::IMF:
					return Construct(other.m_IMFAddress);
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Dummy = 0;

			return Type::Unspecified;
		}

		[[nodiscard]] constexpr Type CopyAssign(const Address& other)
		{
			if (m_Type != other.m_Type)
			{
				Destroy();

				return CopyConstruct(other);
			}

			switch (other.m_Type)
			{
				case Type::IP:
					m_IPAddress = other.m_IPAddress;
					return other.m_Type;
				case Type::BTH:
					m_BTHAddress = other.m_BTHAddress;
					return other.m_Type;
				case Type::IMF:
					m_IMFAddress = other.m_IMFAddress;
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

		[[nodiscard]] constexpr Type MoveConstruct(Address&& other) noexcept
		{
			const auto type = std::exchange(other.m_Type, Type::Unspecified);

			switch (type)
			{
				case Type::IP:
					return Construct(std::move(other.m_IPAddress));
				case Type::BTH:
					return Construct(std::move(other.m_BTHAddress));
				case Type::IMF:
					return Construct(std::move(other.m_IMFAddress));
				case Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}

			m_Dummy = 0;

			return Type::Unspecified;
		}

		[[nodiscard]] constexpr Type MoveAssign(Address&& other) noexcept
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
					m_IPAddress = std::move(other.m_IPAddress);
					return type;
				case Type::BTH:
					m_BTHAddress = std::move(other.m_BTHAddress);
					return type;
				case Type::IMF:
					m_IMFAddress = std::move(other.m_IMFAddress);
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
		[[nodiscard]] constexpr Type Construct(T&& other) noexcept
		{
			std::construct_at(std::addressof(m_IPAddress), std::forward<T>(other));

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
		[[nodiscard]] constexpr Type Construct(T&& other) noexcept
		{
			std::construct_at(std::addressof(m_BTHAddress), std::forward<T>(other));

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

		template<typename T> requires (std::is_same_v<std::decay_t<T>, IMFAddress>)
		[[nodiscard]] constexpr Type Construct(T&& other)
		{
			std::construct_at(std::addressof(m_IMFAddress), std::forward<T>(other));

			switch (m_IMFAddress.GetFamily())
			{
				case IMFAddress::Family::IMF:
					return Type::IMF;
				case IMFAddress::Family::Unspecified:
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
			IMFAddress m_IMFAddress;
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