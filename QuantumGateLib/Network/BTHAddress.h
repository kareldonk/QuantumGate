// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BinaryBTHAddress.h"

namespace QuantumGate::Implementation::Network
{
	class Export BTHAddress
	{
	public:
		using Family = BinaryBTHAddress::Family;

		constexpr BTHAddress() noexcept :
			m_BinaryAddress(BinaryBTHAddress{ BinaryBTHAddress::Family::BTH }) // Defaults to BTH any address
		{}

		constexpr BTHAddress(const BTHAddress& other) noexcept : m_BinaryAddress(other.m_BinaryAddress) {}
		constexpr BTHAddress(BTHAddress&& other) noexcept : m_BinaryAddress(std::move(other.m_BinaryAddress)) {}

		explicit BTHAddress(const WChar* addr_str) { SetAddress(addr_str); }
		explicit BTHAddress(const String& addr_str) { SetAddress(addr_str.c_str()); }
		explicit BTHAddress(const sockaddr_storage* saddr) { SetAddress(saddr); }
		explicit BTHAddress(const sockaddr* saddr) { SetAddress(reinterpret_cast<const sockaddr_storage*>(saddr)); }

		constexpr BTHAddress(const BinaryBTHAddress& bin_addr) { SetAddress(bin_addr); }

		~BTHAddress() = default;

		constexpr BTHAddress& operator=(const BTHAddress& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_BinaryAddress = other.m_BinaryAddress;

			return *this;
		}

		constexpr BTHAddress& operator=(BTHAddress&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_BinaryAddress = other.m_BinaryAddress;

			return *this;
		}

		constexpr bool operator==(const BTHAddress& other) const noexcept
		{
			return (m_BinaryAddress == other.m_BinaryAddress);
		}

		constexpr bool operator!=(const BTHAddress& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr bool operator==(const BinaryBTHAddress& other) const noexcept
		{
			return (m_BinaryAddress == other);
		}

		constexpr bool operator!=(const BinaryBTHAddress& other) const noexcept
		{
			return !(*this == other);
		}

		[[nodiscard]] constexpr Family GetFamily() const noexcept { return m_BinaryAddress.AddressFamily; }
		[[nodiscard]] constexpr const BinaryBTHAddress& GetBinary() const noexcept { return m_BinaryAddress; }
		[[nodiscard]] String GetString() const noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const BTHAddress& addr);
		friend Export std::wostream& operator<<(std::wostream& stream, const BTHAddress& addr);

		[[nodiscard]] static constexpr BTHAddress AnyBTH() noexcept { return { BinaryBTHAddress(BinaryBTHAddress::Family::BTH) }; }

		[[nodiscard]] static bool TryParse(const WChar* addr_str, BTHAddress& addr) noexcept;
		[[nodiscard]] static bool TryParse(const String& addr_str, BTHAddress& addr) noexcept;
		[[nodiscard]] static bool TryParse(const BinaryBTHAddress& bin_addr, BTHAddress& addr) noexcept;

		[[nodiscard]] std::size_t GetHash() const noexcept { return m_BinaryAddress.GetHash(); }

	private:
		void SetAddress(const WChar* addr_str);
		void SetAddress(const sockaddr_storage* saddr);

		constexpr void SetAddress(const BinaryBTHAddress& bin_addr)
		{
			switch (bin_addr.AddressFamily)
			{
				case BinaryBTHAddress::Family::BTH:
					m_BinaryAddress = bin_addr;
					break;
				default:
					throw std::invalid_argument("Unsupported Bluetooth address family");
			}

			return;
		}

		constexpr void Clear() noexcept { m_BinaryAddress.Clear(); }

	private:
		static constexpr UInt8 MaxBTHAddressStringLength{ 20 }; // Maximum length of BTH address

		BinaryBTHAddress m_BinaryAddress;  // In host byte order (little endian)
	};
}

namespace std
{
	// Specialization for standard hash function for BTHAddress
	template<> struct hash<QuantumGate::Implementation::Network::BTHAddress>
	{
		std::size_t operator()(const QuantumGate::Implementation::Network::BTHAddress& addr) const noexcept
		{
			return addr.GetHash();
		}
	};
}