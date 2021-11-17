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
		static_assert(sizeof(m_BinaryAddress.Bytes) >= sizeof(SOCKADDR_BTH::btAddr), "BTH Address length mismatch");

		if (std::wcslen(addr_str) <= BTHAddress::MaxBTHAddressStringLength)
		{
			SOCKADDR_BTH saddr;
			MemInit(&saddr, sizeof(saddr));
			int saddr_len = sizeof(saddr);

			if (WSAStringToAddress(const_cast<WChar*>(addr_str), AF_BTH, nullptr,
								   reinterpret_cast<sockaddr*>(&saddr), &saddr_len) == 0)
			{
				m_BinaryAddress.UInt64s = saddr.btAddr;
				m_BinaryAddress.AddressFamily = BinaryBTHAddress::Family::BTH;
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
				static_assert(sizeof(m_BinaryAddress.Bytes) >= sizeof(SOCKADDR_BTH::btAddr), "BTH Address length mismatch");

				m_BinaryAddress.AddressFamily = BinaryBTHAddress::Family::BTH;

				auto bth = reinterpret_cast<const SOCKADDR_BTH*>(saddr);
				memcpy(&m_BinaryAddress.Bytes, &bth->btAddr, sizeof(bth->btAddr));

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

	std::ostream& operator<<(std::ostream& stream, const BTHAddress& ipaddr)
	{
		stream << Util::ToStringA(ipaddr.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const BTHAddress& ipaddr)
	{
		stream << ipaddr.GetString();
		return stream;
	}
}