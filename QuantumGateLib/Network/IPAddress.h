// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BinaryIPAddress.h"

#include <array>

namespace QuantumGate::Implementation::Network
{
	class Export IPAddress
	{
	public:
		IPAddress() noexcept;
		IPAddress(const IPAddress& other) noexcept;
		IPAddress(IPAddress&& other) noexcept;
		IPAddress(const String& ipaddr_str);
		IPAddress(const sockaddr_storage* saddr);
		IPAddress(const sockaddr* saddr);
		IPAddress(const BinaryIPAddress& bin_ipaddr);
		virtual ~IPAddress() = default;

		IPAddress& operator=(const IPAddress& other) noexcept;
		IPAddress& operator=(IPAddress&& other) noexcept;

		constexpr const bool operator==(const IPAddress& other) const noexcept
		{
			return (m_AddressBinary == other.m_AddressBinary);
		}

		constexpr const bool operator!=(const IPAddress& other) const noexcept
		{
			return !(*this == other);
		}

		inline String GetString() const noexcept { return m_AddressString.data(); }
		inline const WChar* GetCString() const noexcept { return m_AddressString.data(); }
		constexpr const BinaryIPAddress& GetBinary() const noexcept { return m_AddressBinary; }
		constexpr const IPAddressFamily GetFamily() const noexcept { return m_AddressBinary.AddressFamily; }

		[[nodiscard]] static const bool TryParse(const String& ipaddr_str, IPAddress& ipaddr) noexcept;
		[[nodiscard]] static const bool TryParseMask(const IPAddressFamily af,
													 const String& mask, IPAddress& ipaddr) noexcept;
		[[nodiscard]] static const bool TryParseMask(const IPAddressFamily af,
													 UInt8 cidr_lbits, IPAddress& ipaddr) noexcept;
		[[nodiscard]] static const bool TryParseMask(const IPAddressFamily af,
													 UInt8 cidr_lbits, BinaryIPAddress& bin_ipaddr) noexcept;

		static const IPAddress AnyIPv4() noexcept;
		static const IPAddress AnyIPv6() noexcept;

		static const IPAddress LoopbackIPv4() noexcept;
		static const IPAddress LoopbackIPv6() noexcept;

		static const IPAddress Broadcast() noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const IPAddress& ipaddr);
		friend Export std::wostream& operator<<(std::wostream& stream, const IPAddress& ipaddr);

	private:
		void SetAddress(const String& ipaddr_str);
		void SetAddress(const BinaryIPAddress& bin_ipaddr);
		void SetAddress(const sockaddr_storage* saddr);

		void Clear() noexcept;

	private:
		static constexpr UInt8 MaxIPAddressStringLength{ 46 }; // Maximum length of IPv6 address

		std::array<WChar, MaxIPAddressStringLength> m_AddressString{ 0 };
		BinaryIPAddress m_AddressBinary; // In network byte order (big endian)
	};
}
