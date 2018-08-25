// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

// Include and libraries for Windows Sockets
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ntdll.lib")

// QuantumGate Types
#include "..\Types.h"

// QuantumGate DLL functions
#include "..\dllmain.h"

// QuantumGate Local
#include "Local.h"

// QuantumGate Console
#include "Console.h"

namespace QuantumGate
{
	using API::AccessManager;
	using API::Local;
	using API::LocalEnvironment;
	using API::PeerEvent;
	using API::Extender;
}
