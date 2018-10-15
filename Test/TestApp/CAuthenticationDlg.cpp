// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CAuthenticationDlg.h"

CAuthenticationDlg::CAuthenticationDlg(CWnd* pParent) : CDialogBase(CAuthenticationDlg::IDD, pParent)
{}


CAuthenticationDlg::~CAuthenticationDlg()
{}

void CAuthenticationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogBase::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAuthenticationDlg, CDialogBase)
	ON_BN_CLICKED(IDOK, &CAuthenticationDlg::OnBnClickedOk)
END_MESSAGE_MAP()

BOOL CAuthenticationDlg::OnInitDialog()
{
	CDialogBase::OnInitDialog();

	SetValue(IDC_USERNAME, m_Username);
	SetValue(IDC_PASSWORD, m_Password);

	return TRUE;
}

void CAuthenticationDlg::OnBnClickedOk()
{
	m_Username = GetTextValue(IDC_USERNAME);
	m_Password = GetTextValue(IDC_PASSWORD);

	CDialogBase::OnOK();
}