// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "CSocks5ExtenderConfigurationDlg.h"

IMPLEMENT_DYNAMIC(CSocks5ExtenderConfigurationDlg, CDialogBase)

CSocks5ExtenderConfigurationDlg::CSocks5ExtenderConfigurationDlg(CWnd* pParent)
	: CDialogBase(IDD_SOCKS5EXTENDER_CONFIG, pParent)
{}

CSocks5ExtenderConfigurationDlg::~CSocks5ExtenderConfigurationDlg()
{}

void CSocks5ExtenderConfigurationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogBase::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSocks5ExtenderConfigurationDlg, CDialogBase)
	ON_BN_CLICKED(IDOK, &CSocks5ExtenderConfigurationDlg::OnBnClickedOk)
END_MESSAGE_MAP()

BOOL CSocks5ExtenderConfigurationDlg::OnInitDialog()
{
	CDialogBase::OnInitDialog();

	SetValue(IDC_TCP_PORT, m_TCPPort);

	return TRUE;
}

void CSocks5ExtenderConfigurationDlg::OnBnClickedOk()
{
	const auto port = GetUInt64Value(IDC_TCP_PORT);
	if (port <= 65535)
	{
		m_TCPPort = static_cast<UInt16>(port);
	}
	else
	{
		AfxMessageBox(L"The TCP port should be in the range between 0 - 65535.", MB_ICONERROR);
		return;
	}

	CDialogBase::OnOK();
}
