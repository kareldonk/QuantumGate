// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <iostream>
#include <regex>

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

// Undefine conflicting macro from Windows SDK for gsl
#pragma push_macro("max")
#undef max
#include <gsl/gsl>
#pragma pop_macro("max")