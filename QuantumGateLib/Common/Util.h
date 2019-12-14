// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <chrono>
#include <vector>

namespace QuantumGate::Implementation::Util
{
	Export bool GetCurrentLocalTime(const WChar* format, std::array<WChar, 128>& timestr) noexcept;
	Export String GetCurrentLocalTime(const WChar* format) noexcept;
	Export SystemTime GetCurrentSystemTime() noexcept;
	Export SteadyTime GetCurrentSteadyTime() noexcept;
	SystemTime ToTime(const Time& time) noexcept;
	Time ToTimeT(const SystemTime& time) noexcept;

	Export String FormatString(const WChar* format, va_list arglist) noexcept;
	Export String FormatString(const WChar* format, ...) noexcept;

	template<typename T>
	constexpr int GetBinaryStringLength()
	{
		const auto numbits = CHAR_BIT * sizeof(T);
		const auto numsep = sizeof(T) - 1;
		return numbits + numsep + 1; // exclude space for '\0'
	}

	template<typename T>
	constexpr std::array<WChar, GetBinaryStringLength<T>()> ToBinaryString(const T bytes) noexcept
	{
		static_assert(std::is_integral_v<T>, "Unsupported type.");

		const auto numbits = CHAR_BIT * sizeof(T);
		std::array<WChar, GetBinaryStringLength<T>()> txt{ 0 };

		auto pos = 0u;
		auto bitcount = 0u;
		for (auto x = 0u; x < numbits; ++x)
		{
			if (bitcount == CHAR_BIT)
			{
				txt[txt.size() - (pos + 2)] = '\'';
				bitcount = 0;
				++pos;
			}

			txt[txt.size() - (pos + 2)] = static_cast<UChar>((bytes >> x) & 0x1) ? '1' : '0';

			++pos;
			++bitcount;
		}

		return txt;
	}

	Export String ToBinaryString(const gsl::span<Byte> bytes) noexcept;

	Export String ToStringW(const Char* txt) noexcept;		
	Export String ToStringW(const std::string& txt) noexcept;
	Export ProtectedString ToProtectedStringW(const ProtectedStringA& txt) noexcept;
	Export std::string ToStringA(const WChar* txt) noexcept;
	Export std::string ToStringA(const String& txt) noexcept;
	Export ProtectedStringA ToProtectedStringA(const ProtectedString& txt) noexcept;

	Export std::optional<String> ToBase64(const BufferView& buffer) noexcept;
	Export std::optional<String> ToBase64(const Buffer& buffer) noexcept;
	Export std::optional<ProtectedString> ToBase64(const ProtectedBuffer& buffer) noexcept;
	Export std::optional<Buffer> FromBase64(const String& b64) noexcept;
	Export std::optional<Buffer> FromBase64(const std::string& b64) noexcept;
	Export std::optional<ProtectedBuffer> FromBase64(const ProtectedString& b64) noexcept;
	Export std::optional<ProtectedBuffer> FromBase64(const ProtectedStringA& b64) noexcept;

	template<typename T>
	Vector<T> SetToVector(const Set<T>& set)
	{
		Vector<T> vec;
		vec.reserve(set.size());

		for (const auto& itm : set)
		{
			vec.emplace_back(itm);
		}

		return vec;
	}

	Export UInt64 GetNonPersistentHash(const String& txt) noexcept;
	Export UInt64 GetNonPersistentHash(const BufferView& buffer) noexcept;
	Export UInt64 GetPersistentHash(const String& txt) noexcept;

	Export bool SetThreadName(HANDLE thread, const String& name) noexcept;
	Export bool SetCurrentThreadName(const String& name) noexcept;

	Export Int64 GetPseudoRandomNumber() noexcept;
	Export Int64 GetPseudoRandomNumber(const Int64 min, const Int64 max) noexcept;
	Export Buffer GetPseudoRandomBytes(const Size count);

	Export String GetSystemErrorString(const int code) noexcept;

	Export void DisplayDebugMessage(const WChar* format, ...) noexcept;
}

#define GetLastSysErrorString() QuantumGate::Implementation::Util::GetSystemErrorString(WSAGetLastError())
#define GetSysErrorString(x) QuantumGate::Implementation::Util::GetSystemErrorString(x)

#define DiscardReturnValue(x) (void)(x)

#ifdef _DEBUG
#define Dbg(x, ...) QuantumGate::Implementation::Util::DisplayDebugMessage(x, __VA_ARGS__)
#define DbgInvoke(f) std::invoke(f)
#else
#define Dbg(x, ...) ((void)0)
#define DbgInvoke(f) ((void)0)
#endif
