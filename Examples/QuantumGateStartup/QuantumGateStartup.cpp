// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"

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
#include <thread>
#include <chrono>

using namespace std::literals;

int main()
{
	// Send console output to terminal;
	// remove this block if console output isn't needed
	{
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

	// Listen for incoming connections on startup
	params.Listeners.Enable = true;

	// Listen for incoming connections on these ports
	params.Listeners.TCPPorts = { 999, 9999 };

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

	// For testing purposes we allow all IP addresses to connect;
	// by default all IP Addresses are blocked
	if (!qg.GetAccessManager().AddIPFilter(L"0.0.0.0/0", QuantumGate::Access::IPFilterType::Allowed) ||
		!qg.GetAccessManager().AddIPFilter(L"::/0", QuantumGate::Access::IPFilterType::Allowed))
	{
		std::wcout << L"Failed to add an IP filter\r\n";
		return -1;
	}

	// Add any extenders here
	// qg.AddExtender
	// or
	// qg.AddExtenderModule

	const auto result = qg.Startup(params);
	if (result.Succeeded())
	{
		std::wcout << L"Startup successful\r\nPress Enter to shut down";

		std::wcin.ignore();

		if (!qg.Shutdown())
		{
			std::wcout << L"Shutdown failed\r\n";
		}
	}
	else
	{
		std::wcout << L"Startup failed (" << result << L")\r\n";

		return -1;
	}

	return 0;
}