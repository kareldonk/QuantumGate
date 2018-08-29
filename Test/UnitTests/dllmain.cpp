// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) noexcept
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			QuantumGate::InitQuantumGateModule();
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			QuantumGate::DeinitQuantumGateModule();
			break;
	}

	return TRUE;
}
