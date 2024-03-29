﻿// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "CmdMess.h"
#include "CmdConsole.h"

#include <Common\Util.h>
#include <thread>

using namespace QuantumGate;
using namespace QuantumGate::Implementation;

struct Command final
{
	enum class ID { Connect, Disconnect, Query, Send, SecLevel, Verbosity, Help, Quit };

	const ID ID;
	const std::wstring Name;
	const std::wstring RegEx;
	const std::wstring Usage;
};

static std::array commands = {
	Command{
		Command::ID::Connect,
		L"connect",
		L"^connect\\s+([^\\s]*):(\\d+)$",
		L"connect [IP Address]:[Port]"
	},
	Command{
		Command::ID::Disconnect,
		L"disconnect",
		L"^disconnect\\s+([^\\s]+)$",
		L"disconnect [Peer LUID]"
	},
	Command{
		Command::ID::Query,
		L"query",
		L"^query\\s+peers\\s+(.*?)$",
		L"query peers [Parameters: all]"
	},
	Command{
		Command::ID::Send,
		L"send",
		L"^send\\s+([^\\s]+)\\s+\"(.+)\"\\s*(\\d*)$",
		L"send [Peer LUID] \"[Message]\" [Number of times]"
	},
	Command{
		Command::ID::SecLevel,
		L"seclevel",
		L"^set\\s+security\\s+level\\s+(\\d+)$",
		L"set security level [Level: 1-5]"
	},
	Command{
		Command::ID::Verbosity,
		L"verbosity",
		L"^set\\s+verbosity\\s+([^\\s]+)$",
		L"set verbosity [Verbosity: silent, minimal, normal, verbose, debug]"
	},
	Command{
		Command::ID::Help,
		L"help",
		L"^help\\s?$|^\\?\\s?$",
		L"help or ?"
	},
	Command{
		Command::ID::Quit,
		L"quit",
		L"^quit\\s?$|^exit\\s?$",
		L"quit or exit"
	}
};

template<typename T> requires (std::is_integral_v<T>)
[[nodiscard]] bool ParseNumber(const wchar_t* str, T& out) noexcept
{
	const auto len = std::wcslen(str);
	if (len > 0)
	{
		wchar_t* end{ nullptr };
		errno = 0;
		T num{ 0 };

		if constexpr (std::is_same_v<T, unsigned long> || std::is_same_v<T, unsigned int> || std::is_same_v<T, unsigned short>)
		{
			num = static_cast<T>(std::wcstoul(str, &end, 10));
		}
		else if constexpr (std::is_same_v<T, unsigned long long>)
		{
			num = std::wcstoull(str, &end, 10);
		}
		else
		{
			throw std::invalid_argument("Unsupported type.");
		}

		if ((end == str + len) && errno == 0)
		{
			out = num;
			return true;
		}
	}

	return false;
}

static Local m_QuantumGate;
static std::shared_ptr<TestExtender::Extender> m_Extender;

