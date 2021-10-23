// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CSecurityDlgTabCtrlPage.h"

class CSecurityDlgRelaysTab : public CSecurityDlgTabCtrlPage
{
	DECLARE_DYNCREATE(CSecurityDlgRelaysTab)

public:
	CSecurityDlgRelaysTab(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CSecurityDlgRelaysTab();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SECURITY_SETTINGS_RELAYS_TAB };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

private:
	bool LoadData() noexcept override;
	bool SaveData() noexcept override;
};
