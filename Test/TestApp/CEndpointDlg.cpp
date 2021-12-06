// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "CEndpointDlg.h"
#include "Common\Util.h"

#include <regex>

using namespace QuantumGate::Implementation;

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
	ON_CBN_SELCHANGE(IDC_PROTOCOL_COMBO, &CEndpointDlg::OnCbnSelChangeProtocolCombo)
	ON_BN_CLICKED(IDC_BTH_SERVICE_BUTTON, &CEndpointDlg::OnBnClickedBthServiceButton)
END_MESSAGE_MAP()

void CEndpointDlg::SetAddress(const String& addr) noexcept
{
	if (!Address::TryParse(addr, m_Address)) AfxMessageBox(L"Invalid address specified.", MB_ICONERROR);
}

BOOL CEndpointDlg::OnInitDialog()
{
	CDialogBase::OnInitDialog();

	// Init IP combo
	{
		const auto pcombo = (CComboBox*)GetDlgItem(IDC_ADDRESS);

		Address addr;
		String::size_type pos{ 0 };
		String::size_type start{ 0 };

		while ((pos = m_AddressHistory.find(L";", start)) != String::npos)
		{
			const auto addr_str = m_AddressHistory.substr(start, pos - start);

			if (Address::TryParse(addr_str, addr))
			{
				pcombo->AddString(addr_str.c_str());
			}

			start = pos + 1;
		}

		const auto addr_str = m_AddressHistory.substr(start, m_AddressHistory.size() - start);
		if (!addr_str.empty())
		{
			if (Address::TryParse(addr_str, addr))
			{
				pcombo->AddString(addr_str.c_str());
			}
		}
	}

	SetValue(IDC_ADDRESS, m_Address.GetString());
	SetValue(IDC_PORT, m_Port);

	// Init protocol combo
	{
		const auto pcombo = (CComboBox*)GetDlgItem(IDC_PROTOCOL_COMBO);

		if (m_Protocols.contains(Endpoint::Protocol::TCP))
		{
			const auto pos = pcombo->AddString(L"TCP");
			pcombo->SetItemData(pos, static_cast<DWORD_PTR>(QuantumGate::Endpoint::Protocol::TCP));
			if (m_Protocol == QuantumGate::Endpoint::Protocol::TCP) pcombo->SetCurSel(pos);
		}

		if (m_Protocols.contains(Endpoint::Protocol::UDP))
		{
			const auto pos = pcombo->AddString(L"UDP");
			pcombo->SetItemData(pos, static_cast<DWORD_PTR>(QuantumGate::Endpoint::Protocol::UDP));
			if (m_Protocol == QuantumGate::Endpoint::Protocol::UDP) pcombo->SetCurSel(pos);
		}

		if (m_Protocols.contains(Endpoint::Protocol::BTH))
		{
			const auto pos = pcombo->AddString(L"BTH");
			pcombo->SetItemData(pos, static_cast<DWORD_PTR>(QuantumGate::Endpoint::Protocol::BTH));
			if (m_Protocol == QuantumGate::Endpoint::Protocol::BTH) pcombo->SetCurSel(pos);
		}

		OnCbnSelChangeProtocolCombo();
	}

	SetValue(IDC_HOPS, m_Hops);

	if (m_BTHAuthentication) ((CButton*)GetDlgItem(IDC_BTH_AUTH))->SetCheck(BST_CHECKED);
	if (m_ReuseConnection) ((CButton*)GetDlgItem(IDC_REUSE_CONNECTION))->SetCheck(BST_CHECKED);
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
	const auto addr_str = GetTextValue(IDC_ADDRESS);
	if (Address::TryParse(addr_str.GetString(), m_Address))
	{
		const auto sel = ((CComboBox*)GetDlgItem(IDC_PROTOCOL_COMBO))->GetCurSel();
		if (sel == CB_ERR)
		{
			AfxMessageBox(L"Please select a protocol first.", MB_ICONINFORMATION);
			return;
		}

		const auto protocol = static_cast<Endpoint::Protocol>(((CComboBox*)GetDlgItem(IDC_PROTOCOL_COMBO))->GetItemData(sel));

		auto perror{ false };
		if (m_Address.GetType() == Address::Type::IP)
		{
			perror = (protocol != Endpoint::Protocol::TCP && protocol != Endpoint::Protocol::UDP);
		}
		else if (m_Address.GetType() == Address::Type::BTH)
		{
			perror = (protocol != Endpoint::Protocol::BTH);
		}

		if (perror)
		{
			AfxMessageBox(L"Invalid address and protocol combination.", MB_ICONERROR);
			return;
		}
		else m_Protocol = protocol;

		if (m_AddressHistory.find(addr_str) == String::npos)
		{
			if (!m_AddressHistory.empty()) m_AddressHistory += L";";

			m_AddressHistory += addr_str;
		}

		const auto port_str = GetTextValue(IDC_PORT);

		GUID temp_guid{ 0 };
		if (CLSIDFromString(port_str, &temp_guid) == NOERROR)
		{
			m_Port = 0;
			m_ServiceClassID = temp_guid;
		}
		else
		{
			// Require only numbers
			std::wregex r(LR"port(^\s*([\d]+)\s*$)port");
			std::wcmatch m;
			if (!std::regex_search(port_str.GetString(), m, r))
			{
				AfxMessageBox(L"Please specify a valid port or service class ID.", MB_ICONINFORMATION);
				return;
			}

			m_Port = static_cast<UInt16>(GetInt64Value(IDC_PORT));
			m_ServiceClassID = BTHEndpoint::GetNullServiceClassID();
		}

		m_PassPhrase = GetTextValue(IDC_PASSPHRASE);
		m_Hops = static_cast<UInt8>(GetInt64Value(IDC_HOPS));

		const auto id = GetUInt64Value(IDC_RELAY_PEER);
		if (id != 0) m_RelayGatewayPeer = id;

		m_BTHAuthentication = (((CButton*)GetDlgItem(IDC_BTH_AUTH))->GetCheck() == BST_CHECKED);
		m_ReuseConnection = (((CButton*)GetDlgItem(IDC_REUSE_CONNECTION))->GetCheck() == BST_CHECKED);

		CDialogBase::OnOK();
	}
	else
	{
		AfxMessageBox(L"Invalid address specified!", MB_ICONERROR);
	}
}

