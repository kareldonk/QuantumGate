// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "CmdConsole.h"

#include <Common\Util.h>

using namespace QuantumGate;
using namespace QuantumGate::Implementation;

void CmdConsole::PrintLine(const PrintColor pc, const QuantumGate::WChar* message, ...) noexcept
{
	va_list argptr = nullptr;
	va_start(argptr, message);

	const auto color = std::invoke([&]()
	{
		switch (pc)
		{
			case PrintColor::Info:
				return QuantumGate::Console::TerminalOutput::Colors::DefaultInfo;
			case PrintColor::Warning:
				return QuantumGate::Console::TerminalOutput::Colors::DefaultWarn;
			case PrintColor::Error:
				return QuantumGate::Console::TerminalOutput::Colors::DefaultErr;
			case PrintColor::Debug:
				return QuantumGate::Console::TerminalOutput::Colors::DefaultDbg;
			default:
				return QuantumGate::Console::TerminalOutput::Colors::Default;
		}
	});

	{
		std::unique_lock lock(GetTerminalMutex());

		EraseCommandLine();

		std::wcout << color << Util::FormatString(message, argptr) <<
			QuantumGate::Console::TerminalOutput::Colors::Reset << L"\r\n";

		DisplayPromptImpl();
	}

	va_end(argptr);
}