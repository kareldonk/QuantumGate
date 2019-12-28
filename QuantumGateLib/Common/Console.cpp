// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Console.h"

#include <iostream>
#include <fcntl.h>
#include <io.h>

namespace QuantumGate::Implementation
{
	struct ConsoleObject final
	{
		std::atomic<Console::Verbosity> Verbosity{ Console::Verbosity::Silent };
		std::atomic_bool HasOutput{ false };
		Concurrency::ThreadSafe<std::shared_ptr<Console::Output>, std::shared_mutex> Output{ nullptr };

		[[nodiscard]] inline bool CanAddMessage(const Console::MessageType type) const noexcept
		{
			if (!HasOutput.load() ||
				!(static_cast<Int16>(type) & static_cast<Int16>(Verbosity.load()))) return false;

			return true;
		}
	};

	ConsoleObject& GetConsoleObject() noexcept
	{
		static ConsoleObject ConsoleObj;
		return ConsoleObj;
	}

	Console::Window::Window() noexcept
	{
		AllocConsole();

		_wfreopen_s(&m_ConsoleInput, L"CONIN$", L"r", stdin);
		_wfreopen_s(&m_ConsoleOutput, L"CONOUT$", L"w", stdout);
		_wfreopen_s(&m_ConsoleErrOutput, L"CONOUT$", L"w", stderr);

		try
		{
			// Clear all error states
			std::wcin.clear();
			std::wcout.clear();
			std::wcerr.clear();
		}
		catch (...) {}

		SetConsoleTitle(L"QuantumGate Console");
	}

	Console::Window::~Window()
	{
		if (m_ConsoleInput != nullptr)
		{
			fclose(m_ConsoleInput);
			m_ConsoleInput = nullptr;
		}

		if (m_ConsoleOutput != nullptr)
		{
			fclose(m_ConsoleOutput);
			m_ConsoleOutput = nullptr;
		}

		if (m_ConsoleErrOutput != nullptr)
		{
			fclose(m_ConsoleErrOutput);
			m_ConsoleErrOutput = nullptr;
		}

		FreeConsole();
	}

	template<bool Check>
	Console::Log<Check>& Console::Log<Check>::operator<<(const Format fmt)
	{
		GetConsoleObject().Output.WithSharedLock([&](const auto& output)
		{
			if (output != nullptr)
			{
				m_StringStream << output->GetFormat(m_MessageType, fmt);
			}
		});

		return *this;
	}

	// Explicit instantiations
	template class Export Console::Log<false>;
	template class Export Console::Log<true>;

	void Console::SetVerbosity(const Verbosity verbosity) noexcept
	{
		GetConsoleObject().Verbosity = verbosity;
	}

	const Console::Verbosity Console::GetVerbosity() noexcept
	{
		return GetConsoleObject().Verbosity;
	}

	bool Console::SetOutput(const std::shared_ptr<Output>& output) noexcept
	{
		try
		{
			if (output != nullptr)
			{
				GetConsoleObject().Output.WithUniqueLock() = output;
				GetConsoleObject().HasOutput = true;
			}
			else
			{
				GetConsoleObject().HasOutput = false;
				GetConsoleObject().Output.WithUniqueLock()->reset();
			}

			return true;
		}
		catch (...) {}

		return false;
	}

	bool Console::CanAddMessage(const MessageType type) noexcept
	{
		return GetConsoleObject().CanAddMessage(type);
	}

	void Console::AddMessageWithArgs(const MessageType type, const WChar* message, ...) noexcept
	{
		va_list argptr = nullptr;
		va_start(argptr, message);

		AddMessageNoArgs(type, Util::FormatString(message, argptr).c_str());

		va_end(argptr);
	}

	void Console::AddMessageNoArgs(const MessageType type, const WChar* message) noexcept
	{
		try
		{
			GetConsoleObject().Output.WithUniqueLock([&](const auto& output)
			{
				if (output != nullptr)
				{
					output->AddMessage(type, message);
				}
			});
		}
		catch (...) {}
	}

	Console::TerminalOutput::TerminalOutput() noexcept
	{
		InitConsole();
	}

	bool Console::TerminalOutput::InitConsole() noexcept
	{
		// Set output mode to handle virtual terminal sequences
		std::array<ULong, 2> handles{ STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };

		for (auto& handle : handles)
		{
			HANDLE stdhandle = GetStdHandle(handle);
			if (stdhandle != INVALID_HANDLE_VALUE)
			{
				DWORD mode = 0;
				if (GetConsoleMode(stdhandle, &mode))
				{
					mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_ECHO_INPUT |
						ENABLE_LINE_INPUT | ENABLE_LVB_GRID_WORLDWIDE;

					if (!SetConsoleMode(stdhandle, mode)) return false;
				}
				else return false;
			}
			else return false;
		}

		// Enable unicode
		if (_setmode(_fileno(stdout), _O_U8TEXT) == -1 ||
			_setmode(_fileno(stderr), _O_U8TEXT) == -1)
		{
			return false;
		}

		return true;
	}

