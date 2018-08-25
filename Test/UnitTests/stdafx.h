// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

// Headers for CppUnitTest
#include "CppUnitTest.h"

// Undefine conflicting macro from Windows SDK for gsl
#pragma push_macro("max")
#undef max
#include <gsl/gsl>
#pragma pop_macro("max")

#include "Common.h"