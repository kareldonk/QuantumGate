// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CSecurityDlg.h"

CSecurityDlg::CSecurityDlg(CWnd* pParent) : CDialogBase(CSecurityDlg::IDD, pParent)
{}

CSecurityDlg::~CSecurityDlg()
{}

void CSecurityDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogBase::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSecurityDlg, CDialogBase)
	ON_BN_CLICKED(IDOK, &CSecurityDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_NOISE_AUTO_USE, &CSecurityDlg::OnBnClickedNoiseAutoUse)
	ON_EN_CHANGE(IDC_NOISE_AUTO_SECONDS, &CSecurityDlg::OnEnChangeNoiseAutoSeconds)
	ON_EN_CHANGE(IDC_NOISE_AUTO_MIN_BANDWIDTH, &CSecurityDlg::OnEnChangeNoiseAutoMinBandwidth)
	ON_EN_CHANGE(IDC_NOISE_AUTO_MAX_BANDWIDTH, &CSecurityDlg::OnEnChangeNoiseAutoMaxBandwidth)
	ON_BN_CLICKED(IDC_NOISE_AUTO_SATURATE, &CSecurityDlg::OnBnClickedNoiseAutoSaturate)
END_MESSAGE_MAP()

BOOL CSecurityDlg::OnInitDialog()
{
	CDialogBase::OnInitDialog();

	assert(m_QuantumGate != nullptr);

	const auto params = m_QuantumGate->GetSecurityParameters();

	SetValue(IDC_COND_ACCEPT, params.General.UseConditionalAcceptFunction);

	SetValue(IDC_CONNECT_TIMEOUT, params.General.ConnectTimeout);

	SetValue(IDC_HANDSHAKE_DELAY, params.General.MaxHandshakeDelay);
	SetValue(IDC_HANDSHAKE_DURATION, params.General.MaxHandshakeDuration);

	SetValue(IDC_IPREP_IMPROVE_INTERVAL, params.General.IPReputationImprovementInterval);

	SetValue(IDC_NUM_IPCON_ATTEMPTS, params.General.IPConnectionAttempts.MaxPerInterval);
	SetValue(IDC_IPCON_ATTEMPTS_INTERVAL, params.General.IPConnectionAttempts.Interval);

	SetValue(IDC_KEYUPDATE_MINSECS, params.KeyUpdate.MinInterval);
	SetValue(IDC_KEYUPDATE_MAXSECS, params.KeyUpdate.MaxInterval);
	SetValue(IDC_KEYUPDATE_BYTES, params.KeyUpdate.RequireAfterNumProcessedBytes);
	SetValue(IDC_KEYUPDATE_MAXDURATION, params.KeyUpdate.MaxDuration);

	SetValue(IDC_RELAY_CONNECT_TIMEOUT, params.Relay.ConnectTimeout);
	SetValue(IDC_RELAY_GRACEPERIOD, params.Relay.GracePeriod);
	SetValue(IDC_RELAY_NUM_IPCON_ATTEMPTS, params.Relay.IPConnectionAttempts.MaxPerInterval);
	SetValue(IDC_RELAY_IPCON_ATTEMPTS_INTERVAL, params.Relay.IPConnectionAttempts.Interval);

	SetValue(IDC_MESSAGE_AGE_TOLERANCE, params.Message.AgeTolerance);
	SetValue(IDC_EXTENDER_GRACE_PERIOD, params.Message.ExtenderGracePeriod);
	SetValue(IDC_MSG_RND_PREFIX_MIN, params.Message.MinRandomDataPrefixSize);
	SetValue(IDC_MSG_RND_PREFIX_MAX, params.Message.MaxRandomDataPrefixSize);
	SetValue(IDC_MSG_RND_MIN, params.Message.MinInternalRandomDataSize);
	SetValue(IDC_MSG_RND_MAX, params.Message.MaxInternalRandomDataSize);

	SetValue(IDC_SENDNOISE, params.Noise.Enabled);

	SetValue(IDC_NOISE_MSG_INTERVAL, params.Noise.TimeInterval);
	SetValue(IDC_NUM_NOISE_MSG, params.Noise.MinMessagesPerInterval);
	SetValue(IDC_NUM_NOISE_MSG_MAX, params.Noise.MaxMessagesPerInterval);
	SetValue(IDC_NOISE_MINSIZE, params.Noise.MinMessageSize);
	SetValue(IDC_NOISE_MAXSIZE, params.Noise.MaxMessageSize);

	SetValue(IDC_NOISE_AUTO_USE, m_NoiseBasedOnBandwidth.Use);
	SetValue(IDC_NOISE_AUTO_SECONDS, m_NoiseBasedOnBandwidth.TimeInterval.count());
	SetValue(IDC_NOISE_AUTO_MIN_BANDWIDTH, m_NoiseBasedOnBandwidth.MinimumBandwidth);
	SetValue(IDC_NOISE_AUTO_MAX_BANDWIDTH, m_NoiseBasedOnBandwidth.MaximumBandwidth);
	SetValue(IDC_NOISE_AUTO_SATURATE, m_NoiseBasedOnBandwidth.Saturate);

	UpdateNoiseControls();

	m_CanCalculateNoiseSettings = true;

	return TRUE;
}

