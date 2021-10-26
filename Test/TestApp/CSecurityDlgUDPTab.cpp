// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestApp.h"
#include "CSecurityDlgUDPTab.h"

IMPLEMENT_DYNCREATE(CSecurityDlgUDPTab, CSecurityDlgTabCtrlPage)

CSecurityDlgUDPTab::CSecurityDlgUDPTab(CWnd* pParent /*=nullptr*/)
	: CSecurityDlgTabCtrlPage(IDD_SECURITY_SETTINGS_UDP_TAB, pParent)
{}

CSecurityDlgUDPTab::~CSecurityDlgUDPTab()
{}

void CSecurityDlgUDPTab::DoDataExchange(CDataExchange* pDX)
{
	CSecurityDlgTabCtrlPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSecurityDlgUDPTab, CSecurityDlgTabCtrlPage)
END_MESSAGE_MAP()

// CSecurityDlgUDPTab message handlers

bool CSecurityDlgUDPTab::LoadData() noexcept
{
	auto params = GetSecurityParameters();

	SetValue(IDC_UDP_COOKIE_THRESHOLD, params->UDP.ConnectCookieRequirementThreshold);
	SetValue(IDC_UDP_COOKIE_INTERVAL, params->UDP.CookieExpirationInterval);
	SetValue(IDC_UDP_NUM_DECOY_MESSAGES, params->UDP.MaxNumDecoyMessages);
	SetValue(IDC_UDP_DECOY_MESSAGES_INTERVAL, params->UDP.MaxDecoyMessageInterval);
	SetValue(IDC_UDP_MTU_DELAY, params->UDP.MaxMTUDiscoveryDelay);

	return true;
}

bool CSecurityDlgUDPTab::SaveData() noexcept
{
	auto params = GetSecurityParameters();

	params->UDP.ConnectCookieRequirementThreshold = GetSizeValue(IDC_UDP_COOKIE_THRESHOLD);
	params->UDP.CookieExpirationInterval = std::chrono::seconds(GetSizeValue(IDC_UDP_COOKIE_INTERVAL));
	params->UDP.MaxNumDecoyMessages = GetSizeValue(IDC_UDP_NUM_DECOY_MESSAGES);
	params->UDP.MaxDecoyMessageInterval = std::chrono::milliseconds(GetSizeValue(IDC_UDP_DECOY_MESSAGES_INTERVAL));
	params->UDP.MaxMTUDiscoveryDelay = std::chrono::milliseconds(GetSizeValue(IDC_UDP_MTU_DELAY));

	return true;
}