	const WChar* Console::TerminalOutput::GetFormat(const MessageType type, const Format fmt) const noexcept
	{
		switch (fmt)
		{
			case Format::Default:
			{
				switch (type)
				{
					case MessageType::System:
						return Colors::DefaultSys;
					case MessageType::Info:
						return Colors::DefaultInfo;
					case MessageType::Warning:
						return Colors::DefaultWarn;
					case MessageType::Error:
						return Colors::DefaultErr;
					case MessageType::Debug:
						return Colors::DefaultDbg;
					default:
						return Colors::Default;
				}
			}
			case Format::Reset:
				return Colors::Reset;
			case Format::Bold:
				return Colors::Bold;
			case Format::Dim:
				return Colors::Dim;
			case Format::Standout:
				return Colors::Standout;
			case Format::Underline:
				return Colors::Underline;
			case Format::Blink:
				return Colors::Blink;
			case Format::Reverse:
				return Colors::Reverse;
			case Format::Hidden:
				return Colors::Hidden;

			case Format::FGBlack:
				return Colors::FGBlack;
			case Format::FGRed:
				return Colors::FGRed;
			case Format::FGGreen:
				return Colors::FGGreen;
			case Format::FGYellow:
				return Colors::FGYellow;
			case Format::FGBlue:
				return Colors::FGBlue;
			case Format::FGMagenta:
				return Colors::FGMagenta;
			case Format::FGCyan:
				return Colors::FGCyan;
			case Format::FGWhite:
				return Colors::FGWhite;
			case Format::FGBrightBlack:
				return Colors::FGBrightBlack;
			case Format::FGBrightRed:
				return Colors::FGBrightRed;
			case Format::FGBrightGreen:
				return Colors::FGBrightGreen;
			case Format::FGBrightYellow:
				return Colors::FGBrightYellow;
			case Format::FGBrightBlue:
				return Colors::FGBrightBlue;
			case Format::FGBrightMagenta:
				return Colors::FGBrightMagenta;
			case Format::FGBrightCyan:
				return Colors::FGBrightCyan;
			case Format::FGBrightWhite:
				return Colors::FGBrightWhite;

			case Format::BGBlack:
				return Colors::BGBlack;
			case Format::BGRed:
				return Colors::BGRed;
			case Format::BGGreen:
				return Colors::BGGreen;
			case Format::BGYellow:
				return Colors::BGYellow;
			case Format::BGBlue:
				return Colors::BGBlue;
			case Format::BGMagenta:
				return Colors::BGMagenta;
			case Format::BGCyan:
				return Colors::BGCyan;
			case Format::BGWhite:
				return Colors::BGWhite;
			case Format::BGBrightBlack:
				return Colors::BGBrightBlack;
			case Format::BGBrightRed:
				return Colors::BGBrightRed;
			case Format::BGBrightGreen:
				return Colors::BGBrightGreen;
			case Format::BGBrightYellow:
				return Colors::BGBrightYellow;
			case Format::BGBrightBlue:
				return Colors::BGBrightBlue;
			case Format::BGBrightMagenta:
				return Colors::BGBrightMagenta;
			case Format::BGBrightCyan:
				return Colors::BGBrightCyan;
			case Format::BGBrightWhite:
				return Colors::BGBrightWhite;
			default:
				break;
		}

		return Colors::Default;
	}

	void Console::TerminalOutput::AddMessage(const MessageType type, const WChar* message)
	{
		// Default output is cout
		auto output = &std::wcout;
		if (type == MessageType::Error)
		{
			// Error output goes to cerr
			output = &std::wcerr;
		}

		*output << GetFormat(type, Format::Default);

		if (type != MessageType::System)
		{
			std::array<WChar, 128> timestr{ 0 };
			if (Util::GetCurrentLocalTime(L"%d/%m/%Y %H:%M:%S", timestr))
			{
				*output << L"[" << timestr.data() << L"] ";
			}
			else *output << L"[Unknown] ";
		}

		*output << message << GetFormat(type, Format::Reset) << L"\r\n";

#ifdef _DEBUG
		// Additionally send debug messages to the debug output
		if (type == MessageType::Debug) Dbg(message);
#endif
	}

	Console::WindowOutput::WindowOutput() noexcept
	{
		m_ConsoleWindow = std::make_unique<Console::Window>();

		InitConsole();
	}

	Console::WindowOutput::~WindowOutput()
	{
		m_ConsoleWindow.reset();
	}

	void Console::WindowOutput::AddMessage(const MessageType type, const WChar* message)
	{
		Console::TerminalOutput::AddMessage(type, message);
	}
}