Endpoint CEndpointDlg::GetEndpoint() const noexcept
{
	if (m_Address.GetType() == Address::Type::IP)
	{
		const auto protocol = m_Protocol == Endpoint::Protocol::TCP ? IPEndpoint::Protocol::TCP : IPEndpoint::Protocol::UDP;
		return IPEndpoint(protocol, m_Address.GetIPAddress(), m_Port);
	}
	else if (m_Address.GetType() == Address::Type::BTH)
	{
		return BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, m_Address.GetBTHAddress(), m_Port, m_ServiceClassID);
	}

	return {};
}

void CEndpointDlg::OnCbnSelChangeProtocolCombo()
{
	const auto sel = ((CComboBox*)GetDlgItem(IDC_PROTOCOL_COMBO))->GetCurSel();
	if (sel != CB_ERR)
	{
		const auto protocol = static_cast<Endpoint::Protocol>(((CComboBox*)GetDlgItem(IDC_PROTOCOL_COMBO))->GetItemData(sel));
		if (protocol == Endpoint::Protocol::BTH)
		{
			GetDlgItem(IDC_BTH_AUTH)->ShowWindow(SW_SHOW);
			GetDlgItem(IDC_BTH_SERVICE_BUTTON)->ShowWindow(SW_SHOW);
			GetDlgItem(IDC_PORT_LABEL)->SetWindowText(L"Port / Service Class ID:");
		}
		else
		{
			GetDlgItem(IDC_BTH_AUTH)->ShowWindow(SW_HIDE);
			GetDlgItem(IDC_BTH_SERVICE_BUTTON)->ShowWindow(SW_HIDE);
			GetDlgItem(IDC_PORT_LABEL)->SetWindowText(L"Port:");
		}
	}
}

void CEndpointDlg::OnBnClickedBthServiceButton()
{
	SetValue(IDC_PORT, Util::ToString(BTHEndpoint::GetQuantumGateServiceClassID()));
}
