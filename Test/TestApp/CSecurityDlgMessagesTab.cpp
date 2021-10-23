// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestApp.h"
#include "CSecurityDlgMessagesTab.h"

IMPLEMENT_DYNCREATE(CSecurityDlgMessagesTab, CSecurityDlgTabCtrlPage)

CSecurityDlgMessagesTab::CSecurityDlgMessagesTab(CWnd* pParent /*=nullptr*/)
	: CSecurityDlgTabCtrlPage(IDD_SECURITY_SETTINGS_MESSAGES_TAB, pParent)
{}

CSecurityDlgMessagesTab::~CSecurityDlgMessagesTab()
{}

void CSecurityDlgMessagesTab::DoDataExchange(CDataExchange* pDX)
{
	CSecurityDlgTabCtrlPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSecurityDlgMessagesTab, CSecurityDlgTabCtrlPage)
END_MESSAGE_MAP()

// CSecurityDlgMessagesTab message handlers

bool CSecurityDlgMessagesTab::LoadData() noexcept
{
	auto params = GetSecurityParameters();

	SetValue(IDC_MESSAGE_AGE_TOLERANCE, params->Message.AgeTolerance);
	SetValue(IDC_EXTENDER_GRACE_PERIOD, params->Message.ExtenderGracePeriod);
	SetValue(IDC_MSG_RND_PREFIX_MIN, params->Message.MinRandomDataPrefixSize);
	SetValue(IDC_MSG_RND_PREFIX_MAX, params->Message.MaxRandomDataPrefixSize);
	SetValue(IDC_MSG_RND_MIN, params->Message.MinInternalRandomDataSize);
	SetValue(IDC_MSG_RND_MAX, params->Message.MaxInternalRandomDataSize);

	return true;
}

bool CSecurityDlgMessagesTab::SaveData() noexcept
{
	auto params = GetSecurityParameters();

	params->Message.AgeTolerance = std::chrono::seconds(GetSizeValue(IDC_MESSAGE_AGE_TOLERANCE));
	params->Message.ExtenderGracePeriod = std::chrono::seconds(GetSizeValue(IDC_EXTENDER_GRACE_PERIOD));
	params->Message.MinRandomDataPrefixSize = GetSizeValue(IDC_MSG_RND_PREFIX_MIN);
	params->Message.MaxRandomDataPrefixSize = GetSizeValue(IDC_MSG_RND_PREFIX_MAX);
	params->Message.MinInternalRandomDataSize = GetSizeValue(IDC_MSG_RND_MIN);
	params->Message.MaxInternalRandomDataSize = GetSizeValue(IDC_MSG_RND_MAX);

	return true;
}