// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

// Include the QuantumGate main header with API definitions
#include <QuantumGate.h>

// Link with the QuantumGate library depending on architecture
#if defined(_DEBUG)
#if !defined(_WIN64)
#pragma comment (lib, "QuantumGate32D.lib")
#else
#pragma comment (lib, "QuantumGate64D.lib")
#endif
#else
#if !defined(_WIN64)
#pragma comment (lib, "QuantumGate32.lib")
#else
#pragma comment (lib, "QuantumGate64.lib")
#endif
#endif

#include <iostream>
#include <regex>

#include "BluetoothMessengerExtender.h"

struct Command final
{
	enum class ID { Scan, Connect, Disconnect, Send, Help, Quit };

	ID ID;
	std::wstring Name;
	std::wstring RegEx;
	std::wstring Usage;
	std::wstring Example;
};

std::array commands = {
	Command{
		Command::ID::Scan,
		L"scan",
		L"^scan\\s?$",
		L"scan"
	},
	Command{
		Command::ID::Connect,
		L"connect",
		L"^connect\\s+(\\([a-f0-9:]*\\)):?(?:(\\d+)|(\\{[a-f0-9-]*\\}))?$",
		L"connect [Bluetooth Address]:([Port] or [ServiceClassID])",
		L"connect (D3:A5:D3:FA:15:33):9"
	},
	Command{
		Command::ID::Disconnect,
		L"disconnect",
		L"^disconnect\\s+([^\\s]+)$",
		L"disconnect [Peer LUID]"
	},
	Command{
		Command::ID::Send,
		L"send",
		L"^send\\s+([0-9]+)\\s+\"(.+)\"\\s*(\\d*)$",
		L"send [Peer LUID] \"[Message]\" [Number of times]",
		L"send 12 \"Hello peer, how are you?\" 1"
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

void DisplayHelp() noexcept
{
	std::wcout << L"\r\nSupported commands:\r\n\r\n";

	size_t maxlen{ 0 };
	for (const auto& command : commands)
	{
		if (command.Name.size() > maxlen) maxlen = command.Name.size();
	}

	for (const auto& command : commands)
	{
		std::wcout << L"\t";
		std::wcout << L"\x1b[93m";
		std::wcout.width(maxlen);
		std::wcout << command.Name;
		std::wcout << L"\x1b[39m";
		std::wcout << L" - Usage: " << command.Usage << L"\r\n";

		if (!command.Example.empty())
		{
			std::wcout << L"\r\n\t";
			std::wcout.width(maxlen);
			std::wcout << L" ";
			std::wcout << L"          e.g. " << command.Example << L"\r\n\r\n";
		}
		else std::wcout << L"\r\n";
	}

	std::wcout << L"\r\n";
}

std::wstring GetInput()
{
	std::wstring input;
	std::getline(std::wcin, input);
	return input;
}

void ScanForDevices(QuantumGate::Local& qg) noexcept
{
	std::wcout << L"Looking for Bluetooth devices, please wait...\r\n";

	// Note that we pass 'true' to GetEnvironment() to
	// update cached information and scan for changes,
	// in this case possibly new Bluetooth devices in range
	const auto result = qg.GetEnvironment(true).GetBluetoothDevices();
	if (result.Failed())
	{
		std::wcout << L"Failed to look for Bluetooth devices (" << result << L")\r\n";
		return;
	}

	const auto& devices = result.GetValue();
	if (devices.empty())
	{
		std::wcout << L"No Bluetooth devices were found.\r\n";
		return;
	}

	std::wcout << L"Found Bluetooth devices:\r\n";

	for (const auto& device : devices)
	{
		std::wcout << L"\r\nDevice Name:\t\x1b[93m" << device.Name << L"\x1b[39m\r\n";

		std::wcout << L"Remote Address:\t\x1b[97m" << device.RemoteAddress.GetString() << L"\x1b[39m\r\n";

		std::wcout << L"Connected:\t";
		if (device.Connected)
		{
			std::wcout << L"Yes\r\n";
		}
		else
		{
			std::wcout << L"No\r\n";
		}

		std::wcout << L"Authenticated:\t";

		if (device.Authenticated)
		{
			std::wcout << L"Yes\r\n";
		}
		else
		{
			std::wcout << L"No\r\n";
		}

		std::wcout << L"Remembered:\t";

		if (device.Remembered)
		{
			std::wcout << L"Yes\r\n";
		}
		else
		{
			std::wcout << L"No\r\n";
		}
	}

	std::wcout << L"\r\n";
}

bool HandleCommand(QuantumGate::Local& qg, const BluetoothMessengerExtender& ext, const std::wstring& cmdline)
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
				switch (cmd.ID)
				{
					case Command::ID::Scan:
					{
						ScanForDevices(qg);
						break;
					}
					case Command::ID::Connect:
					{
						QuantumGate::BTHAddress addr;
						if (QuantumGate::BTHAddress::TryParse(m[1].str().c_str(), addr))
						{
							std::optional<QuantumGate::BTHEndpoint> endp;

							if (m[2].matched)
							{
								wchar_t* end{ nullptr };
								const auto port = static_cast<QuantumGate::UInt16>(std::wcstoul(m[2].str().c_str(), &end, 10));
								
								// Connect to a specific port
								endp = QuantumGate::BTHEndpoint(QuantumGate::BTHEndpoint::Protocol::RFCOMM, addr, port);
							}
							else if (m[3].matched)
							{
								GUID scid{ 0 };
								if (::IIDFromString(m[3].str().c_str(), &scid) == S_OK)
								{
									// Connect to a specific service class ID
									endp = QuantumGate::BTHEndpoint(QuantumGate::BTHEndpoint::Protocol::RFCOMM, addr, scid);
								}
								else
								{
									std::wcout << L"Invalid service class ID specified\r\n";
								}
							}
							else
							{
								// No port specified so we try to connect using the default QuantumGate service class ID
								// and leave it up to the OS to find the associated port via Bluetooth service advertising
								endp = QuantumGate::BTHEndpoint(QuantumGate::BTHEndpoint::Protocol::RFCOMM, addr,
																QuantumGate::BTHEndpoint::GetQuantumGateServiceClassID());
							}

							if (endp.has_value())
							{
								QuantumGate::ConnectParameters params;
								params.PeerEndpoint = *endp;
								
								// Don't require Bluetooth authentication (device pairing) for outgoing connections
								params.Bluetooth.RequireAuthentication = false;

								std::wcout << L"Connecting to endpoint " << endp->GetString() << L"...\r\n";

								const auto result = qg.ConnectTo(std::move(params));
								if (result.Failed())
								{
									std::wcout << L"Failed to connect to endpoint " << endp->GetString() << L" (" <<
										result.GetErrorDescription() << L")\r\n";
								}
							}
						}
						else
						{
							std::wcout << L"Invalid Bluetooth address specified.\r\n";
						}

						break;
					}
					case Command::ID::Disconnect:
					{
						wchar_t* end{ nullptr };
						const QuantumGate::PeerLUID pluid = std::wcstoull(m[1].str().c_str(), &end, 10);

						std::wcout << L"Disconnecting peer " << pluid << L"...\r\n";

						const auto result = qg.DisconnectFrom(pluid);
						if (result.Failed())
						{
							std::wcout << L"Could not disconnect peer " << pluid << L" (" << result.GetErrorDescription() << L")\r\n";
						}

						break;
					}
					case Command::ID::Send:
					{
						wchar_t* end{ nullptr };
						const QuantumGate::PeerLUID pluid{ std::wcstoull(m[1].str().c_str(), &end, 10)};

						const auto msg = m[2].str();

						int num_times{ 1 };
						const auto num_times_str = m[3].str();
						if (!num_times_str.empty())
						{
							num_times = std::wcstoul(num_times_str.c_str(), &end, 10);
						}

						if (num_times > 0)
						{
							std::wcout << L"Sending message '" << msg << L"' to peer " << pluid << L", " << num_times;
							if (num_times == 1)
							{
								std::wcout << L" time...\r\n";
							}
							else
							{
								std::wcout << L" times...\r\n";
							}

							ext.SendMessage(pluid, msg, num_times);
						}

						break;
					}
					case Command::ID::Help:
					{
						DisplayHelp();
						break;
					}
					case Command::ID::Quit:
					{
						std::wcout << L"Shutting down QuantumGate, please wait...\r\n";

						if (const auto result = qg.Shutdown(); result.Succeeded())
						{
							std::wcout << L"\r\nQuantumGate shut down successful\r\n";
						}
						else
						{
							std::wcout << L"QuantumGate shut down failed (" << result.GetErrorDescription() << L")";
						}

						std::wcout << L"\r\nBye...\r\n\r\n";

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
			std::wcout << L"\r\nUnrecognized command or bad syntax: " << cmdline << L"\r\n";
			std::wcout << L"Type 'help' or '?' and press Enter for help.\r\n\r\n";
		}
	}

	return true;
}

int main()
{
	auto enable_console = false;

	std::wcout << L"Would you like to enable QuantumGate console output? (Y/N): ";
	auto answer = GetInput();
	if (answer.size() > 0)
	{
		if (answer == L"y" || answer == L"Y")
		{
			enable_console = true;
		}
	}

	if (enable_console)
	{
		// Send console output to terminal;
		QuantumGate::Console::SetOutput(std::make_shared<QuantumGate::Console::TerminalOutput>());
		QuantumGate::Console::SetVerbosity(QuantumGate::Console::Verbosity::Debug);
	}

	QuantumGate::StartupParameters params;

	// Create a UUID for the local instance with matching keypair;
	// normally you should do this once and save and reload the UUID
	// and keys. The UUID and public key can be shared with other peers,
	// while the private key should be protected and kept private.
	{
		auto [success, uuid, keys] = QuantumGate::UUID::Create(QuantumGate::UUID::Type::Peer,
															   QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519);
		if (success)
		{
			params.UUID = uuid;
			params.Keys = std::move(*keys);
		}
		else
		{
			std::wcout << L"Failed to create peer UUID\r\n";
			return -1;
		}
	}

	// Set the supported algorithms
	params.SupportedAlgorithms.Hash = {
		QuantumGate::Algorithm::Hash::BLAKE2B512
	};
	params.SupportedAlgorithms.PrimaryAsymmetric = {
		QuantumGate::Algorithm::Asymmetric::ECDH_X25519
	};
	params.SupportedAlgorithms.SecondaryAsymmetric = {
		QuantumGate::Algorithm::Asymmetric::KEM_NTRUPRIME
	};
	params.SupportedAlgorithms.Symmetric = {
		QuantumGate::Algorithm::Symmetric::CHACHA20_POLY1305
	};
	params.SupportedAlgorithms.Compression = {
		QuantumGate::Algorithm::Compression::ZSTANDARD
	};

	// Listen for incoming Bluetooth connections on startup
	params.Listeners.BTH.Enable = true;

	// Listen for incoming connections on this port
	params.Listeners.BTH.Ports = { 9 };

	// Be discoverable for other devices while listening for incoming connections
	params.Listeners.BTH.Discoverable = true;

	// Don't require Bluetooth authentication (device pairing) for incoming connections
	params.Listeners.BTH.RequireAuthentication = false;

	// Start extenders on startup
	params.EnableExtenders = true;

	// For testing purposes we disable authentication requirement; when
	// authentication is required we would need to add peers to the instance
	// via QuantumGate::Local::GetAccessManager().AddPeer() including their
	// UUID and public key so that they can be authenticated when connecting
	params.RequireAuthentication = false;

	// Our local instance object
	QuantumGate::Local qg;

	// For testing purposes we allow access by default
	qg.GetAccessManager().SetPeerAccessDefault(QuantumGate::Access::PeerAccessDefault::Allowed);

	// The following is just for convenience to display the local Bluetooth address(es)
	// so that we know how to connect to this device and to make sure we didn't forget 
	// to enable Bluetooth
	for (auto retry = 0u; retry < 3u; ++retry)
	{
		const auto result1 = qg.GetEnvironment().GetBluetoothRadios();
		if (result1.Succeeded())
		{
			const auto& radios = result1.GetValue();
			if (!radios.empty())
			{
				std::wcout << L"\r\nLocal Bluetooth addresses are:\r\n";

				for (const auto& radio : radios)
				{
					std::wcout << L"- " << radio.Address.GetString() << L"\r\n";
				}

				std::wcout << L"\r\nQuantumGate will listen for incoming connections on the following local endpoints:\r\n";

				for (const auto& radio : radios)
				{
					for (const auto port : params.Listeners.BTH.Ports)
					{
						const auto local_endpoint_port = QuantumGate::BTHEndpoint(QuantumGate::BTHEndpoint::Protocol::RFCOMM,
																				  radio.Address, port);
						const auto local_endpoint_sci = QuantumGate::BTHEndpoint(QuantumGate::BTHEndpoint::Protocol::RFCOMM,
																				 radio.Address,
																				 QuantumGate::BTHEndpoint::GetQuantumGateServiceClassID());
						std::wcout << L"- " << local_endpoint_port.GetString() << L"\r\n";
						std::wcout << L"- " << local_endpoint_sci.GetString() << L"\r\n";
					}
				}

				std::wcout << L"\r\n";

				break;
			}
			else
			{
				std::wcout << L"\r\nNo Bluetooth radios were found on the local system. "
					"Make sure Bluetooth is enabled.\r\nPress Enter to continue...\r\n";

				std::wcin.ignore();
			}
		}
	}

	// Add our custom Bluetooth Messenger Extender so that we can send messages to peers;
	// this is not required if we would just connect to peers and do nothing
	auto extender = std::make_shared<BluetoothMessengerExtender>();
	if (const auto result = qg.AddExtender(extender); result.Failed())
	{
		std::wcout << L"Failed to add Bluetooth Messenger Extender (" << result << L")\r\n";
		return -1;
	}

	if (!enable_console)
	{
		std::wcout << L"\r\nStarting QuantumGate...\r\n";
	}

	const auto result2 = qg.Startup(params);
	if (result2.Succeeded())
	{
		if (!enable_console)
		{
			std::wcout << L"QuantumGate startup successful\r\n\r\n";
		}

		// Let user know what's possible
		DisplayHelp();

		std::wcout << L"\x1b[93m" << L"\r\n";
		std::wcout << L"Make sure the Bluetooth sample is also running on the other device(s)\r\n"
			"and then type 'scan' and press Enter to look for nearby Bluetooth devices.\r\n";
		std::wcout << L"\x1b[39m" << L"\r\n";

		while (true)
		{
			std::wcout << L"\x1b[106m\x1b[30m" << L" >> " << L"\x1b[40m\x1b[39m ";

			const auto cmdline = GetInput();
			if (!HandleCommand(qg, *extender, cmdline)) break;
		}
	}
	else
	{
		std::wcout << L"Startup failed (" << result2 << L")\r\n";
		return -1;
	}

	return 0;
}