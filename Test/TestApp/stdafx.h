// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdlgs.h>

#include <afxdisp.h>        // MFC Automation classes



#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC support for Internet Explorer 4 Common Controls
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>     // MFC support for ribbons and control bars

#include <afxdialogex.h>

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

#include "resource.h"

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

#include <cassert>
#include <atomic>
#include <thread>
#include <fstream>

// Undefine conflicting macro from Windows SDK for gsl
#pragma push_macro("max")
#undef max
#include <gsl/gsl>
#pragma pop_macro("max")

#include <json.hpp>
#include <afxcontrolbars.h>
#include <afxcontrolbars.h>


