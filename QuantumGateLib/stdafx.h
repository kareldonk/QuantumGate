// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#define SECURITY_WIN32
#include <security.h>
#pragma comment(lib, "secur32.lib")

#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ntdll.lib")

#if defined(_DEBUG)
	#if !defined(_WIN64)
	#pragma comment(lib, "zlib32.lib")
	#pragma comment(lib, "libzstd32.lib")
	#pragma comment(lib, "libcrypto32d.lib")
	#pragma comment(lib, "QuantumGateCryptoLib32D.lib")
	#else
	#pragma comment(lib, "zlib64.lib")
	#pragma comment(lib, "libzstd64.lib")
	#pragma comment(lib, "libcrypto64d.lib")
	#pragma comment(lib, "QuantumGateCryptoLib64D.lib")
	#endif
#else
	#if !defined(_WIN64)
	#pragma comment(lib, "zlib32.lib")
	#pragma comment(lib, "libzstd32.lib")
	#pragma comment(lib, "libcrypto32.lib")
	#pragma comment(lib, "QuantumGateCryptoLib32.lib")
	#else
	#pragma comment(lib, "zlib64.lib")
	#pragma comment(lib, "libzstd64.lib")
	#pragma comment(lib, "libcrypto64.lib")
	#pragma comment(lib, "QuantumGateCryptoLib64.lib")
	#endif
#endif

// Undefine conflicting macro from Windows SDK for gsl
#pragma push_macro("max")
#undef max
#include <gsl/gsl>
#pragma pop_macro("max")

#include "Types.h"
#include "Common\Util.h"
#include "Common\Console.h"
#include "Settings.h"