int main()
{
	// Send console output to CmdConsole
	Console::SetOutput(std::make_shared<CmdConsole>());
	Console::SetVerbosity(Console::Verbosity::Debug);

	PrintInfoLine(L"Starting QuantumGate, please wait...\r\n");

	StartupParameters params;
	params.UUID.Set(L"5a378a95-f00e-d9a0-532f-8d3a036117bf");

	params.SupportedAlgorithms.Hash = { Algorithm::Hash::SHA256, Algorithm::Hash::SHA512,
		Algorithm::Hash::BLAKE2S256, Algorithm::Hash::BLAKE2B512 };

	params.SupportedAlgorithms.PrimaryAsymmetric = {
		Algorithm::Asymmetric::ECDH_X448, Algorithm::Asymmetric::ECDH_X25519 };

	params.SupportedAlgorithms.SecondaryAsymmetric = { Algorithm::Asymmetric::KEM_NEWHOPE,
		Algorithm::Asymmetric::KEM_NTRUPRIME };

	params.SupportedAlgorithms.Symmetric = { Algorithm::Symmetric::AES256_GCM,
		Algorithm::Symmetric::CHACHA20_POLY1305 };

	params.SupportedAlgorithms.Compression = { Algorithm::Compression::DEFLATE,
		Algorithm::Compression::ZSTANDARD };

	params.RequireAuthentication = false;
	params.Listeners.TCP.Enable = true;
	params.Listeners.TCP.Ports = { 9999 };
	params.EnableExtenders = true;
	params.Relays.Enable = true;

	m_Extender = std::make_shared<TestExtender::Extender>(nullptr);
	auto extp = std::static_pointer_cast<Extender>(m_Extender);

	if (const auto result = m_QuantumGate.AddExtender(extp); result.Failed())
	{
		PrintErrLine(L"Failed to add extender: %s", result.GetErrorDescription().c_str());
		return -1;
	}

	// Allow access by default
	m_QuantumGate.GetAccessManager().SetPeerAccessDefault(Access::PeerAccessDefault::Allowed);

	// Allow all IP addresses to connect
	if (!m_QuantumGate.GetAccessManager().AddIPFilter(L"0.0.0.0/0", Access::IPFilterType::Allowed) ||
		!m_QuantumGate.GetAccessManager().AddIPFilter(L"::/0", Access::IPFilterType::Allowed))
	{
		PrintErrLine(L"Failed to add an IP filter.");
		return -1;
	}

	if (const auto result = m_QuantumGate.Startup(params); result.Succeeded())
	{
		PrintInfoLine(L"\r\nQuantumGate startup successful.\r\n\r\nType a command and press Enter. Type 'help' for help.\r\n");
	}
	else
	{
		PrintErrLine(L"Failed to start QuantumGate: %s", result.GetErrorDescription().c_str());
		return -1;
	}

	CmdConsole::SetDisplayPrompt(true);
	CmdConsole::DisplayPrompt();

	while (true)
	{
		if (CmdConsole::HasInputEvent())
		{
			if (CmdConsole::ProcessInputEvent() == CmdConsole::KeyInputEventResult::ReturnPressed)
			{
				CmdConsole::SetDisplayPrompt(false);

				if (!HandleCommand(CmdConsole::AcceptCommandLine())) break;

				CmdConsole::SetDisplayPrompt(true);
				CmdConsole::DisplayPrompt();
			}
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
	}

	return 0;
}

bool HandleCommand(const String& cmdline)
{
	if (!cmdline.empty())
	{
		auto handled{ false };

		for (const auto& cmd : commands)
		{
			std::wregex r(cmd.RegEx, std::regex_constants::icase);
			std::wsmatch m;
			if (std::regex_search(cmdline, m, r))
			{
				switch (cmd.ID)
				{
					case Command::ID::Connect:
					{
						UInt16 port{ 0 };
						if (ParseNumber(m[2].str().c_str(), port))
						{
							IPAddress addr;
							if (IPAddress::TryParse(m[1].str().c_str(), addr))
							{
								const auto endp = IPEndpoint(IPEndpoint::Protocol::TCP, addr, static_cast<UInt16>(port));

								const auto result = m_QuantumGate.ConnectTo({ endp }, [=](PeerLUID pluid, Result<Peer> cresult) mutable
								{
									if (cresult.Succeeded())
									{
										PrintInfoLine(L"Successfully connected to endpoint %s with peer LUID %llu (%s, %s).",
													  endp.GetString().c_str(), pluid,
													  cresult->GetAuthenticated().GetValue() ? L"Authenticated" : L"NOT Authenticated",
													  cresult->GetRelayed().GetValue() ? L"Relayed" : L"NOT Relayed");
									}
									else
									{
										PrintErrLine(L"Failed to connect to endpoint %s: %s", endp.GetString().c_str(), cresult.GetErrorDescription().c_str());
									}
								});

								if (result.Succeeded())
								{
									PrintInfoLine(L"Connecting to endpoint %s...", endp.GetString().c_str());
								}
								else
								{
									PrintErrLine(L"Failed to connect to endpoint %s: %s", endp.GetString().c_str(), result.GetErrorDescription().c_str());
								}
							}
							else
							{
								PrintErrLine(L"Invalid IP address specified.");
							}
						}
						else
						{
							PrintErrLine(L"Invalid port specified.");
						}
						break;
					}
					case Command::ID::Disconnect:
					{
						PeerLUID pluid{ 0 };
						if (ParseNumber(m[1].str().c_str(), pluid))
						{
							const auto result = m_QuantumGate.DisconnectFrom(pluid, [](PeerLUID pluid, PeerUUID puuid) mutable
							{
								PrintInfoLine(L"Peer %llu disconnected.", pluid);
							});

							if (result.Succeeded())
							{
								PrintInfoLine(L"Disconnecting peer %llu...", pluid);
							}
							else
							{
								PrintErrLine(L"Could not disconnect peer %llu: %s", pluid, result.GetErrorDescription().c_str());
							}
						}
						else
						{
							PrintErrLine(L"Invalid peer LUID specified.");
						}
						break;
					}
					case Command::ID::Query:
					{
						QueryPeers(m[1].str());
						break;
					}
					case Command::ID::SecLevel:
					{
						UInt16 lvl{ 0 };
						if (ParseNumber(m[1].str().c_str(), lvl))
						{
							const auto seclvl = static_cast<SecurityLevel>(lvl);

							if (m_QuantumGate.SetSecurityLevel(seclvl))
							{
								PrintInfoLine(L"Security level set to %s.", m[1].str().c_str());
							}
							else
							{
								PrintErrLine(L"Failed to change security level.");
							}
						}
						else
						{
							PrintErrLine(L"Invalid security level specified.");
						}
						break;
					}
					case Command::ID::Verbosity:
					{
						SetVerbosity(m[1].str());
						break;
					}
					case Command::ID::Send:
					{
						Send(m[1].str(), m[2].str(), m[3].str());
						break;
					}
					case Command::ID::Help:
					{
						DisplayHelp();
						break;
					}
					case Command::ID::Quit:
					{
						PrintInfoLine(L"Shutting down QuantumGate, please wait...\r\n");

						if (const auto result = m_QuantumGate.Shutdown(); result.Succeeded())
						{
							PrintInfoLine(L"\r\nQuantumGate shut down successful.\r\n");
						}
						else
						{
							PrintErrLine(L"QuantumGate shut down failed: %s", result.GetErrorDescription().c_str());
						}

						PrintInfoLine(L"\r\nBye...\r\n");

						return false;
					}
					default:
					{
						assert(false);
						break;
					}
				}

				handled = true;
				break;
			}
		}

		if (!handled)
		{
			PrintErrLine(L"Unrecognized command or bad syntax: %s", cmdline.c_str());
			PrintErrLine(L"Type 'help' or '?' and press Enter for help.");
		}
	}

	return true;
}

bool Send(const std::wstring& pluidstr, const std::wstring& msg, const std::wstring& count)
{
	PeerLUID pluid{ 0 };
	if (!ParseNumber(pluidstr.c_str(), pluid))
	{
		PrintErrLine(L"Invalid peer LUID specified.");
		return false;
	}

	UInt nmess{ 1 };

	if (count.size() > 0)
	{
		if (!ParseNumber(count.c_str(), nmess))
		{
			PrintErrLine(L"Invalid number of messages specified.");
			return false;
		}
	}

	PrintInfoLine(L"Sending message '%s' to peer %llu, %d %s", msg.c_str(), pluid, nmess, ((nmess == 1u) ? L"time..." : L"times..."));

	auto success{ true };
	const auto begin{ std::chrono::high_resolution_clock::now() };

	String txt;

	for (UInt x = 0u; x < nmess; ++x)
	{
		txt = msg;

		if (nmess > 1u)
		{
			txt += Util::FormatString(L" #%d", x);
		}

		if (!m_Extender->SendMessage(pluid, txt, SendParameters::PriorityOption::Normal, std::chrono::milliseconds(0)))
		{
			PrintErrLine(L"Could not send message %d to peer.", x);
			success = false;
			break;
		}
	}

	if (success)
	{
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);
		PrintInfoLine(L"Sent in %jd milliseconds.", ms.count());
	}

	return success;
}

