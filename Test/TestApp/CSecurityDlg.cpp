// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "CSecurityDlg.h"

CSecurityDlg::CSecurityDlg(CWnd* pParent) : CDialogBase(CSecurityDlg::IDD, pParent)
{}

CSecurityDlg::~CSecurityDlg()
{}

void CSecurityDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogBase::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB_CTRL, m_TabCtrl);
}

BEGIN_MESSAGE_MAP(CSecurityDlg, CDialogBase)
	ON_BN_CLICKED(IDOK, &CSecurityDlg::OnBnClickedOk)
END_MESSAGE_MAP()

BOOL CSecurityDlg::OnInitDialog()
{
	CDialogBase::OnInitDialog();

	assert(m_QuantumGate != nullptr);

	m_SecurityParameters = m_QuantumGate->GetSecurityParameters();

	if (!InitializeTabCtrl())
	{
		AfxMessageBox(L"Cannot open security settings; failed to create tab control.", MB_ICONERROR);
		EndDialog(IDCANCEL);
		return TRUE;
	}

	m_TabCtrl.LoadData();
	m_TabCtrl.UpdateControls();

	return TRUE;
}

bool CSecurityDlg::InitializeTabCtrl() noexcept
{
	// Tabpages
	if (m_TabCtrl.AddPage(RUNTIME_CLASS(CSecurityDlgGeneralTab), IDD_SECURITY_SETTINGS_GENERAL_TAB, L"General") &&
		m_TabCtrl.AddPage(RUNTIME_CLASS(CSecurityDlgMessagesTab), IDD_SECURITY_SETTINGS_MESSAGES_TAB, L"Messages") &&
		m_TabCtrl.AddPage(RUNTIME_CLASS(CSecurityDlgNoiseTab), IDD_SECURITY_SETTINGS_NOISE_TAB, L"Noise") &&
		m_TabCtrl.AddPage(RUNTIME_CLASS(CSecurityDlgKeyUpdatesTab), IDD_SECURITY_SETTINGS_KEYUPDATES_TAB, L"Key Updates") &&
		m_TabCtrl.AddPage(RUNTIME_CLASS(CSecurityDlgUDPTab), IDD_SECURITY_SETTINGS_UDP_TAB, L"UDP") &&
		m_TabCtrl.AddPage(RUNTIME_CLASS(CSecurityDlgRelaysTab), IDD_SECURITY_SETTINGS_RELAYS_TAB, L"Relays"))
	{
		if (m_TabCtrl.Initialize())
		{
			m_TabCtrl.ForEachTab([&](CTabCtrlPage* tab)
			{
				auto sectab = dynamic_cast<CSecurityDlgTabCtrlPage*>(tab);
				sectab->SetQuantumGateInstance(m_QuantumGate);
				sectab->SetSecurityParameters(&m_SecurityParameters);
			});

			return true;
		}
	}

	return false;
}

BOOL CSecurityDlg::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// Let tab pages handle commands first
	if (m_TabCtrl.ForwardOnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) return TRUE;

	return CDialogBase::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CSecurityDlg::OnBnClickedOk()
{
	if (!m_TabCtrl.SaveData()) return;

	if (m_QuantumGate->SetSecurityLevel(SecurityLevel::Custom, m_SecurityParameters).Failed())
	{
		AfxMessageBox(L"Could not set custom security level. Check the console output for details.", MB_ICONERROR);
		return;
	}

	CDialogBase::OnOK();
}
