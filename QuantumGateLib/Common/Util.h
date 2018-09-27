// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <chrono>
#include <vector>

namespace QuantumGate::Implementation::Util
{
	Export String GetCurrentLocalTime(const String& format) noexcept;
	Export SystemTime GetCurrentSystemTime() noexcept;
	Export SteadyTime GetCurrentSteadyTime() noexcept;
	SystemTime ToTime(const Time& time) noexcept;
	Time ToTimeT(const SystemTime& time) noexcept;

	Export String FormatString(const StringView format, ...) noexcept;
	Export String FormatString(const StringView format, va_list arglist) noexcept;

	template<typename T>
	Export String ToBinaryString(const T bytes) noexcept;

	String ToBinaryString(const gsl::span<Byte> bytes) noexcept;
		
	Export String ToStringW(const std::string& txt) noexcept;
	Export std::string ToStringA(const String& txt) noexcept;

	Export std::optional<String> GetBase64(const BufferView& buffer) noexcept;
	Export std::optional<String> GetBase64(const Buffer& buffer) noexcept;
	Export std::optional<String> GetBase64(const Byte* buffer, const Size len) noexcept;
	Export std::optional<Buffer> FromBase64(const String& b64) noexcept;
	Export std::optional<Buffer> FromBase64(const std::string& b64) noexcept;
	Export std::optional<ProtectedBuffer> FromBase64(const ProtectedStringA& b64) noexcept;

	template<typename S, typename B>
	const bool FromBase64(const S& b64, B& buffer) noexcept;

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

	Export UInt64 NonPersistentHash(const String& txt) noexcept;
	Export UInt64 PersistentHash(const String& txt) noexcept;

	Export const bool SetThreadName(HANDLE thread, const String& name) noexcept;
	Export const bool SetCurrentThreadName(const String& name) noexcept;

	Export Int64 GetPseudoRandomNumber() noexcept;
	Export Int64 GetPseudoRandomNumber(const Int64 min, const Int64 max) noexcept;
	Export Buffer GetPseudoRandomBytes(const Size count);

	Export String GetSystemErrorString(const int code) noexcept;

	Export void DisplayDebugMessage(const StringView format, ...) noexcept;
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
