// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "CmdMess.h"
#include "CmdConsole.h"

#include <Common\Util.h>
#include <thread>

using namespace QuantumGate;
using namespace QuantumGate::Implementation;

struct Commands final
{
	std::wstring ID;
	std::wstring RegEx;
};

std::vector<Commands> commands = {
	{ L"disconnect", L"^disconnect\\s+([^\\s]+)$" },				// disconnect <PeerLUID>
	{ L"connect", L"^connect\\s+([^\\s]*):(\\d+)$" },				// connect <IPAddress>:<Port>
	{ L"quit", L"^quit\\s?$|^exit\\s?$" },							// quit or exit
	{ L"send", L"^send\\s+([^\\s]+)\\s+\"(.+)\"\\s*(\\d*)$" },		// send <PeerLUID> "<this message>" <Number of times>
	{ L"seclevel", L"^set\\s+security\\s+level\\s+(\\d+)$" },		// set security level <Level>
	{ L"verbosity", L"^set\\s+verbosity\\s+([^\\s]+)$" }			// set verbosity <Verbosity>
};

int main()
{
	// Send console output to terminal
	Console::SetOutput(std::make_shared<CmdConsole>());
	Console::SetVerbosity(Console::Verbosity::Debug);

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

	Local m_QuantumGate;
	auto m_Extender = std::make_shared<TestExtender::Extender>(nullptr);
	auto extp = std::static_pointer_cast<Extender>(m_Extender);

	if (!m_QuantumGate.AddExtender(extp)) return -1;

	// Allow access by default
	m_QuantumGate.GetAccessManager().SetPeerAccessDefault(Access::PeerAccessDefault::Allowed);

	// Allow all IP addresses to connect
	if (!m_QuantumGate.GetAccessManager().AddIPFilter(L"0.0.0.0/0", Access::IPFilterType::Allowed) ||
		!m_QuantumGate.GetAccessManager().AddIPFilter(L"::/0", Access::IPFilterType::Allowed))
	{
		LogErr(L"Failed to add an IP filter");
		return -1;
	}

	if (m_QuantumGate.Startup(params).Failed())
	{
		LogErr(L"Failed to start QuantumGate");
		return -1;
	}

	CmdConsole::DisplayPrompt();

	while (true)
	{
		if (CmdConsole::HasInputEvent())
		{
			if (CmdConsole::ProcessInputEvent() == CmdConsole::KeyInputEventResult::ReturnPressed)
			{
				std::wcout << L"\r\n";
				
				if (CmdConsole::GetCommandLine().size() > 0)
				{
					auto cmdline{ CmdConsole::GetCommandLine() };
					CmdConsole::ClearCommandLine();

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

								LogInfo(L"Disconnecting peer %llu", pluid);

								if (m_QuantumGate.DisconnectFrom(pluid, nullptr) == ResultCode::PeerNotFound)
								{
									LogErr(L"Could not disconnect peer; peer not found");
								}
							}
							else if (cmd.ID == L"connect")
							{
								LogInfo(L"Connecting to endpoint %s:%s", m[1].str().c_str(), m[2].str().c_str());

								wchar_t* end{ nullptr };
								const auto port = wcstoul(m[2].str().c_str(), &end, 10);

								if (!m_QuantumGate.ConnectTo({ IPEndpoint(IPAddress(m[1].str().c_str()), static_cast<UInt16>(port)) }))
								{
									LogErr(L"Failed to connect");
								}
							}
							else if (cmd.ID == L"seclevel")
							{
								wchar_t* end{ nullptr };
								const auto lvl = wcstoul(m[1].str().c_str(), &end, 10);
								const auto seclvl = static_cast<SecurityLevel>(lvl);

								if (!m_QuantumGate.SetSecurityLevel(seclvl))
								{
									LogErr(L"Failed to change security level");
								}
							}
							else if (cmd.ID == L"verbosity")
							{
								SetVerbosity(m[1].str());
							}
							else if (cmd.ID == L"quit")
							{
								if (!m_QuantumGate.Shutdown())
								{
									LogErr(L"QuantumGate shut down failed");
								}

								return 0;
							}
							else if (cmd.ID == L"send")
							{
								Send(m_Extender, m[1].str(), m[2].str(), m[3].str());
							}

							handled = true;

							break;
						}
					}

					if (!handled)
					{
						LogErr(L"Unrecognized command or bad syntax: %s", cmdline.c_str());
					}
				}

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

bool Send(const std::shared_ptr<TestExtender::Extender>& extender, const std::wstring& pluidstr,
		  const std::wstring& msg, const std::wstring& count)
{
	wchar_t* end{ nullptr };
	const PeerLUID pluid{ wcstoull(pluidstr.c_str(), &end, 10) };
	int nmess{ 1 };

	if (count.size() > 0) nmess = wcstoul(count.c_str(), &end, 10);

	LogInfo(L"Sending message '%s' to peer %llu, %d %s", msg.c_str(), pluid, nmess, ((nmess == 1) ? L"time" : L"times"));

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

		if (!extender->SendMessage(pluid, txt, SendParameters::PriorityOption::Normal, std::chrono::milliseconds(0)))
		{
			LogErr(L"Could not send message %d to peer", x);
			success = false;
			break;
		}
	}

	if (success)
	{
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);
		LogInfo(L"Sent in %d milliseconds", ms.count());
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
		LogSys(L"Console verbosity set to silent");
	}
	else if (verb == L"minimal")
	{
		set = true;
		verbosity = Console::Verbosity::Minimal;
		LogSys(L"Console verbosity set to minimal");
	}
	else if (verb == L"normal")
	{
		set = true;
		verbosity = Console::Verbosity::Normal;
		LogSys(L"Console verbosity set to normal");
	}
	else if (verb == L"verbose")
	{
		set = true;
		verbosity = Console::Verbosity::Verbose;
		LogSys(L"Console verbosity set to verbose");
	}
	else if (verb == L"debug")
	{
		set = true;
		verbosity = Console::Verbosity::Debug;
		LogSys(L"Console verbosity set to debug");
	}
	else LogErr(L"Unknown console verbosity level");

	if (set) Console::SetVerbosity(verbosity);

	return set;
}
