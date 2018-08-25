// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Util.h"
#include "Endian.h"
#include "Random.h"
#include "..\Algorithms.h"
#include "..\Common\Hash.h"
#include "..\Common\ScopeGuard.h"

#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace QuantumGate::Implementation::Util
{
	Export String GetCurrentLocalTime(const String& format) noexcept
	{
		const Time time = std::time(nullptr);
		tm time_tm{ 0 };
		WChar timestr[100]{ 0 };

		if (localtime_s(&time_tm, &time) == 0)
		{
			if (wcsftime(timestr, 100, format.c_str(), &time_tm) != 0) return timestr;
		}

		return L"";
	}

	Export SystemTime GetCurrentSystemTime() noexcept
	{
		return std::chrono::system_clock::now();
	}

	Export SteadyTime GetCurrentSteadyTime() noexcept
	{
		return std::chrono::steady_clock::now();
	}

	SystemTime ToTime(const Time& time) noexcept
	{
		return std::chrono::system_clock::from_time_t(time);
	}

	Time ToTimeT(const SystemTime& time) noexcept
	{
		return std::chrono::system_clock::to_time_t(time);
	}

	Export String FormatString(const StringView format, ...) noexcept
	{
		va_list argptr = nullptr;
		va_start(argptr, format);

		auto fstr = FormatString(format, argptr);

		va_end(argptr);

		return fstr;
	}

	Export String FormatString(const StringView format, va_list arglist) noexcept
	{
		String tmpstr;
		WChar tmp[1024]{ 0 };
		WChar* fmtptr{ nullptr };

		// Need to add '\0' to format
		if (format.size() < sizeof(tmp))
		{
			memcpy(&tmp, format.data(), format.size() * sizeof(WChar));
			fmtptr = tmp;
		}
		else
		{
			tmpstr = format;
			fmtptr = tmpstr.data();
		}

		const Size size = _vscwprintf(fmtptr, arglist) + 1; // include space for '\0'

		String txt;
		txt.resize(size - 1u); // exclude space for '\0'
		std::vswprintf(txt.data(), size, fmtptr, arglist);

		return txt;
	}

	template<typename T>
	Export String ToBinaryString(const T bytes) noexcept
	{
		static_assert(std::is_integral_v<T>, "Unsupported type.");

		const auto numbits = CHAR_BIT * sizeof(T);
		const auto numsep = sizeof(T) - 1u;
		Char txt[numbits + numsep + 1u]{ 0 };

		auto pos = 0u;
		auto bitcount = 0u;
		for (auto x = 0u; x < numbits; ++x)
		{
			if (bitcount == CHAR_BIT)
			{
				txt[sizeof(txt) - (pos + 2)] = '\'';
				bitcount = 0u;
				++pos;
			}

			txt[sizeof(txt) - (pos + 2)] = static_cast<UChar>((bytes >> x) & 0x1) ? '1' : '0';

			++pos;
			++bitcount;
		}

		return ToStringW(txt);
	}

	// Specific instantiations
	template Export String ToBinaryString<Int8>(const Int8 bytes) noexcept;
	template Export String ToBinaryString<UInt8>(const UInt8 bytes) noexcept;
	template Export String ToBinaryString<Int16>(const Int16 bytes) noexcept;
	template Export String ToBinaryString<UInt16>(const UInt16 bytes) noexcept;
	template Export String ToBinaryString<Int32>(const Int32 bytes) noexcept;
	template Export String ToBinaryString<UInt32>(const UInt32 bytes) noexcept;
	template Export String ToBinaryString<Int64>(const Int64 bytes) noexcept;
	template Export String ToBinaryString<UInt64>(const UInt64 bytes) noexcept;

	String ToBinaryString(const gsl::span<Byte> bytes) noexcept
	{
		String txt;
		for (const auto byte : bytes)
		{
			if (!txt.empty()) txt += L"'";
			txt += ToBinaryString(static_cast<UChar>(byte));
		}

		return txt;
	}

	Export String ToStringW(const std::string& txt) noexcept
	{
		return String(txt.begin(), txt.end());
	}

	Export std::string ToStringA(const String& txt) noexcept
	{
		return std::string(txt.begin(), txt.end());
	}

	Export std::optional<String> GetBase64(const BufferView& buffer) noexcept
	{
		return GetBase64(buffer.GetBytes(), buffer.GetSize());
	}

	Export std::optional<String> GetBase64(const Buffer& buffer) noexcept
	{
		return GetBase64(buffer.GetBytes(), buffer.GetSize());
	}

	std::optional<String> GetBase64(const Byte* buffer, const Size len) noexcept
	{
		try
		{
			if (buffer != nullptr && len > 0)
			{
				// Docs: https://www.openssl.org/docs/manmaster/man3/BIO_f_base64.html
				auto b64f = BIO_new(BIO_f_base64());
				auto buff = BIO_new(BIO_s_mem());

				// When we leave release bio
				auto sg = MakeScopeGuard([&] { BIO_free_all(buff); });

				buff = BIO_push(b64f, buff);

				BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
				BIO_set_close(buff, BIO_CLOSE);

				// Return value greater than zero means successful (https://www.openssl.org/docs/manmaster/man3/BIO_write.html)
				if (BIO_write(buff, buffer, static_cast<int>(len)) > 0)
				{
					BIO_flush(buff);

					BUF_MEM* ptr = nullptr;
					BIO_get_mem_ptr(buff, &ptr);
					if (ptr != nullptr)
					{
						// Copy base64 data to new buffer, extra byte for null terminator
						std::vector<Char> out(ptr->length + 1);
						memcpy(out.data(), ptr->data, ptr->length);

						// Make sure string is null terminated
						out[ptr->length] = '\0';

						return { ToStringW(out.data()) };
					}
				}
			}
		}
		catch (...) {}

		return std::nullopt;
	}

	std::optional<Buffer> FromBase64(const String& b64) noexcept
	{
		// Convert to string (char*) first
		const auto b64str = ToStringA(b64);

		return FromBase64(b64str);
	}

	Export std::optional<Buffer> FromBase64(const std::string& b64) noexcept
	{
		try
		{
			Buffer buffer;
			if (FromBase64(b64, buffer)) return { std::move(buffer) };
		}
		catch (...) {}

		return std::nullopt;
	}

	std::optional<ProtectedBuffer> FromBase64(const ProtectedStringA& b64) noexcept
	{
		try
		{
			ProtectedBuffer buffer;
			if (FromBase64(b64, buffer)) return { std::move(buffer) };
		}
		catch (...) {}

		return std::nullopt;
	}

	template<typename S, typename B>
	const bool FromBase64(const S& b64, B& buffer) noexcept
	{
		try
		{
			buffer.Allocate(b64.size());

			// Docs: https://www.openssl.org/docs/manmaster/man3/BIO_f_base64.html
			auto b64f = BIO_new(BIO_f_base64());
			auto buff = BIO_new_mem_buf(b64.c_str(), static_cast<int>(b64.size()));

			// When we leave release bio
			auto sg = MakeScopeGuard([&] { BIO_free_all(buff); });

			buff = BIO_push(b64f, buff);

			BIO_set_flags(buff, BIO_FLAGS_BASE64_NO_NL);
			BIO_set_close(buff, BIO_CLOSE);

			// Return value greater than zero means successful (https://www.openssl.org/docs/manmaster/man3/BIO_write.html)
			const auto bytesread = BIO_read(b64f, buffer.GetBytes(), static_cast<int>(buffer.GetSize()));
			if (bytesread > 0)
			{
				buffer.Resize(bytesread);
				return true;
			}
		}
		catch (...) {}

		return false;
	}

	// Specific instantiations
	template const bool FromBase64<std::string, Buffer>(const std::string& b64, Buffer& buffer) noexcept;
	template const bool FromBase64<ProtectedStringA, ProtectedBuffer>(const ProtectedStringA& b64,
																	  ProtectedBuffer& buffer) noexcept;

	Export UInt64 NonPersistentHash(const String& txt) noexcept
	{
		return Hash::GetNonPersistentHash(txt);
	}

	Export UInt64 PersistentHash(const String& txt) noexcept
	{
		return Hash::GetPersistentHash(txt);
	}

	Export const bool SetThreadName(HANDLE thread, const String& name) noexcept
	{
		const HRESULT hr = SetThreadDescription(thread, name.c_str());
		if (FAILED(hr)) return false;

		return true;
	}

	Export const bool SetCurrentThreadName(const String& name) noexcept
	{
		return SetThreadName(GetCurrentThread(), name);
	}

	Export Int64 GetPseudoRandomNumber() noexcept
	{
		return Random::GetPseudoRandomNumber();
	}

	Export Int64 GetPseudoRandomNumber(const Int64 min, const Int64 max) noexcept
	{
		return Random::GetPseudoRandomNumber(min, max);
	}

	Export Buffer GetPseudoRandomBytes(const Size count)
	{
		return Random::GetPseudoRandomBytes(count);
	}

	Export String GetSystemErrorString(const int code) noexcept
	{
		const auto error = std::error_code(code, std::system_category());

		return FormatString(L"%s : %d : %s",
							ToStringW(error.category().name()).c_str(),
							error.value(),
							ToStringW(error.message()).c_str());
	}

	Export void DisplayDebugMessage(const StringView message, ...) noexcept
	{
		va_list argptr = nullptr;
		va_start(argptr, message);

		OutputDebugString(FormatString(message, argptr).c_str());
		OutputDebugString(L"\r\n");

		va_end(argptr);
	}
}