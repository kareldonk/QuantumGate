// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestApp.h"
#include "CSecurityDlgRelaysTab.h"

IMPLEMENT_DYNCREATE(CSecurityDlgRelaysTab, CSecurityDlgTabCtrlPage)

CSecurityDlgRelaysTab::CSecurityDlgRelaysTab(CWnd* pParent /*=nullptr*/)
	: CSecurityDlgTabCtrlPage(IDD_SECURITY_SETTINGS_RELAYS_TAB, pParent)
{}

CSecurityDlgRelaysTab::~CSecurityDlgRelaysTab()
{}

void CSecurityDlgRelaysTab::DoDataExchange(CDataExchange* pDX)
{
	CSecurityDlgTabCtrlPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSecurityDlgRelaysTab, CSecurityDlgTabCtrlPage)
END_MESSAGE_MAP()

// CSecurityDlgRelaysTab message handlers

bool CSecurityDlgRelaysTab::LoadData() noexcept
{
	auto params = GetSecurityParameters();

	SetValue(IDC_RELAY_CONNECT_TIMEOUT, params->Relay.ConnectTimeout);
	SetValue(IDC_RELAY_GRACEPERIOD, params->Relay.GracePeriod);
	SetValue(IDC_RELAY_MAX_SUSPEND_DURATION, params->Relay.MaxSuspendDuration);
	SetValue(IDC_RELAY_NUM_IPCON_ATTEMPTS, params->Relay.ConnectionAttempts.MaxPerInterval);
	SetValue(IDC_RELAY_IPCON_ATTEMPTS_INTERVAL, params->Relay.ConnectionAttempts.Interval);

	return true;
}

bool CSecurityDlgRelaysTab::SaveData() noexcept
{
	auto params = GetSecurityParameters();

	params->Relay.ConnectTimeout = std::chrono::seconds(GetSizeValue(IDC_RELAY_CONNECT_TIMEOUT));
	params->Relay.GracePeriod = std::chrono::seconds(GetSizeValue(IDC_RELAY_GRACEPERIOD));
	params->Relay.MaxSuspendDuration = std::chrono::seconds(GetSizeValue(IDC_RELAY_MAX_SUSPEND_DURATION));
	params->Relay.ConnectionAttempts.MaxPerInterval = GetSizeValue(IDC_RELAY_NUM_IPCON_ATTEMPTS);
	params->Relay.ConnectionAttempts.Interval = std::chrono::seconds(GetSizeValue(IDC_RELAY_IPCON_ATTEMPTS_INTERVAL));

	return true;
}