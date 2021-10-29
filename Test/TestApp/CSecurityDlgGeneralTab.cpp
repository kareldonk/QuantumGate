// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestApp.h"
#include "CSecurityDlgGeneralTab.h"

IMPLEMENT_DYNCREATE(CSecurityDlgGeneralTab, CSecurityDlgTabCtrlPage)

CSecurityDlgGeneralTab::CSecurityDlgGeneralTab(CWnd* pParent /*=nullptr*/)
	: CSecurityDlgTabCtrlPage(IDD_SECURITY_SETTINGS_GENERAL_TAB, pParent)
{}

CSecurityDlgGeneralTab::~CSecurityDlgGeneralTab()
{}

void CSecurityDlgGeneralTab::DoDataExchange(CDataExchange* pDX)
{
	CSecurityDlgTabCtrlPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSecurityDlgGeneralTab, CSecurityDlgTabCtrlPage)
END_MESSAGE_MAP()

// CSecurityDlgGeneralTab message handlers

bool CSecurityDlgGeneralTab::LoadData() noexcept
{
	auto params = GetSecurityParameters();

	SetValue(IDC_COND_ACCEPT, params->General.UseConditionalAcceptFunction);

	SetValue(IDC_CONNECT_TIMEOUT, params->General.ConnectTimeout);
	SetValue(IDC_SUSPEND_TIMEOUT, params->General.SuspendTimeout);
	SetValue(IDC_MAX_SUSPEND_DURATION, params->General.MaxSuspendDuration);

	SetValue(IDC_HANDSHAKE_DELAY, params->General.MaxHandshakeDelay);
	SetValue(IDC_HANDSHAKE_DURATION, params->General.MaxHandshakeDuration);

	SetValue(IDC_IPREP_IMPROVE_INTERVAL, params->General.IPReputationImprovementInterval);

	SetValue(IDC_NUM_IPCON_ATTEMPTS, params->General.IPConnectionAttempts.MaxPerInterval);
	SetValue(IDC_IPCON_ATTEMPTS_INTERVAL, params->General.IPConnectionAttempts.Interval);

	return true;
}

bool CSecurityDlgGeneralTab::SaveData() noexcept
{
	auto params = GetSecurityParameters();

	params->General.UseConditionalAcceptFunction = (((CButton*)GetDlgItem(IDC_COND_ACCEPT))->GetCheck() == BST_CHECKED);
	params->General.ConnectTimeout = std::chrono::seconds(GetSizeValue(IDC_CONNECT_TIMEOUT));
	params->General.SuspendTimeout = std::chrono::seconds(GetSizeValue(IDC_SUSPEND_TIMEOUT));
	params->General.MaxSuspendDuration = std::chrono::seconds(GetSizeValue(IDC_MAX_SUSPEND_DURATION));
	params->General.MaxHandshakeDelay = std::chrono::milliseconds(GetSizeValue(IDC_HANDSHAKE_DELAY));
	params->General.MaxHandshakeDuration = std::chrono::seconds(GetSizeValue(IDC_HANDSHAKE_DURATION));
	params->General.IPReputationImprovementInterval = std::chrono::seconds(GetSizeValue(IDC_IPREP_IMPROVE_INTERVAL));
	params->General.IPConnectionAttempts.MaxPerInterval = GetSizeValue(IDC_NUM_IPCON_ATTEMPTS);
	params->General.IPConnectionAttempts.Interval = std::chrono::seconds(GetSizeValue(IDC_IPCON_ATTEMPTS_INTERVAL));

	return true;
}
