// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Util.h"
#include "Random.h"
#include "..\Common\Hash.h"
#include "..\Common\ScopeGuard.h"

#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace QuantumGate::Implementation::Util
{
	Export bool GetCurrentLocalTime(const WChar* format, std::array<WChar, 128>& timestr) noexcept
	{
		const Time time = std::time(nullptr);
		tm time_tm{ 0 };

		if (localtime_s(&time_tm, &time) == 0)
		{
			if (std::wcsftime(timestr.data(), timestr.size(), format, &time_tm) != 0)
			{
				return true;
			}
		}

		return false;
	}

	Export String GetCurrentLocalTime(const WChar* format) noexcept
	{
		try
		{
			std::array<WChar, 128> timestr{ 0 };

			if (GetCurrentLocalTime(format, timestr))
			{
				return timestr.data();
			}
		}
		catch (...) {}

		return {};
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

	Export String FormatString(const WChar* format, va_list arglist) noexcept
	{
		try
		{
			const std::size_t size = static_cast<std::size_t>(_vscwprintf(format, arglist)) + 1; // include space for '\0'

			String txt;
			txt.resize(size - 1); // exclude space for '\0'
			std::vswprintf(txt.data(), size, format, arglist);

			return txt;
		}
		catch (...) {}

		return {};
	}

	Export String FormatString(const WChar* format, ...) noexcept
	{
		va_list argptr = nullptr;
		va_start(argptr, format);

		auto fstr = FormatString(format, argptr);

		va_end(argptr);

		return fstr;
	}

	Export String ToBinaryString(const gsl::span<Byte> bytes) noexcept
	{
		try
		{
			const auto numbits = CHAR_BIT * bytes.size();
			const auto numsep = bytes.size() - 1;
			const auto len = numbits + numsep;

			String txt;
			txt.reserve(len);

			for (const auto byte : bytes)
			{
				if (!txt.empty()) txt += L"'";
				txt += ToBinaryString(static_cast<UChar>(byte)).data();
			}

			return txt;
		}
		catch (...) {}

		return {};
	}

	template<typename T, typename U, bool clear>
	T ToStringWImpl(const U& txt) noexcept
	{
		T str;

		try
		{
			std::array<typename T::value_type, 1024> tmp{ 0 };

			const auto ret = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, txt.data(), -1, tmp.data(), static_cast<int>(tmp.size()));
			if (ret != 0)
			{
				str = tmp.data();
			}
			else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				const auto len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, txt.data(), -1, nullptr, 0);
				if (len > 0)
				{
					str.resize(len - 1); // exclude space for '\0'

					MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, txt.data(), -1, str.data(), len);
				}
			}

			if constexpr (clear)
			{
				// Wipe all temp data
				MemClear(tmp.data(), sizeof(typename T::value_type) * tmp.size());
			}
		}
		catch (...) {}

		return str;
	}

	Export String ToStringW(const std::string& txt) noexcept
	{
		return ToStringWImpl<String, std::string, false>(txt);
	}

	Export ProtectedString ToProtectedStringW(const ProtectedStringA& txt) noexcept
	{
		return ToStringWImpl<ProtectedString, ProtectedStringA, true>(txt);
	}

	template<typename T, typename U, bool clear>
	T ToStringAImpl(const U& txt) noexcept
	{
		T str;

		try
		{
			std::array<typename T::value_type, 1024> tmp{ 0 };

			const auto ret = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, txt.data(), -1,
												 tmp.data(), static_cast<int>(tmp.size()), nullptr, nullptr);
			if (ret != 0)
			{
				str = tmp.data();
			}
			else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
			{
				const auto len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, txt.data(), -1, nullptr, 0, nullptr, nullptr);
				if (len > 0)
				{
					str.resize(len - 1); // exclude space for '\0'

					WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, txt.data(), -1, str.data(), len, nullptr, nullptr);
				}
			}

			if constexpr (clear)
			{
				// Wipe all temp data
				MemClear(tmp.data(), sizeof(typename T::value_type) * tmp.size());
			}
		}
		catch (...) {}

		return str;
	}

	Export std::string ToStringA(const String& txt) noexcept
	{
		return ToStringAImpl<std::string, String, false>(txt);
	}

	Export ProtectedStringA ToProtectedStringA(const ProtectedString& txt) noexcept
	{
		return ToStringAImpl<ProtectedStringA, ProtectedString, true>(txt);
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
				auto sg = MakeScopeGuard([&]() noexcept { BIO_free_all(buff); });

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
						Vector<Char> out(ptr->length + 1);
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

	template<typename S, typename B>
	bool FromBase64(const S& b64, B& buffer) noexcept
	{
		try
		{
			buffer.Allocate(b64.size());

			// Docs: https://www.openssl.org/docs/manmaster/man3/BIO_f_base64.html
			auto b64f = BIO_new(BIO_f_base64());
			auto buff = BIO_new_mem_buf(b64.c_str(), static_cast<int>(b64.size()));

			// When we leave release bio
			auto sg = MakeScopeGuard([&]() noexcept { BIO_free_all(buff); });

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
	template bool FromBase64<std::string, Buffer>(const std::string& b64, Buffer& buffer) noexcept;
	template bool FromBase64<ProtectedStringA, ProtectedBuffer>(const ProtectedStringA& b64, ProtectedBuffer& buffer) noexcept;

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


	Export UInt64 NonPersistentHash(const String& txt) noexcept
	{
		return Hash::GetNonPersistentHash(txt);
	}

	Export UInt64 PersistentHash(const String& txt) noexcept
	{
		return Hash::GetPersistentHash(txt);
	}

	Export bool SetThreadName(HANDLE thread, const String& name) noexcept
	{
		const HRESULT hr = SetThreadDescription(thread, name.c_str());
		if (FAILED(hr)) return false;

		return true;
	}

	Export bool SetCurrentThreadName(const String& name) noexcept
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
		auto errorstr = FormatString(L"%s : %d : %s",
									 ToStringW(error.category().name()).c_str(),
									 error.value(),
									 ToStringW(error.message()).c_str());

		// Remove new line characters at the end
		while (errorstr[errorstr.size() - 1] == L'\r' ||
			   errorstr[errorstr.size() - 1] == L'\n')
		{
			errorstr.resize(errorstr.size() - 1);
		}

		return errorstr;
	}

	Export void DisplayDebugMessage(const WChar* message, ...) noexcept
	{
		va_list argptr = nullptr;
		va_start(argptr, message);

		OutputDebugString(FormatString(message, argptr).c_str());
		OutputDebugString(L"\r\n");

		va_end(argptr);
	}
}