// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestApp.h"
#include "CSecurityDlgKeyUpdatesTab.h"

IMPLEMENT_DYNCREATE(CSecurityDlgKeyUpdatesTab, CSecurityDlgTabCtrlPage)

CSecurityDlgKeyUpdatesTab::CSecurityDlgKeyUpdatesTab(CWnd* pParent /*=nullptr*/)
	: CSecurityDlgTabCtrlPage(IDD_SECURITY_SETTINGS_KEYUPDATES_TAB, pParent)
{}

CSecurityDlgKeyUpdatesTab::~CSecurityDlgKeyUpdatesTab()
{}

void CSecurityDlgKeyUpdatesTab::DoDataExchange(CDataExchange* pDX)
{
	CSecurityDlgTabCtrlPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSecurityDlgKeyUpdatesTab, CSecurityDlgTabCtrlPage)
END_MESSAGE_MAP()

// CSecurityDlgKeyUpdatesTab message handlers

bool CSecurityDlgKeyUpdatesTab::LoadData() noexcept
{
	auto params = GetSecurityParameters();

	SetValue(IDC_KEYUPDATE_MINSECS, params->KeyUpdate.MinInterval);
	SetValue(IDC_KEYUPDATE_MAXSECS, params->KeyUpdate.MaxInterval);
	SetValue(IDC_KEYUPDATE_BYTES, params->KeyUpdate.RequireAfterNumProcessedBytes);
	SetValue(IDC_KEYUPDATE_MAXDURATION, params->KeyUpdate.MaxDuration);

	return true;
}

bool CSecurityDlgKeyUpdatesTab::SaveData() noexcept
{
	auto params = GetSecurityParameters();

	params->KeyUpdate.MinInterval = std::chrono::seconds(GetSizeValue(IDC_KEYUPDATE_MINSECS));
	params->KeyUpdate.MaxInterval = std::chrono::seconds(GetSizeValue(IDC_KEYUPDATE_MAXSECS));
	params->KeyUpdate.RequireAfterNumProcessedBytes = GetSizeValue(IDC_KEYUPDATE_BYTES);
	params->KeyUpdate.MaxDuration = std::chrono::seconds(GetSizeValue(IDC_KEYUPDATE_MAXDURATION));

	return true;
}