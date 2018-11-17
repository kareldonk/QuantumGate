// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

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

	DECLARE_MESSAGE_MAP()
	
	afx_msg void OnBnClickedOk();

private:
	QuantumGate::Local* m_QuantumGate{ nullptr };
};

