// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

class CAuthenticationDlg : public CDialogBase
{
public:
	CAuthenticationDlg(CWnd* pParent = NULL);
	virtual ~CAuthenticationDlg();

	enum { IDD = IDD_AUTHENTICATION_DLG };

	inline void SetCredentials(const CString& usr, const CString& pwd) noexcept
	{
		m_Username = usr;
		m_Password = pwd;
	}

	inline const CString& GetUsername() const noexcept { return m_Username; }
	inline const CString& GetPassword() const noexcept { return m_Password; }

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();

private:
	CString m_Username;
	CString m_Password;
};

