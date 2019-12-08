// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CEndpointDlg.h"

CEndpointDlg::CEndpointDlg(CWnd* pParent) : CDialogBase(CEndpointDlg::IDD, pParent)
{}

CEndpointDlg::~CEndpointDlg()
{}

void CEndpointDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogBase::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CEndpointDlg, CDialogBase)
	ON_BN_CLICKED(IDOK, &CEndpointDlg::OnBnClickedOk)
END_MESSAGE_MAP()

void CEndpointDlg::SetIPAddress(const String& ip) noexcept
{
	if (!IPAddress::TryParse(ip, m_IPAddress)) AfxMessageBox(L"Invalid IP address.", MB_ICONERROR);
}

BOOL CEndpointDlg::OnInitDialog()
{
	CDialogBase::OnInitDialog();

	SetValue(IDC_IP, m_IPAddress.GetString());
	SetValue(IDC_PORT, m_Port);
	SetValue(IDC_HOPS, m_Hops);
	if (m_RelayGatewayPeer) SetValue(IDC_RELAY_PEER, *m_RelayGatewayPeer);

	if (m_ShowRelay)
	{
		GetDlgItem(IDC_HOPS)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_HOPS_LABEL)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_RELAY_PEER)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_RELAY_PEER_LABEL)->ShowWindow(SW_SHOW);
	}

	return TRUE;
}

void CEndpointDlg::OnBnClickedOk()
{
	if (IPAddress::TryParse(GetTextValue(IDC_IP).GetString(), m_IPAddress))
	{
		m_Port = static_cast<UInt16>(GetInt64Value(IDC_PORT));
		m_PassPhrase = GetTextValue(IDC_PASSPHRASE);
		m_Hops = static_cast<UInt8>(GetInt64Value(IDC_HOPS));

		auto id = GetUInt64Value(IDC_RELAY_PEER);
		if (id != 0) m_RelayGatewayPeer = id;

		CDialogBase::OnOK();
	}
	else
	{
		AfxMessageBox(L"Specify a valid IP address.", MB_ICONERROR);
	}
}
