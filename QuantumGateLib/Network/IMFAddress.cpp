// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "IMFAddress.h"
#include "IPAddress.h"

#include <regex>

namespace QuantumGate::Implementation::Network
{
	// Email regex based on https://github.com/Microsoft/referencesource/blob/master/System.ComponentModel.DataAnnotations/DataAnnotations/EmailAddressAttribute.cs

	ForceInline static const std::wregex& GetIMFAddrSpecLocalPartRegEx()
	{
		static std::wregex r(LR"imf(^((([a-z]|\d|[!#\$%&'\*\+\-\/=\?\^_`{\|}~]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])+(\.([a-z]|\d|[!#\$%&'\*\+\-\/=\?\^_`{\|}~]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])+)*)|((\x22)((((\x20|\x09)*(\x0d\x0a))?(\x20|\x09)+)?(([\x01-\x08\x0b\x0c\x0e-\x1f\x7f]|\x21|[\x23-\x5b]|[\x5d-\x7e]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(\\([\x01-\x09\x0b\x0c\x0d-\x7f]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]))))*(((\x20|\x09)*(\x0d\x0a))?(\x20|\x09)+)?(\x22)))$)imf",
							 std::regex_constants::ECMAScript | std::regex_constants::icase | std::regex_constants::optimize);
		return r;
	}

	ForceInline static const std::wregex& GetIMFAddrSpecDomainPartDotAtomRegEx()
	{
		static std::wregex r(LR"imf(^(([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.?$|^(((([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.)+(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]))))\.?$)imf",
							 std::regex_constants::ECMAScript | std::regex_constants::icase | std::regex_constants::optimize);
		return r;
	}

	ForceInline static const std::wregex& GetIMFAddrSpecDomainPartLiteralRegEx()
	{
		static std::wregex r(LR"imf(^\[([0-9.]+)\]$|^\[(?:IPv6:)?([0-9a-f:.]+)\]$)imf",
							 std::regex_constants::ECMAScript | std::regex_constants::icase | std::regex_constants::optimize);
		return r;
	}

	bool IMFAddress::TryParse(const WChar* addr_str, IMFAddress& addr) noexcept
	{
		try
		{
			IMFAddress temp_ip(addr_str);
			addr = std::move(temp_ip);
			return true;
		}
		catch (...) {}

		return false;
	}

	bool IMFAddress::TryParse(const String& addr_str, IMFAddress& addr) noexcept
	{
		return TryParse(addr_str.c_str(), addr);
	}

	bool IMFAddress::TryParse(const BinaryIMFAddress& bin_addr, IMFAddress& addr) noexcept
	{
		try
		{
			IMFAddress temp_ip(bin_addr);
			addr = std::move(temp_ip);
			return true;
		}
		catch (...) {}

		return false;
	}

	void IMFAddress::SetAddress(const WChar* addr_str)
	{
		const StringView addr_strv(addr_str);

		if (addr_strv.size() <= BinaryIMFAddress::MaxAddressStringLength)
		{
			const auto pos = addr_strv.rfind(L"@");
			if (pos != addr_strv.npos)
			{
				const auto local_part = addr_strv.substr(0, pos);
				const auto domain_part = addr_strv.substr(pos+1, addr_strv.size() - (pos+1));

				const auto check_local_part = [](const StringView str) -> bool
				{
					if (str.size() <= BinaryIMFAddress::MaxAddressLocalPartStringLength)
					{
						std::wcmatch ml;
						if (std::regex_match(str.data(), str.data() + str.size(), ml, GetIMFAddrSpecLocalPartRegEx()))
						{
							return true;
						}
					}

					return false;
				};

				const auto check_domain_part = [](const StringView str) -> bool
				{
					std::wcmatch mdda, mdl;
					if (std::regex_match(str.data(), str.data() + str.size(), mdda, GetIMFAddrSpecDomainPartDotAtomRegEx()))
					{
						return true;
					}
					else if (std::regex_match(str.data(), str.data() + str.size(), mdl, GetIMFAddrSpecDomainPartLiteralRegEx()))
					{
						assert(mdl.size() == 3);

						constexpr auto max_ipstring_length{ 46u };  // Maximum length of IPv6 address plus null terminator
						std::array<WChar, max_ipstring_length> ipaddr_str{ 0 };

						if (mdl[1].matched && static_cast<size_t>(mdl[1].length()) < ipaddr_str.size())
						{
							std::copy(mdl[1].first, mdl[1].second, ipaddr_str.begin());
						}
						else if (mdl[2].matched && static_cast<size_t>(mdl[2].length()) < ipaddr_str.size())
						{
							std::copy(mdl[2].first, mdl[2].second, ipaddr_str.begin());
						}
						else return false;

						IPAddress ipaddr;
						if (IPAddress::TryParse(ipaddr_str.data(), ipaddr))
						{
							return true;
						}
					}

					return false;
				};

				if (check_local_part(local_part) && check_domain_part(domain_part))
				{
					m_BinaryAddress = BinaryIMFAddress(BinaryIMFAddress::Family::IMF, addr_str);
					return;
				}
			}
		}

		throw std::invalid_argument("Invalid Internet Message Format address");

		return;
	}

	String IMFAddress::GetString() const noexcept
	{
		if (m_BinaryAddress.GetChars() != nullptr)
		{
			try
			{
				return m_BinaryAddress.GetChars();
			}
			catch (...) {}
		}

		return {};
	}

	std::ostream& operator<<(std::ostream& stream, const IMFAddress& addr)
	{
		stream << Util::ToStringA(addr.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const IMFAddress& addr)
	{
		stream << addr.GetString();
		return stream;
	}
}