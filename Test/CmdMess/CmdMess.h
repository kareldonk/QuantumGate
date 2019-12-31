// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\TestExtender\TestExtender.h"

template<typename T, typename = std::enable_if_t<std::is_same_v<T, QuantumGate::String> || std::is_same_v<T, std::wstring>>>
T PadRight(const T& str, const QuantumGate::Size maxlen)
{
	auto str2{ str };
	if (maxlen > str2.size()) str2.insert(str2.cend(), maxlen - str2.size(), L' ');
	return str2;
}

template<typename T, typename = std::enable_if_t<std::is_same_v<T, QuantumGate::String> || std::is_same_v<T, std::wstring>>>
T GetLine(const QuantumGate::Size len)
{
	T str;
	str.insert(str.cend(), len, L'-');
	return str;
}

bool HandleCommand(const QuantumGate::String& cmdline);
void DisplayHelp() noexcept;
bool Send(const std::wstring& pluidstr, const std::wstring& msg, const std::wstring& count);
bool SetVerbosity(const std::wstring& verb);
void QueryPeers(const std::wstring& verb);