bool SetVerbosity(const std::wstring& verb)
{
	auto set{ false };
	auto verbosity{ Console::Verbosity::Debug };

	if (verb == L"silent")
	{
		set = true;
		verbosity = Console::Verbosity::Silent;
		PrintInfoLine(L"Console verbosity set to silent.");
	}
	else if (verb == L"minimal")
	{
		set = true;
		verbosity = Console::Verbosity::Minimal;
		PrintInfoLine(L"Console verbosity set to minimal.");
	}
	else if (verb == L"normal")
	{
		set = true;
		verbosity = Console::Verbosity::Normal;
		PrintInfoLine(L"Console verbosity set to normal.");
	}
	else if (verb == L"verbose")
	{
		set = true;
		verbosity = Console::Verbosity::Verbose;
		PrintInfoLine(L"Console verbosity set to verbose.");
	}
	else if (verb == L"debug")
	{
		set = true;
		verbosity = Console::Verbosity::Debug;
		PrintInfoLine(L"Console verbosity set to debug.");
	}
	else PrintErrLine(L"Unknown console verbosity level.");

	if (set) Console::SetVerbosity(verbosity);

	return set;
}

void DisplayHelp() noexcept
{
	String output{ L"\r\n" };
	output += Console::TerminalOutput::Colors::FGBrightGreen;
	output += L"Supported commands:";
	output += Console::TerminalOutput::Colors::FGWhite;
	output += L"\r\n\r\n";

	size_t maxlen{ 0 };
	for (const auto& command : commands)
	{
		if (command.Name.size() > maxlen) maxlen = command.Name.size();
	}

	for (const auto& command : commands)
	{
		output += L"\t";
		output += Console::TerminalOutput::Colors::FGBrightYellow;
		output += PadRight(command.Name, maxlen);
		output += Console::TerminalOutput::Colors::FGBlack;
		output += Console::TerminalOutput::Colors::FGBrightBlack;
		output += L"\t\tUsage: ";
		output += Console::TerminalOutput::Colors::FGWhite;
		output += command.Usage;
		output += Console::TerminalOutput::Colors::FGWhite;
		output += L"\r\n\r\n";
	}

	PrintInfoLine(output.c_str());
}

