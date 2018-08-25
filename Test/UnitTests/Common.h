// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "QuantumGate.h"

#if defined(_DEBUG)
#if !defined(_WIN64)
#pragma comment (lib, "QuantumGate32SD.lib")
#else
#pragma comment (lib, "QuantumGate64SD.lib")
#endif
#else
#if !defined(_WIN64)
#pragma comment (lib, "QuantumGate32S.lib")
#else
#pragma comment (lib, "QuantumGate64S.lib")
#endif
#endif

using namespace QuantumGate;
using namespace QuantumGate::Implementation;
