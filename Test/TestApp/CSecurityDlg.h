// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"
#include "CTabCtrlEx.h"
#include "CSecurityDlgGeneralTab.h"
#include "CSecurityDlgMessagesTab.h"
#include "CSecurityDlgNoiseTab.h"
#include "CSecurityDlgKeyUpdatesTab.h"
#include "CSecurityDlgUDPTab.h"
#include "CSecurityDlgRelaysTab.h"

using namespace QuantumGate;

class CSecurityDlg final : public CDialogBase
{
public:
	CSecurityDlg(CWnd* pParent = NULL);
	virtual ~CSecurityDlg();

	enum { IDD = IDD_SECURITY_SETTINGS };

	void SetQuantumGate(QuantumGate::Local* qg) noexcept { m_QuantumGate = qg; }

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

	DECLARE_MESSAGE_MAP()
	
	afx_msg void OnBnClickedOk();

private:
	bool InitializeTabCtrl() noexcept;

private:
	QuantumGate::Local* m_QuantumGate{ nullptr };
	QuantumGate::SecurityParameters m_SecurityParameters;

	CTabCtrlEx m_TabCtrl;
};

