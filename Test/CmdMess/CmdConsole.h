// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <mutex>

class CmdConsole final : public QuantumGate::Console::TerminalOutput
{
public:
	enum class KeyInputEventResult
	{
		NoInput, NormalInput, ReturnPressed
	};

	CmdConsole()
	{
		DWORD mode{ 0 };
		if (GetConsoleMode(m_StdInHandle, &mode))
		{
			// Disable having to press enter before
			// reading input from stream with (w)cin.get() etc.
			mode &= ~ENABLE_LINE_INPUT;

			SetConsoleMode(m_StdInHandle, mode);
		}
	}

	virtual ~CmdConsole() {}

	void AddMessage(const QuantumGate::Console::MessageType type, const QuantumGate::WChar* message) override
	{
		std::unique_lock lock(GetTerminalMutex());

		EraseCommandLine();

		TerminalOutput::AddMessage(type, message);

		DisplayPromptImpl();
	}

	[[nodiscard]] static bool HasInputEvent() noexcept
	{
		INPUT_RECORD ir{ 0 };
		DWORD num{ 0 };
		if (PeekConsoleInput(m_StdInHandle, &ir, 1, &num))
		{
			if (num > 0)
			{
				return true;
			}
		}

		return false;
	}

	[[nodiscard]] static KeyInputEventResult ProcessInputEvent() noexcept
	{
		wchar_t wc{ 0 };

		INPUT_RECORD ir{ 0 };
		DWORD num{ 0 };

		if (ReadConsoleInput(m_StdInHandle, &ir, 1, &num))
		{
			if (num > 0 && ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown)
			{
				wc = ir.Event.KeyEvent.uChar.UnicodeChar;
			}
		}

		if (wc == 0) return KeyInputEventResult::NoInput;

		if (wc != L'\n' && wc != L'\r')
		{
			std::unique_lock lock(GetTerminalMutex());

			CONSOLE_SCREEN_BUFFER_INFO csbi{ 0 };
			GetConsoleScreenBufferInfo(m_StdOutHandle, &csbi);

			if (wc != L'\b')
			{
				std::wcout << wc;
				m_CommandLine += wc;

				if (csbi.dwCursorPosition.X >= csbi.dwMaximumWindowSize.X - 1)
				{
					std::wcout << L"\n";
					++m_CommandLineRowCount;
				}
			}
			else
			{
				if (m_CommandLine.size() > 0)
				{
					COORD c{ csbi.dwCursorPosition };

					if (csbi.dwCursorPosition.X == 0)
					{
						c.X = csbi.dwMaximumWindowSize.X - 1;
						c.Y = csbi.dwCursorPosition.Y - 1;

						--m_CommandLineRowCount;
					}
					else --c.X;

					DWORD num{ 0 };
					WriteConsoleOutputCharacter(m_StdOutHandle, L" ", 1, c, &num);

					SetConsoleCursorPosition(m_StdOutHandle, c);

					m_CommandLine.erase(m_CommandLine.size() - 1, 1);
				}
			}

			return KeyInputEventResult::NormalInput;
		}

		return KeyInputEventResult::ReturnPressed;
	}

	static void EraseCurrentConsoleRow() noexcept
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi{ 0 };
		GetConsoleScreenBufferInfo(m_StdOutHandle, &csbi);

		EraseConsoleRows(csbi.dwCursorPosition.Y, 1, csbi.dwMaximumWindowSize.X);
	}

	static void EraseCommandLine() noexcept
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi{ 0 };
		GetConsoleScreenBufferInfo(m_StdOutHandle, &csbi);

		EraseConsoleRows((csbi.dwCursorPosition.Y + 1) - m_CommandLineRowCount, m_CommandLineRowCount, csbi.dwMaximumWindowSize.X);
	}

	static void EraseConsoleRows(const int begin_row, const int num_rows, const int width) noexcept
	{
		COORD c{ 0 };
		DWORD num{ 0 };

		for (c.Y = begin_row; c.Y < begin_row + num_rows; ++c.Y)
		{
			for (c.X = 0; c.X < width; ++c.X)
			{
				if (!WriteConsoleOutputCharacter(m_StdOutHandle, L" ", 1, c, &num))
				{
					break;
				}
			}
		}

		c.X = 0;
		c.Y = begin_row;

		SetConsoleCursorPosition(m_StdOutHandle, c);
	}

	[[nodiscard]] static inline const QuantumGate::String& GetCommandLine() noexcept { return m_CommandLine; }

	static inline void ClearCommandLine() noexcept { m_CommandLine.clear(); }

	[[nodiscard]] static inline std::shared_mutex& GetTerminalMutex() noexcept { return m_TerminalMutex; }

	static void DisplayPrompt()
	{
		std::unique_lock lock(GetTerminalMutex());

		DisplayPromptImpl();
	}

private:
	static void DisplayPromptImpl()
	{
		std::wcout << L"\r\x1b[106m\x1b[30m" << L" QuantumGate: " << L"\x1b[49m\x1b[39m " << GetCommandLine();
	}

private:
	static inline HANDLE m_StdOutHandle{ GetStdHandle(STD_OUTPUT_HANDLE) };
	static inline HANDLE m_StdInHandle{ GetStdHandle(STD_INPUT_HANDLE) };
	static inline std::shared_mutex m_TerminalMutex;
	static inline QuantumGate::String m_CommandLine;
	static inline int m_CommandLineRowCount{ 1 };
};
