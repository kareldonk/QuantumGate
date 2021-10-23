// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestApp.h"
#include "CSecurityDlgNoiseTab.h"

IMPLEMENT_DYNCREATE(CSecurityDlgNoiseTab, CSecurityDlgTabCtrlPage)

CSecurityDlgNoiseTab::CSecurityDlgNoiseTab(CWnd* pParent /*=nullptr*/)
	: CSecurityDlgTabCtrlPage(IDD_SECURITY_SETTINGS_NOISE_TAB, pParent)
{}

CSecurityDlgNoiseTab::~CSecurityDlgNoiseTab()
{}

void CSecurityDlgNoiseTab::DoDataExchange(CDataExchange* pDX)
{
	CSecurityDlgTabCtrlPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSecurityDlgNoiseTab, CSecurityDlgTabCtrlPage)
	ON_BN_CLICKED(IDC_NOISE_AUTO_USE, &CSecurityDlgNoiseTab::OnBnClickedNoiseAutoUse)
	ON_EN_CHANGE(IDC_NOISE_AUTO_SECONDS, &CSecurityDlgNoiseTab::OnEnChangeNoiseAutoSeconds)
	ON_EN_CHANGE(IDC_NOISE_AUTO_MIN_BANDWIDTH, &CSecurityDlgNoiseTab::OnEnChangeNoiseAutoMinBandwidth)
	ON_EN_CHANGE(IDC_NOISE_AUTO_MAX_BANDWIDTH, &CSecurityDlgNoiseTab::OnEnChangeNoiseAutoMaxBandwidth)
	ON_BN_CLICKED(IDC_NOISE_AUTO_SATURATE, &CSecurityDlgNoiseTab::OnBnClickedNoiseAutoSaturate)
END_MESSAGE_MAP()

// CSecurityDlgNoiseTab message handlers

bool CSecurityDlgNoiseTab::LoadData() noexcept
{
	auto params = GetSecurityParameters();

	SetValue(IDC_SENDNOISE, params->Noise.Enabled);
	SetValue(IDC_NOISE_MSG_INTERVAL, params->Noise.TimeInterval);
	SetValue(IDC_NUM_NOISE_MSG, params->Noise.MinMessagesPerInterval);
	SetValue(IDC_NUM_NOISE_MSG_MAX, params->Noise.MaxMessagesPerInterval);
	SetValue(IDC_NOISE_MINSIZE, params->Noise.MinMessageSize);
	SetValue(IDC_NOISE_MAXSIZE, params->Noise.MaxMessageSize);

	if (GetQuantumGateInstance()->GetSecurityLevel() != SecurityLevel::Custom)
	{
		m_NoiseBasedOnBandwidth.Use = false;
	}

	SetValue(IDC_NOISE_AUTO_USE, m_NoiseBasedOnBandwidth.Use);
	SetValue(IDC_NOISE_AUTO_SECONDS, m_NoiseBasedOnBandwidth.TimeInterval.count());
	SetValue(IDC_NOISE_AUTO_MIN_BANDWIDTH, m_NoiseBasedOnBandwidth.MinimumBandwidth);
	SetValue(IDC_NOISE_AUTO_MAX_BANDWIDTH, m_NoiseBasedOnBandwidth.MaximumBandwidth);
	SetValue(IDC_NOISE_AUTO_SATURATE, m_NoiseBasedOnBandwidth.Saturate);

	m_CanCalculateNoiseSettings = true;

	return true;
}

bool CSecurityDlgNoiseTab::SaveData() noexcept
{
	auto params = GetSecurityParameters();

	params->Noise.Enabled = (((CButton*)GetDlgItem(IDC_SENDNOISE))->GetCheck() == BST_CHECKED);
	params->Noise.TimeInterval = std::chrono::seconds(GetSizeValue(IDC_NOISE_MSG_INTERVAL));
	params->Noise.MinMessagesPerInterval = GetSizeValue(IDC_NUM_NOISE_MSG);
	params->Noise.MaxMessagesPerInterval = GetSizeValue(IDC_NUM_NOISE_MSG_MAX);
	params->Noise.MinMessageSize = GetSizeValue(IDC_NOISE_MINSIZE);
	params->Noise.MaxMessageSize = GetSizeValue(IDC_NOISE_MAXSIZE);

	m_NoiseBasedOnBandwidth.Use = (((CButton*)GetDlgItem(IDC_NOISE_AUTO_USE))->GetCheck() == BST_CHECKED);
	m_NoiseBasedOnBandwidth.TimeInterval = std::chrono::seconds(GetSizeValue(IDC_NOISE_AUTO_SECONDS));
	m_NoiseBasedOnBandwidth.MinimumBandwidth = GetSizeValue(IDC_NOISE_AUTO_MIN_BANDWIDTH);
	m_NoiseBasedOnBandwidth.MaximumBandwidth = GetSizeValue(IDC_NOISE_AUTO_MAX_BANDWIDTH);
	m_NoiseBasedOnBandwidth.Saturate = (((CButton*)GetDlgItem(IDC_NOISE_AUTO_SATURATE))->GetCheck() == BST_CHECKED);

	return true;
}

