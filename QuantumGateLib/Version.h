// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#define STR1(x)  #x
#define STR(x)  STR1(x)

#define VERSION_MAJOR             0
#define VERSION_MINOR             1
#define VERSION_REVISION          0
#define VERSION_BUILD             740

#define FILE_VERSION			VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_BUILD
#define FILE_VERSION_STR		STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "."  STR(VERSION_REVISION) "." STR(VERSION_BUILD)
#define PRODUCT_VERSION         FILE_VERSION
#define PRODUCT_VERSION_STR     FILE_VERSION_STR


