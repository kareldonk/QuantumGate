// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#define INCLUDE_AVEXTENDER

#include "resource.h"		// main symbols
#include "QuantumGate.h"

using namespace QuantumGate;

class CTestAppApp final : public CWinApp
{
public:
	CTestAppApp();

	String GetAppVersion() noexcept;
	const String& GetFolder() noexcept;

	std::optional<CString> BrowseForFile(HWND hwnd, const bool save) const noexcept;

	bool LoadKey(const String& path, ProtectedBuffer& key) const noexcept;
	bool SaveKey(const String& path, const ProtectedBuffer& key) const noexcept;

	int GetScaledWidth(const int width) const noexcept;
	int GetScaledHeight(const int height) const noexcept;

private:
	String GetVersionInfo(const WChar* module_name, const WChar* value) const noexcept;

	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()

private:
	String m_AppFolder;
};

CTestAppApp* GetApp() noexcept;

extern CTestAppApp theApp;