// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "TestApp.h"
#include "TestAppDlg.h"
#include "Common\Util.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace QuantumGate::Implementation;

BEGIN_MESSAGE_MAP(CTestAppApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

CTestAppApp::CTestAppApp()
{
	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// Place all significant initialization in InitInstance
}

// The one and only CTestAppApp object
CTestAppApp theApp;

CTestAppApp* GetApp() noexcept { return &theApp; }

BOOL CTestAppApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	AfxEnableControlContainer();

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager* pShellManager = new CShellManager;

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	SetRegistryKey(_T("QuantumGate"));

	CTestAppDlg dlg;
	m_pMainWnd = &dlg;
	const INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
	}
	else if (nResponse == IDCANCEL)
	{
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

	// Delete the shell manager created above.
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

std::optional<CString> CTestAppApp::BrowseForFile(HWND hwnd, const bool save) const noexcept
{
	wchar_t szFile[MAX_PATH]{ 0 };

	OPENFILENAME ofn{ 0 };

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = L"All\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;

	BOOL ret = false;

	if (save)
	{
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
		ret = GetSaveFileName(&ofn);
	}
	else
	{
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		ret = GetOpenFileName(&ofn);
	}

	if (ret == TRUE)
	{
		return { CString(szFile) };
	}

	return std::nullopt;
}

bool CTestAppApp::LoadKey(const String& path, ProtectedBuffer& key) const noexcept
{
	try
	{
		if (std::filesystem::exists(path))
		{
			const auto fsize = std::filesystem::file_size(path);

			ProtectedStringA b64keystr;
			b64keystr.resize(static_cast<size_t>(fsize));

			std::ifstream file(path, std::ios::in | std::ios::binary);
			file.read(b64keystr.data(), fsize);

			auto b64key = Util::FromBase64(b64keystr);
			if (b64key)
			{
				key = std::move(*b64key);
				return true;
			}
			else
			{
				AfxMessageBox(L"Couldn't convert the key from a base64 representation!", MB_ICONERROR);
			}
		}
		else
		{
			const auto msg = L"Couldn't load peer key from the file " + path + L"; the file does not exist.";
			AfxMessageBox(msg.c_str(), MB_ICONERROR);
			return false;
		}
	}
	catch (const std::exception& e)
	{
		const auto msg = L"Couldn't load key from the file " + path + L"; Exception: " + Util::ToStringW(e.what());
		AfxMessageBox(msg.c_str(), MB_ICONERROR);
	}

	return false;
}

bool CTestAppApp::SaveKey(const String& path, const ProtectedBuffer& key) const noexcept
{
	try
	{
		std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
		auto b64key = Util::ToBase64(key);
		if (b64key)
		{
			auto b64keystr = Util::ToProtectedStringA(*b64key);
			file.write(b64keystr.data(), b64keystr.size());
			return true;
		}
		else
		{
			AfxMessageBox(L"Couldn't convert the key into a base64 representation!", MB_ICONERROR);
		}
	}
	catch (const std::exception& e)
	{
		const auto msg = L"Couldn't save key to the file " + path + L"; Exception: " + Util::ToStringW(e.what());
		AfxMessageBox(msg.c_str(), MB_ICONERROR);
	}

	return false;
}

const std::wstring& CTestAppApp::GetFolder() noexcept
{
	if (m_AppFolder.empty())
	{
		// Get application folder
		wchar_t pModuleFile[MAX_PATH]{ 0 };
		const auto dwSize = GetModuleFileName(NULL, pModuleFile, MAX_PATH);
		if (dwSize > 0)
		{
			m_AppFolder = pModuleFile;

			const auto pos = m_AppFolder.rfind(L"\\");
			if (pos != std::wstring::npos)
			{
				m_AppFolder = m_AppFolder.substr(0, pos + 1);
			}
		}
	}

	return m_AppFolder;
}

int CTestAppApp::GetScaledWidth(const int width) const noexcept
{
	auto dc = GetDC(0);
	auto dpix = GetDeviceCaps(dc, LOGPIXELSX);
	ReleaseDC(0, dc);

	return static_cast<int>((static_cast<double>(width) / 96.0) * static_cast<double>(dpix));
}

int CTestAppApp::GetScaledHeight(const int height) const noexcept
{
	auto dc = GetDC(0);
	auto dpiy = GetDeviceCaps(dc, LOGPIXELSY);
	ReleaseDC(0, dc);

	return static_cast<int>((static_cast<double>(height) / 96.0) * static_cast<double>(dpiy));
}