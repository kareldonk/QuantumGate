// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BTHAddress.h"
#include "..\Common\Endian.h"

#include <cwctype>

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
		// Look for address in the format (XX:XX:XX:XX:XX:XX) 
		// for example "(92:5F:D3:5B:93:B2)" which is the
		// same as the WSAStringToAddress() function
		const auto parse_address = [](const WChar* addr_str, UInt64& bth_addr) noexcept -> bool
		{
			if (std::wcslen(addr_str) != BTHAddress::MaxBTHAddressStringLength ||
				addr_str[0] != L'(' || addr_str[18] != L')')
			{
				return false;
			}

			std::array<WChar, 4> addr{ 0 };

			for (auto i = 1u; i <= 16u; i += 3)
			{
				addr[0] = addr_str[i];
				addr[1] = addr_str[i+1];
				addr[2] = addr_str[i+2];

				if (std::iswspace(addr[0]) == 0)
				{
					WChar* end{ nullptr };
					errno = 0;

					const auto addri = std::wcstoull(addr.data(), &end, 16);
					if (errno == 0 && end == (addr.data() + 2))
					{
						if (i < 16u && *end != L':')
						{
							return false;
						}

						bth_addr = ((bth_addr) << 8) | (addri & 0xff);
					}
					else return false;
				}
				else return false;
			}

			return true;
		};

		UInt64 bth_addr{ 0 };

		if (parse_address(addr_str, bth_addr))
		{
			m_BinaryAddress.AddressFamily = BinaryBTHAddress::Family::BTH;
			m_BinaryAddress.UInt64s = bth_addr;
			return;
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
		// Same format as the WSAAddressToString() function
		return Util::FormatString(L"(%02X:%02X:%02X:%02X:%02X:%02X)",
								  m_BinaryAddress.Bytes[5], m_BinaryAddress.Bytes[4], m_BinaryAddress.Bytes[3],
								  m_BinaryAddress.Bytes[2], m_BinaryAddress.Bytes[1], m_BinaryAddress.Bytes[0]);
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