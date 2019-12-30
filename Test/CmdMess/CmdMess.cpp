// This file is part of the QuantumGate project. For copyright and
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
	std::wstring ID;
	std::wstring RegEx;
	std::wstring Usage;
};

static std::vector<Command> commands = {
	Command{ L"connect",	L"^connect\\s+([^\\s]*):(\\d+)$",				L"connect [IP Address]:[Port]" },
	Command{ L"disconnect",	L"^disconnect\\s+([^\\s]+)$",					L"disconnect [Peer LUID]" },
	Command{ L"send",		L"^send\\s+([^\\s]+)\\s+\"(.+)\"\\s*(\\d*)$",	L"send [Peer LUID] \"[Message]\" [Number of times]" },
	Command{ L"seclevel",	L"^set\\s+security\\s+level\\s+(\\d+)$",		L"set security level [Level: 1-5]" },
	Command{ L"verbosity",	L"^set\\s+verbosity\\s+([^\\s]+)$",				L"set verbosity [Verbosity: silent, minimal, normal, verbose, debug]" },
	Command{ L"help",		L"^help\\s?$|^\\?\\s?$",						L"help or ?" },
	Command{ L"quit",		L"^quit\\s?$|^exit\\s?$",						L"quit or exit" }
};

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
	params.Listeners.Enable = true;
	params.Listeners.TCPPorts = { 9999 };
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
		PrintErrLine(L"Failed to add an IP filter");
		return -1;
	}

	if (const auto result = m_QuantumGate.Startup(params); result.Succeeded())
	{
		PrintInfoLine(L"\r\nQuantumGate startup successful\r\n\r\nType a command and press Enter. Type 'help' for help.\r\n");
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
	if (cmdline.size() > 0)
	{
		auto handled{ false };

		for (auto& cmd : commands)
		{
			std::wregex r(cmd.RegEx, std::regex_constants::icase);
			std::wsmatch m;
			if (regex_search(cmdline, m, r))
			{
				if (cmd.ID == L"disconnect")
				{
					wchar_t* end{ nullptr };
					const PeerLUID pluid = wcstoull(m[1].str().c_str(), &end, 10);

					const auto result = m_QuantumGate.DisconnectFrom(pluid, [](PeerLUID pluid, PeerUUID puuid) mutable
					{
						PrintInfoLine(L"Peer %llu disconnected", pluid);
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
				else if (cmd.ID == L"connect")
				{
					wchar_t* end{ nullptr };
					const auto port = wcstoul(m[2].str().c_str(), &end, 10);

					IPAddress addr;
					if (IPAddress::TryParse(m[1].str().c_str(), addr))
					{
						const auto endp = IPEndpoint(addr, static_cast<UInt16>(port));

						const auto result = m_QuantumGate.ConnectTo({ endp }, [&](PeerLUID pluid, Result<ConnectDetails> cresult) mutable
						{
							if (cresult.Succeeded())
							{
								PrintInfoLine(L"Successfully connected to endpoint %s with peer LUID %llu (%s, %s)",
											  endp.GetString().c_str(), pluid,
											  cresult->IsAuthenticated ? L"Authenticated" : L"NOT Authenticated",
											  cresult->IsRelayed ? L"Relayed" : L"NOT Relayed");
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
					else PrintErrLine(L"Invalid IP address specified");
				}
				else if (cmd.ID == L"seclevel")
				{
					wchar_t* end{ nullptr };
					const auto lvl = wcstoul(m[1].str().c_str(), &end, 10);
					const auto seclvl = static_cast<SecurityLevel>(lvl);

					if (m_QuantumGate.SetSecurityLevel(seclvl))
					{
						PrintInfoLine(L"Security level set to %s", m[1].str().c_str());
					}
					else
					{
						PrintErrLine(L"Failed to change security level");
					}
				}
				else if (cmd.ID == L"verbosity")
				{
					SetVerbosity(m[1].str());
				}
				else if (cmd.ID == L"send")
				{
					Send(m[1].str(), m[2].str(), m[3].str());
				}
				else if (cmd.ID == L"help")
				{
					DisplayHelp();
				}
				else if (cmd.ID == L"quit")
				{
					PrintInfoLine(L"Shutting down QuantumGate, please wait...\r\n");

					if (const auto result = m_QuantumGate.Shutdown(); result.Succeeded())
					{
						PrintInfoLine(L"\r\nQuantumGate shut down successful\r\n");
					}
					else
					{
						PrintErrLine(L"QuantumGate shut down failed: %s", result.GetErrorDescription().c_str());
					}

					PrintInfoLine(L"\r\nBye...\r\n");

					return false;
				}

				handled = true;

				break;
			}
		}

		if (!handled)
		{
			PrintErrLine(L"Unrecognized command or bad syntax: %s", cmdline.c_str());
			PrintErrLine(L"Type 'help' or '?' and press Enter for help.", cmdline.c_str());
		}
	}

	return true;
}

bool Send(const std::wstring& pluidstr, const std::wstring& msg, const std::wstring& count)
{
	wchar_t* end{ nullptr };
	const PeerLUID pluid{ wcstoull(pluidstr.c_str(), &end, 10) };
	int nmess{ 1 };

	if (count.size() > 0) nmess = wcstoul(count.c_str(), &end, 10);

	PrintInfoLine(L"Sending message '%s' to peer %llu, %d %s", msg.c_str(), pluid, nmess, ((nmess == 1) ? L"time..." : L"times..."));

	auto success{ true };
	const auto begin{ std::chrono::high_resolution_clock::now() };

	String txt;

	for (int x = 0; x < nmess; ++x)
	{
		txt = msg;

		if (nmess > 1)
		{
			txt += Util::FormatString(L" #%d", x);
		}

		if (!m_Extender->SendMessage(pluid, txt, SendParameters::PriorityOption::Normal, std::chrono::milliseconds(0)))
		{
			PrintErrLine(L"Could not send message %d to peer", x);
			success = false;
			break;
		}
	}

	if (success)
	{
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);
		PrintInfoLine(L"Sent in %d milliseconds", ms.count());
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
		PrintInfoLine(L"Console verbosity set to silent");
	}
	else if (verb == L"minimal")
	{
		set = true;
		verbosity = Console::Verbosity::Minimal;
		PrintInfoLine(L"Console verbosity set to minimal");
	}
	else if (verb == L"normal")
	{
		set = true;
		verbosity = Console::Verbosity::Normal;
		PrintInfoLine(L"Console verbosity set to normal");
	}
	else if (verb == L"verbose")
	{
		set = true;
		verbosity = Console::Verbosity::Verbose;
		PrintInfoLine(L"Console verbosity set to verbose");
	}
	else if (verb == L"debug")
	{
		set = true;
		verbosity = Console::Verbosity::Debug;
		PrintInfoLine(L"Console verbosity set to debug");
	}
	else PrintErrLine(L"Unknown console verbosity level");

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
	for (auto& command : commands)
	{
		if (command.ID.size() > maxlen) maxlen = command.ID.size();
	}

	auto fixlen = [&](const std::wstring& str)
	{
		String str2{ str };
		while (str2.size() < maxlen)
		{
			str2 += L" ";
		}

		return str2;
	};

	for (auto& command : commands)
	{
		output += L"\t";
		output += Console::TerminalOutput::Colors::FGBrightYellow + fixlen(command.ID) + Console::TerminalOutput::Colors::FGBlack;
		output += Console::TerminalOutput::Colors::FGBrightBlack;
		output += L"\t\tUsage: ";
		output += Console::TerminalOutput::Colors::FGWhite + command.Usage + Console::TerminalOutput::Colors::FGWhite;
		output += L"\r\n\r\n";
	}

	PrintInfoLine(output.c_str());
}