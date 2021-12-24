// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BTHAddress.h"
#include "..\Common\Endian.h"

#include <regex>

namespace QuantumGate::Implementation::Network
{
	bool BTHAddress::TryParse(const WChar* addr_str, BTHAddress& addr) noexcept
	{
		try
		{
			BTHAddress temp_ip(addr_str);
			addr = std::move(temp_ip);
			return true;
		}
		catch (...) {}

		return false;
	}

	bool BTHAddress::TryParse(const String& addr_str, BTHAddress& addr) noexcept
	{
		return TryParse(addr_str.c_str(), addr);
	}

	bool BTHAddress::TryParse(const BinaryBTHAddress& bin_addr, BTHAddress& addr) noexcept
	{
		try
		{
			BTHAddress temp_ip(bin_addr);
			addr = std::move(temp_ip);
			return true;
		}
		catch (...) {}

		return false;
	}

	void BTHAddress::SetAddress(const WChar* addr_str)
	{
		static_assert(sizeof(m_BinaryAddress.UInt64s) >= sizeof(SOCKADDR_BTH::btAddr), "BTH Address length mismatch");

		const auto addr_len = std::wcslen(addr_str);
		if (addr_len > 0 && addr_len <= BTHAddress::MaxBTHAddressStringLength)
		{
			SOCKADDR_BTH saddr;
			MemInit(&saddr, sizeof(saddr));
			int saddr_len = sizeof(saddr);

			if (WSAStringToAddress(const_cast<WChar*>(addr_str), AF_BTH, nullptr,
								   reinterpret_cast<sockaddr*>(&saddr), &saddr_len) == 0)
			{
				m_BinaryAddress.AddressFamily = BinaryBTHAddress::Family::BTH;
				m_BinaryAddress.UInt64s = saddr.btAddr;
				return;
			}
		}

		throw std::invalid_argument("Invalid Bluetooth address");

		return;
	}

	void BTHAddress::SetAddress(const sockaddr_storage* saddr)
	{
		assert(saddr != nullptr);

		switch (saddr->ss_family)
		{
			case AF_BTH:
			{
				static_assert(sizeof(m_BinaryAddress.UInt64s) >= sizeof(SOCKADDR_BTH::btAddr), "BTH Address length mismatch");

				auto bth = reinterpret_cast<const SOCKADDR_BTH*>(saddr);
				m_BinaryAddress.AddressFamily = BinaryBTHAddress::Family::BTH;
				m_BinaryAddress.UInt64s = bth->btAddr;

				break;
			}
			default:
			{
				throw std::invalid_argument("Unsupported Bluetooth address family");
			}
		}

		return;
	}

	String BTHAddress::GetString() const noexcept
	{
		try
		{
			// WSAAddressToString() requires at least 40 characters even though
			// the maximum Bluetooth address length is 20 characters; see docs:
			// https://docs.microsoft.com/en-us/windows/win32/bluetooth/bluetooth-and-wsaaddresstostring
			std::array<WChar, 40> addr_str{ 0 };

			SOCKADDR_BTH saddr{
				.addressFamily = static_cast<USHORT>(BTH::AddressFamilyToNetwork(m_BinaryAddress.AddressFamily)),
				.btAddr = m_BinaryAddress.UInt64s
			};

			auto addr_str_len = static_cast<DWORD>(addr_str.size());

			if (WSAAddressToString(reinterpret_cast<sockaddr*>(&saddr), sizeof(saddr), nullptr,
								   addr_str.data(), &addr_str_len) == 0)
			{
				return addr_str.data();
			}
		}
		catch (...) {}

		return {};
	}

	std::ostream& operator<<(std::ostream& stream, const BTHAddress& addr)
	{
		stream << Util::ToStringA(addr.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const BTHAddress& addr)
	{
		stream << addr.GetString();
		return stream;
	}
}