void CSecurityDlg::OnBnClickedOk()
{
	QuantumGate::SecurityParameters params;

	params.General.UseConditionalAcceptFunction = (((CButton*)GetDlgItem(IDC_COND_ACCEPT))->GetCheck() == BST_CHECKED);
	params.General.ConnectTimeout = std::chrono::seconds(GetSizeValue(IDC_CONNECT_TIMEOUT));
	params.General.MaxHandshakeDelay = std::chrono::milliseconds(GetSizeValue(IDC_HANDSHAKE_DELAY));
	params.General.MaxHandshakeDuration = std::chrono::seconds(GetSizeValue(IDC_HANDSHAKE_DURATION));
	params.General.IPReputationImprovementInterval = std::chrono::seconds(GetSizeValue(IDC_IPREP_IMPROVE_INTERVAL));
	params.General.IPConnectionAttempts.MaxPerInterval = GetSizeValue(IDC_NUM_IPCON_ATTEMPTS);
	params.General.IPConnectionAttempts.Interval = std::chrono::seconds(GetSizeValue(IDC_IPCON_ATTEMPTS_INTERVAL));

	params.KeyUpdate.MinInterval = std::chrono::seconds(GetSizeValue(IDC_KEYUPDATE_MINSECS));
	params.KeyUpdate.MaxInterval = std::chrono::seconds(GetSizeValue(IDC_KEYUPDATE_MAXSECS));
	params.KeyUpdate.RequireAfterNumProcessedBytes = GetSizeValue(IDC_KEYUPDATE_BYTES);
	params.KeyUpdate.MaxDuration = std::chrono::seconds(GetSizeValue(IDC_KEYUPDATE_MAXDURATION));

	params.Relay.ConnectTimeout = std::chrono::seconds(GetSizeValue(IDC_RELAY_CONNECT_TIMEOUT));
	params.Relay.GracePeriod = std::chrono::seconds(GetSizeValue(IDC_RELAY_GRACEPERIOD));
	params.Relay.IPConnectionAttempts.MaxPerInterval = GetSizeValue(IDC_RELAY_NUM_IPCON_ATTEMPTS);
	params.Relay.IPConnectionAttempts.Interval = std::chrono::seconds(GetSizeValue(IDC_RELAY_IPCON_ATTEMPTS_INTERVAL));

	params.Message.AgeTolerance = std::chrono::seconds(GetSizeValue(IDC_MESSAGE_AGE_TOLERANCE));
	params.Message.ExtenderGracePeriod = std::chrono::seconds(GetSizeValue(IDC_EXTENDER_GRACE_PERIOD));
	params.Message.MinRandomDataPrefixSize = GetSizeValue(IDC_MSG_RND_PREFIX_MIN);
	params.Message.MaxRandomDataPrefixSize = GetSizeValue(IDC_MSG_RND_PREFIX_MAX);
	params.Message.MinInternalRandomDataSize = GetSizeValue(IDC_MSG_RND_MIN);
	params.Message.MaxInternalRandomDataSize = GetSizeValue(IDC_MSG_RND_MAX);

	params.Noise.Enabled = (((CButton*)GetDlgItem(IDC_SENDNOISE))->GetCheck() == BST_CHECKED);
	params.Noise.TimeInterval = std::chrono::seconds(GetSizeValue(IDC_NOISE_MSG_INTERVAL));
	params.Noise.MinMessagesPerInterval = GetSizeValue(IDC_NUM_NOISE_MSG);
	params.Noise.MaxMessagesPerInterval = GetSizeValue(IDC_NUM_NOISE_MSG_MAX);
	params.Noise.MinMessageSize = GetSizeValue(IDC_NOISE_MINSIZE);
	params.Noise.MaxMessageSize = GetSizeValue(IDC_NOISE_MAXSIZE);

	if (m_QuantumGate->SetSecurityLevel(SecurityLevel::Custom, params).Failed())
	{
		AfxMessageBox(L"Could not set custom security level.", MB_ICONERROR);
		return;
	}

	m_NoiseBasedOnBandwidth.Use = (((CButton*)GetDlgItem(IDC_NOISE_AUTO_USE))->GetCheck() == BST_CHECKED);
	m_NoiseBasedOnBandwidth.TimeInterval = std::chrono::seconds(GetSizeValue(IDC_NOISE_AUTO_SECONDS));
	m_NoiseBasedOnBandwidth.MinimumBandwidth = GetSizeValue(IDC_NOISE_AUTO_MIN_BANDWIDTH);
	m_NoiseBasedOnBandwidth.MaximumBandwidth = GetSizeValue(IDC_NOISE_AUTO_MAX_BANDWIDTH);
	m_NoiseBasedOnBandwidth.Saturate = (((CButton*)GetDlgItem(IDC_NOISE_AUTO_SATURATE))->GetCheck() == BST_CHECKED);

	CDialogBase::OnOK();
}

void CSecurityDlg::OnBnClickedNoiseAutoUse()
{
	UpdateNoiseControls();
	CalculateNoiseSettings();
}

void CSecurityDlg::UpdateNoiseControls() noexcept
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

void CSecurityDlg::CalculateNoiseSettings() noexcept
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

void CSecurityDlg::OnEnChangeNoiseAutoSeconds()
{
	CalculateNoiseSettings();
}

void CSecurityDlg::OnEnChangeNoiseAutoMinBandwidth()
{
	CalculateNoiseSettings();
}

void CSecurityDlg::OnEnChangeNoiseAutoMaxBandwidth()
{
	CalculateNoiseSettings();
}

void CSecurityDlg::OnBnClickedNoiseAutoSaturate()
{
	CalculateNoiseSettings();
}
