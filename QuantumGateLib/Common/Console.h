// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <sstream>

namespace QuantumGate::Implementation
{
	class Export Console final
	{
		class Window
		{
		public:
			Window() noexcept;
			Window(const Window&) = delete;
			Window(Window&&) = default;
			~Window();
			Window& operator=(const Window&) = delete;
			Window& operator=(Window&&) = default;

			FILE* m_ConsoleInput{ nullptr };
			FILE* m_ConsoleOutput{ nullptr };
			FILE* m_ConsoleErrOutput{ nullptr };
		};

	public:
		enum class MessageType : Int16
		{
			Error = 0b00000000'00000001,
			Warning = 0b00000000'00000010,
			System = 0b00000000'00000100,
			Info = 0b00000000'00001000,
			Debug = 0b00000000'00010000
		};

		enum class Verbosity : Int16
		{
			Silent = 0b00000000'00000000,
			Minimal = 0b00000000'00000011,
			Normal = 0b00000000'00000111,
			Verbose = 0b00000000'00001111,
			Debug = 0b00000000'00011111
		};

		enum class Format
		{
			Default,
			Reset,
			Bold,
			Dim,
			Standout,
			Underline,
			Blink,
			Reverse,
			Hidden, 

			FGBlack,
			FGRed,
			FGGreen,
			FGYellow,
			FGBlue,
			FGMagenta,
			FGCyan,
			FGWhite,
			FGBrightBlack,
			FGBrightRed,
			FGBrightGreen,
			FGBrightYellow,
			FGBrightBlue,
			FGBrightMagenta,
			FGBrightCyan,
			FGBrightWhite,

			BGBlack,
			BGRed,
			BGGreen,
			BGYellow,
			BGBlue,
			BGMagenta,
			BGCyan,
			BGWhite,
			BGBrightBlack,
			BGBrightRed,
			BGBrightGreen,
			BGBrightYellow,
			BGBrightBlue,
			BGBrightMagenta,
			BGBrightCyan,
			BGBrightWhite,
		};

		class Export Output
		{
		public:
			Output() = default;
			Output(const Output&) = delete;
			Output(Output&&) = default;
			virtual ~Output() = default;
			Output& operator=(const Output&) = delete;
			Output& operator=(Output&&) = default;

			virtual const WChar* GetFormat(const MessageType type, const Format fmt) const = 0;
			virtual void AddMessage(const MessageType type, const WChar* message) = 0;
		};

		class Export DummyOutput : public Output
		{
		public:
			const WChar* GetFormat(const MessageType type, const Format fmt) const noexcept override { return L""; }
			void AddMessage(const MessageType type, const WChar* message) noexcept override {}
		};

		class Export TerminalOutput : public Output
		{
			struct Colors final
			{
				static constexpr const WChar* Default{ L"\x1b[0;37m" };
				static constexpr const WChar* DefaultSys{ L"\x1b[0;97m" };
				static constexpr const WChar* DefaultInfo{ L"\x1b[0;37m" };
				static constexpr const WChar* DefaultWarn{ L"\x1b[0;93m" };
				static constexpr const WChar* DefaultErr{ L"\x1b[0;91m" };
				static constexpr const WChar* DefaultDbg{ L"\x1b[0;32m" };

				static constexpr const WChar* Reset{ L"\x1b[0m" };
				static constexpr const WChar* Bold{ L"\x1b[1m" };
				static constexpr const WChar* Dim{ L"\x1b[2m" };
				static constexpr const WChar* Standout{ L"\x1b[3m" };
				static constexpr const WChar* Underline{ L"\x1b[4m" };
				static constexpr const WChar* Blink{ L"\x1b[5m" };
				static constexpr const WChar* Reverse{ L"\x1b[7m" };
				static constexpr const WChar* Hidden{ L"\x1b[8m" };

				static constexpr const WChar* FGBlack{ L"\x1b[30m" };
				static constexpr const WChar* FGRed{ L"\x1b[31m" };
				static constexpr const WChar* FGGreen{ L"\x1b[32m" };
				static constexpr const WChar* FGYellow{ L"\x1b[33m" };
				static constexpr const WChar* FGBlue{ L"\x1b[34m" };
				static constexpr const WChar* FGMagenta{ L"\x1b[35m" };
				static constexpr const WChar* FGCyan{ L"\x1b[36m" };
				static constexpr const WChar* FGWhite{ L"\x1b[37m" };
				static constexpr const WChar* FGBrightBlack{ L"\x1b[90m" };
				static constexpr const WChar* FGBrightRed{ L"\x1b[91m" };
				static constexpr const WChar* FGBrightGreen{ L"\x1b[92m" };
				static constexpr const WChar* FGBrightYellow{ L"\x1b[93m" };
				static constexpr const WChar* FGBrightBlue{ L"\x1b[94m" };
				static constexpr const WChar* FGBrightMagenta{ L"\x1b[95m" };
				static constexpr const WChar* FGBrightCyan{ L"\x1b[96m" };
				static constexpr const WChar* FGBrightWhite{ L"\x1b[97m" };

