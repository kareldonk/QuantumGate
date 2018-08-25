// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CmdMess.h"

using namespace QuantumGate::Implementation;

struct Commands
{
	wstring ID;
	wstring RegEx;
};

vector<Commands> commands = {
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
	auto m_Extender = make_shared<TestExtender::Extender>(nullptr);
	auto extp = static_pointer_cast<Extender>(m_Extender);
	
	if (!m_QuantumGate.AddExtender(extp)) return - 1;

	m_QuantumGate.GetAccessManager().SetPeerAccessDefault(PeerAccessDefault::Allowed);

	if (m_QuantumGate.Startup(params).Succeeded())
	{
		while (true)
		{
			DisplayPrompt();

			wstring cmdline;
			getline(wcin, cmdline);

			if (cmdline.size() > 0)
			{
				bool handled = false;

				for (auto& cmd : commands)
				{
					wregex r(cmd.RegEx, regex_constants::icase);
					wsmatch m;
					if (regex_search(cmdline, m, r))
					{
						if (cmd.ID == L"disconnect")
						{
							wchar_t* end = nullptr;

							PeerLUID pluid = wcstoull(m[1].str().c_str(), &end, 10);

							LogInfo(L"Disconnecting peer %u", pluid);
							
							if (m_QuantumGate.DisconnectFrom(pluid, nullptr) == ResultCode::PeerNotFound)
							{
								LogErr(L"Could not disconnect peer; peer not found");
							}
						}
						else if (cmd.ID == L"connect")
						{
							LogInfo(L"Connecting to endpoint %s:%s", m[1].str().c_str(), m[2].str().c_str());

							wchar_t* end = nullptr;
							auto port = wcstoul(m[2].str().c_str(), &end, 10);

							if (!m_QuantumGate.ConnectTo({ IPEndpoint(m[1].str(), static_cast<UInt16>(port)) }))
							{
								LogErr(L"Failed to connect");
							}
						}
						else if (cmd.ID == L"seclevel")
						{
							wchar_t* end = nullptr;
							auto lvl = wcstoul(m[1].str().c_str(), &end, 10);
							auto seclvl = static_cast<QuantumGate::SecurityLevel>(lvl);

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
					LogErr(L"Unrecognized command or bad syntax");
				}
			}
		}
	}
	else
	{
		LogErr(L"Failed to start QuantumGate");
		return -1;
	}

    return 0;
}

bool Send(const shared_ptr<TestExtender::Extender>& extender, const wstring& pluidstr, const wstring& msg, const wstring& count)
{
	wchar_t* end = nullptr;
	PeerLUID pluid = wcstoull(pluidstr.c_str(), &end, 10);
	int nmess = 1;

	if (count.size() > 0) nmess = wcstoul(count.c_str(), &end, 10);

	LogInfo(L"Sending message '%s' to peer %u, %d %s", msg.c_str(), pluid, nmess, ((nmess == 1) ? L"time" : L"times"));

	auto success = true;
	auto begin = chrono::high_resolution_clock::now();

	for (int x = 0; x < nmess; x++)
	{
		String txt = msg;

		if (nmess > 1)
		{
			txt += L" " + Util::FormatString(L"#%d", x);
		}

		if (!extender->SendMessage(pluid, txt))
		{
			LogErr(L"Could not send message %d to peer", x);
			success = false;
			break;
		}
	}

	if (success)
	{
		auto ms = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - begin);
		LogInfo(L"Sent in %d milliseconds", ms.count());
	}

	return success;
}

bool SetVerbosity(const wstring& verb)
{
	auto set = false;
	auto verbosity = QuantumGate::Console::Verbosity::Debug;

	if (verb == L"silent")
	{
		set = true;
		verbosity = QuantumGate::Console::Verbosity::Silent;
		LogSys(L"Console verbosity set to silent");
	}
	else if (verb == L"minimal")
	{
		set = true;
		verbosity = QuantumGate::Console::Verbosity::Minimal;
		LogSys(L"Console verbosity set to minimal");
	}
	else if (verb == L"normal")
	{
		set = true;
		verbosity = QuantumGate::Console::Verbosity::Normal;
		LogSys(L"Console verbosity set to normal");
	}
	else if (verb == L"verbose")
	{
		set = true;
		verbosity = QuantumGate::Console::Verbosity::Verbose;
		LogSys(L"Console verbosity set to verbose");
	}
	else if (verb == L"debug")
	{
		set = true;
		verbosity = QuantumGate::Console::Verbosity::Debug;
		LogSys(L"Console verbosity set to debug");
	}
	else LogErr(L"Opened console verbosity level");

	if (set) QuantumGate::Console::SetVerbosity(verbosity);

	return set;
}

void DisplayPrompt()
{
	wcout << L"\x1b[106m\x1b[30m" << L" QuantumGate " << L"\uE0B1\x1b[49m\x1b[96m\uE0B0" << L"\x1b[39m";
}