void QueryPeers(const std::wstring& verb)
{
	const PeerQueryParameters pm;

	if (const auto result = m_QuantumGate.QueryPeers(pm); result.Succeeded())
	{
		if (result->size() == 0)
		{
			PrintInfoLine(L"No peers found.");
			return;
		}

		struct Column
		{
			const WChar* Name{ nullptr };
			const Size Len{ 0 };
		};

		std::array columns = {
			Column{ L"LUID", 20 },
			Column{ L"UUID", 37 },
			Column{ L"Auth.", 6 },
			Column{ L"Relay", 6 },
			Column{ L"Peer Endpoint", 46 }
		};

		String output{ L"\r\n" };
		output += Console::TerminalOutput::Colors::FGBrightGreen;
		output += Util::FormatString(L"%zu connected %s:\r\n\r\n", result->size(), result->size() == 1 ? L"peer" : L"peers");
		output += QuantumGate::Console::TerminalOutput::Colors::BGBlue;
		output += QuantumGate::Console::TerminalOutput::Colors::FGBrightWhite;

		String hdr;
		for (const auto& column : columns)
		{
			hdr += PadRight(String{ column.Name }, column.Len);
			hdr += L" ";
		}

		output += PadRight(hdr, CmdConsole::GetWidth());
		output += L"\r\n";
		output += QuantumGate::Console::TerminalOutput::Colors::FGWhite;
		output += GetLine<String>(CmdConsole::GetWidth());

		for (const auto& pluid : *result)
		{
			m_QuantumGate.GetPeer(pluid).Succeeded([&](auto& result2)
			{
				if (const auto result3 = result2->GetDetails(); result3.Succeeded())
				{
					output += L"\r\n";
					output += PadRight(Util::FormatString(L"%llu", pluid), columns[0].Len);
					output += L" ";
					output += PadRight(Util::FormatString(L"%s", result3->PeerUUID.GetString().c_str()), columns[1].Len);
					output += L" ";
					output += PadRight(Util::FormatString(L"%s", result3->IsAuthenticated ? L"Yes" : L"No"), columns[2].Len);
					output += L" ";
					output += PadRight(Util::FormatString(L"%s", result3->IsRelayed ? L"Yes" : L"No"), columns[3].Len);
					output += L" ";
					output += PadRight(Util::FormatString(L"%s", result3->PeerEndpoint.GetString().c_str()), columns[4].Len);
				}
			});
		}

		output += QuantumGate::Console::TerminalOutput::Colors::Reset;
		output += L"\r\n";

		PrintInfoLine(output.c_str());
	}
	else
	{
		PrintErrLine(L"Failed to query peers: %s", result.GetErrorDescription().c_str());
	}
}