				static constexpr const WChar* BGBlack{ L"\x1b[40m" };
				static constexpr const WChar* BGRed{ L"\x1b[41m" };
				static constexpr const WChar* BGGreen{ L"\x1b[42m" };
				static constexpr const WChar* BGYellow{ L"\x1b[43m" };
				static constexpr const WChar* BGBlue{ L"\x1b[44m" };
				static constexpr const WChar* BGMagenta{ L"\x1b[45m" };
				static constexpr const WChar* BGCyan{ L"\x1b[46m" };
				static constexpr const WChar* BGWhite{ L"\x1b[47m" };
				static constexpr const WChar* BGBrightBlack{ L"\x1b[100m" };
				static constexpr const WChar* BGBrightRed{ L"\x1b[101m" };
				static constexpr const WChar* BGBrightGreen{ L"\x1b[102m" };
				static constexpr const WChar* BGBrightYellow{ L"\x1b[103m" };
				static constexpr const WChar* BGBrightBlue{ L"\x1b[104m" };
				static constexpr const WChar* BGBrightMagenta{ L"\x1b[105m" };
				static constexpr const WChar* BGBrightCyan{ L"\x1b[106m" };
				static constexpr const WChar* BGBrightWhite{ L"\x1b[107m" };
			};

		public:
			TerminalOutput() noexcept;

			bool InitConsole() noexcept;

			const WChar* GetFormat(const MessageType type, const Format fmt) const noexcept override;
			void AddMessage(const MessageType type, const WChar* message) override;
		};

		class Export WindowOutput : public TerminalOutput
		{
		public:
			WindowOutput() noexcept;
			~WindowOutput();

			void AddMessage(const MessageType type, const WChar* message) override;

		protected:
			std::unique_ptr<Window> m_ConsoleWindow;
		};

		template<bool Check>
		class Export Log
		{
		public:
			Log(const MessageType type) noexcept : m_MessageType(type) {}
			Log(const Log&) = delete;
			Log(Log&&) = delete;

			virtual ~Log()
			{
				try
				{
					if constexpr (Check)
					{
						Console::AddMessage(m_MessageType, m_StringStream.str().c_str());
					}
					else
					{
						Console::AddMessageWithNoCheck(m_MessageType, m_StringStream.str().c_str());
					}
				}
				catch (...) {}
			}

			Log& operator=(const Log&) = delete;
			Log& operator=(Log&&) = delete;

			template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Format>>>
			Log& operator<<(const T& out)
			{
				m_StringStream << out;
				return *this;
			}

			Log& operator<<(const Format fmt);

		private:
			const MessageType m_MessageType{ MessageType::Debug };
			std::wostringstream m_StringStream;
		};

	private:
		Console() = default;

	public:
		static void SetVerbosity(const Verbosity verbosity) noexcept;
		[[nodiscard]] static const Verbosity GetVerbosity() noexcept;

		static bool SetOutput(const std::shared_ptr<Output>& output) noexcept;

		[[nodiscard]] static bool CanAddMessage(const MessageType type) noexcept;

		template<typename... Args>
		static void AddMessage(const MessageType type, const WChar* message, const Args&... args) noexcept
		{
			if (CanAddMessage(type)) AddMessageWithNoCheck(type, message, args...);
		}

		template<typename... Args>
		static void AddMessageWithNoCheck(const MessageType type, const WChar* message, const Args&... args) noexcept
		{
			if constexpr (sizeof...(Args) > 0)
			{
				AddMessageWithArgs(type, message, args...);
			}
			else
			{
				AddMessageNoArgs(type, message);
			}
		}

	private:
		static void AddMessageNoArgs(const MessageType type, const WChar* message) noexcept;
		static void AddMessageWithArgs(const MessageType type, const WChar* message, ...) noexcept;
	};
}

#define SLogFmt(x) QuantumGate::Implementation::Console::Format::x

#define SLogSys(x) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::System)) {}\
						else QuantumGate::Implementation::Console::Log<false>(QuantumGate::Implementation::Console::MessageType::System) << x

#define SLogInfo(x) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::Info)) {}\
						else QuantumGate::Implementation::Console::Log<false>(QuantumGate::Implementation::Console::MessageType::Info) << x

#define SLogWarn(x) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::Warning)) {}\
						else QuantumGate::Implementation::Console::Log<false>(QuantumGate::Implementation::Console::MessageType::Warning) << x

#define SLogErr(x) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::Error)) {}\
						else QuantumGate::Implementation::Console::Log<false>(QuantumGate::Implementation::Console::MessageType::Error) << x

#ifdef _DEBUG
#define SLogDbg(x) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::Debug)) {}\
						else QuantumGate::Implementation::Console::Log<false>(QuantumGate::Implementation::Console::MessageType::Debug) << x
#else
#define SLogDbg(x) ((void)0)
#endif

#define LogSys(x, ...) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::System)) {}\
							else QuantumGate::Implementation::Console::AddMessageWithNoCheck(\
								QuantumGate::Implementation::Console::MessageType::System, x, __VA_ARGS__)

#define LogInfo(x, ...) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::Info)) {}\
							else QuantumGate::Implementation::Console::AddMessageWithNoCheck(\
								QuantumGate::Implementation::Console::MessageType::Info, x, __VA_ARGS__)

#define LogWarn(x, ...) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::Warning)) {}\
							else QuantumGate::Implementation::Console::AddMessageWithNoCheck(\
								QuantumGate::Implementation::Console::MessageType::Warning, x, __VA_ARGS__)

#define LogErr(x, ...) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::Error)) {}\
							else QuantumGate::Implementation::Console::AddMessageWithNoCheck(\
								QuantumGate::Implementation::Console::MessageType::Error, x, __VA_ARGS__)

#ifdef _DEBUG
#define LogDbg(x, ...) if (!QuantumGate::Implementation::Console::CanAddMessage(QuantumGate::Implementation::Console::MessageType::Debug)) {}\
							else QuantumGate::Implementation::Console::AddMessageWithNoCheck(\
								QuantumGate::Implementation::Console::MessageType::Debug, x, __VA_ARGS__)
#else
#define LogDbg(x, ...) ((void)0)
#endif
