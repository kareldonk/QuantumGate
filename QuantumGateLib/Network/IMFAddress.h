// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BinaryIMFAddress.h"

namespace QuantumGate::Implementation::Network
{
	class Export IMFAddress
	{
	public:
		using Family = BinaryIMFAddress::Family;

		constexpr IMFAddress() noexcept {}

		constexpr IMFAddress(const IMFAddress& other) : m_BinaryAddress(other.m_BinaryAddress) {}
		constexpr IMFAddress(IMFAddress&& other) noexcept : m_BinaryAddress(std::move(other.m_BinaryAddress)) {}

		explicit IMFAddress(const WChar* addr_str) { SetAddress(addr_str); }
		explicit IMFAddress(const String& addr_str) { SetAddress(addr_str.c_str()); }

		constexpr IMFAddress(const BinaryIMFAddress& bin_addr) { SetAddress(bin_addr); }

		~IMFAddress() = default;

		constexpr IMFAddress& operator=(const IMFAddress& other)
		{
			// Check for same object
			if (this == &other) return *this;

			m_BinaryAddress = other.m_BinaryAddress;

			return *this;
		}

		constexpr IMFAddress& operator=(IMFAddress&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_BinaryAddress = std::move(other.m_BinaryAddress);

			return *this;
		}

		constexpr bool operator==(const IMFAddress& other) const noexcept
		{
			return (m_BinaryAddress == other.m_BinaryAddress);
		}

		constexpr bool operator!=(const IMFAddress& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr bool operator==(const BinaryIMFAddress& other) const noexcept
		{
			return (m_BinaryAddress == other);
		}

		constexpr bool operator!=(const BinaryIMFAddress& other) const noexcept
		{
			return !(*this == other);
		}

		[[nodiscard]] constexpr Family GetFamily() const noexcept { return m_BinaryAddress.AddressFamily; }
		[[nodiscard]] constexpr const BinaryIMFAddress& GetBinary() const noexcept { return m_BinaryAddress; }
		[[nodiscard]] String GetString() const noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const IMFAddress& addr);
		friend Export std::wostream& operator<<(std::wostream& stream, const IMFAddress& addr);

		[[nodiscard]] static bool TryParse(const WChar* addr_str, IMFAddress& addr) noexcept;
		[[nodiscard]] static bool TryParse(const String& addr_str, IMFAddress& addr) noexcept;
		[[nodiscard]] static bool TryParse(const BinaryIMFAddress& bin_addr, IMFAddress& addr) noexcept;

		[[nodiscard]] std::size_t GetHash() const noexcept { return m_BinaryAddress.GetHash(); }

	private:
		void SetAddress(const WChar* addr_str);

		constexpr void SetAddress(const BinaryIMFAddress& bin_addr)
		{
			switch (bin_addr.AddressFamily)
			{
				case BinaryIMFAddress::Family::IMF:
					m_BinaryAddress = bin_addr;
					break;
				default:
					throw std::invalid_argument("Unsupported Internet Message Format address family");
			}

			return;
		}

		constexpr void Clear() noexcept { m_BinaryAddress.Clear(); }

	private:
		BinaryIMFAddress m_BinaryAddress;
	};
}

namespace std
{
	// Specialization for standard hash function for IMFAddress
	template<> struct hash<QuantumGate::Implementation::Network::IMFAddress>
	{
		std::size_t operator()(const QuantumGate::Implementation::Network::IMFAddress& addr) const noexcept
		{
			return addr.GetHash();
		}
	};
}