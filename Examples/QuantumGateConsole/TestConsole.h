// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

// Include the QuantumGate main header with API definitions
#include <QuantumGate.h>

#include <mutex>
#include <iostream>

// Our custom console class needs to inherit from QuantumGate::Console::Output
class TestConsole : public QuantumGate::Console::Output
{
public:
	TestConsole()
	{
		// The code below enables use of special color codes
		// in the standard terminal output. This isn't needed if
		// the output would go to a file or a text control.
		HANDLE stdhandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (stdhandle != INVALID_HANDLE_VALUE)
		{
			DWORD mode = 0;
			if (GetConsoleMode(stdhandle, &mode))
			{
				mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

				SetConsoleMode(stdhandle, mode);
			}
		}
	}

	// This function is called by QuantumGate to change output formatting;
	// override it to customize.
	const QuantumGate::WChar* GetFormat(const QuantumGate::Console::MessageType type,
										const QuantumGate::Console::Format fmt) const noexcept override
	{
		// This overrides all formatting and returns no formatting
		// except for the default where the text color is set to bright
		// magenta as an example. 
		// Depending on your output you could return formatting based on
		// the type and fmt parameters or simply ignore all formatting.
		switch (fmt)
		{
			case QuantumGate::Console::Format::Default:
				return L"\x1b[95m"; // bright magenta
			case QuantumGate::Console::Format::Reset:
				return L"\x1b[0m";
			default:
				break;
		}

		return L"";
	}

	// This function is called by QuantumGate whenever a message needs to be 
	// added to the console.
	// QuantumGate implements synchronization when calling the AddMessage
	// function from multiple threads. However, if you intend to call
	// this function yourself directly (instead of through Console::AddMessage,
	// or, if this function accesses non-const member variables in this class or
	// other data elsewhere that can also be accessed by other threads,
	// then you need to implement additional synchronization. 
	void AddMessage(const QuantumGate::Console::MessageType type,
					const QuantumGate::StringView message) override
	{
		// Simply output the message to the standard output. We could instead
		// send the output to a file or a text control in the case of an 
		// application with a GUI.
		std::wcout << GetFormat(type, QuantumGate::Console::Format::Default);
		std::wcout << message << L"\r\n";
		std::wcout << GetFormat(type, QuantumGate::Console::Format::Reset);
	}
};