void CSecurityDlgNoiseTab::OnBnClickedNoiseAutoUse()
{
	UpdateControls();
	CalculateNoiseSettings();
}

void CSecurityDlgNoiseTab::UpdateControls() noexcept
{
	const auto nbb = (((CButton*)GetDlgItem(IDC_NOISE_AUTO_USE))->GetCheck() == BST_CHECKED);

	((CEdit*)GetDlgItem(IDC_NOISE_AUTO_SECONDS))->SetReadOnly(!nbb);
	((CEdit*)GetDlgItem(IDC_NOISE_AUTO_MIN_BANDWIDTH))->SetReadOnly(!nbb);
	((CEdit*)GetDlgItem(IDC_NOISE_AUTO_MAX_BANDWIDTH))->SetReadOnly(!nbb);
	GetDlgItem(IDC_NOISE_AUTO_SATURATE)->EnableWindow(nbb);

	((CEdit*)GetDlgItem(IDC_NUM_NOISE_MSG))->SetReadOnly(nbb);
	((CEdit*)GetDlgItem(IDC_NUM_NOISE_MSG_MAX))->SetReadOnly(nbb);
	((CEdit*)GetDlgItem(IDC_NOISE_MSG_INTERVAL))->SetReadOnly(nbb);
	((CEdit*)GetDlgItem(IDC_NOISE_MINSIZE))->SetReadOnly(nbb);
	((CEdit*)GetDlgItem(IDC_NOISE_MAXSIZE))->SetReadOnly(nbb);
}

void CSecurityDlgNoiseTab::CalculateNoiseSettings() noexcept
{
	if (m_CanCalculateNoiseSettings && ((CButton*)GetDlgItem(IDC_NOISE_AUTO_USE))->GetCheck() == BST_CHECKED)
	{
		Size minmsgsize{ 0 };
		Size maxmsgsize{ 16384 };

		const auto numsecs = GetSizeValue(IDC_NOISE_AUTO_SECONDS);

		auto minbw = GetSizeValue(IDC_NOISE_AUTO_MIN_BANDWIDTH);
		if (((CButton*)GetDlgItem(IDC_NOISE_AUTO_SATURATE))->GetCheck() == BST_CHECKED)
		{
			minbw = GetSizeValue(IDC_NOISE_AUTO_MAX_BANDWIDTH);
		}

		auto maxbw = GetSizeValue(IDC_NOISE_AUTO_MAX_BANDWIDTH);
		if (maxbw < minbw) maxbw = minbw;

		auto maxmsg = static_cast<Size>((static_cast<double>(maxbw) / static_cast<double>(maxmsgsize)) * static_cast<double>(numsecs));
		while (maxmsg == 0u)
		{
			if (maxmsgsize > 1u)
			{
				--maxmsgsize;
				maxmsg = static_cast<Size>((static_cast<double>(maxbw) / static_cast<double>(maxmsgsize)) * static_cast<double>(numsecs));
			}
			else
			{
				maxmsgsize = 0u;
				break;
			}
		}

		Size minmsg{ 0 };
		if (maxbw > 0) minmsg = static_cast<Size>(static_cast<double>(maxmsg) * (static_cast<double>(minbw) / static_cast<double>(maxbw)));

		if (minmsg == 0 && minbw > 0 && numsecs > 0) minmsg = 1u;

		auto minmsgd = minmsg;
		if (minmsgd == 0u) minmsgd = 1u;

		minmsgsize = static_cast<Size>((static_cast<double>(numsecs) * static_cast<double>(minbw)) / static_cast<double>(minmsgd));
		while (minmsgsize > maxmsgsize)
		{
			if (minmsg < maxmsg)
			{
				++minmsg;
				minmsgsize = static_cast<Size>((static_cast<double>(numsecs) * static_cast<double>(minbw)) / static_cast<double>(minmsg));
			}
			else minmsgsize = maxmsgsize;
		}

		SetValue(IDC_NOISE_MSG_INTERVAL, numsecs);
		SetValue(IDC_NUM_NOISE_MSG, minmsg);
		SetValue(IDC_NUM_NOISE_MSG_MAX, maxmsg);
		SetValue(IDC_NOISE_MINSIZE, minmsgsize);
		SetValue(IDC_NOISE_MAXSIZE, maxmsgsize);
	}
}

void CSecurityDlgNoiseTab::OnEnChangeNoiseAutoSeconds()
{
	CalculateNoiseSettings();
}

void CSecurityDlgNoiseTab::OnEnChangeNoiseAutoMinBandwidth()
{
	CalculateNoiseSettings();
}

void CSecurityDlgNoiseTab::OnEnChangeNoiseAutoMaxBandwidth()
{
	CalculateNoiseSettings();
}

void CSecurityDlgNoiseTab::OnBnClickedNoiseAutoSaturate()
{
	CalculateNoiseSettings();
}
