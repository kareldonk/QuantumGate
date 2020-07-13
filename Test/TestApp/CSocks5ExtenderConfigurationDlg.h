// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

class CSocks5ExtenderConfigurationDlg : public CDialogBase
{
	DECLARE_DYNAMIC(CSocks5ExtenderConfigurationDlg)

public:
	CSocks5ExtenderConfigurationDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CSocks5ExtenderConfigurationDlg();

	inline void SetTCPPort(const UInt16 port) noexcept { m_TCPPort = port; }
	inline UInt16 GetTCPPort() const noexcept { return m_TCPPort; }

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SOCKS5EXTENDER_CONFIG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();

private:
	UInt16 m_TCPPort{ 9090 };
};
