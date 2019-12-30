// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\TestExtender\TestExtender.h"

bool HandleCommand(const QuantumGate::String& cmdline);
void DisplayHelp() noexcept;
bool Send(const std::wstring& pluidstr, const std::wstring& msg, const std::wstring& count);
bool SetVerbosity(const std::wstring& verb);
