// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "TestApp.h"
#include "CIPFiltersDlg.h"

#include "Common\Util.h"
#include "Network\IPAddress.h"

using namespace QuantumGate::Implementation;

CIPFiltersDlg::CIPFiltersDlg(CWnd* pParent) : CDialogBase(CIPFiltersDlg::IDD, pParent)
{}

CIPFiltersDlg::~CIPFiltersDlg()
{}

void CIPFiltersDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogBase::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CIPFiltersDlg, CDialogBase)
	ON_BN_CLICKED(IDC_REMOVEFILTER, &CIPFiltersDlg::OnBnClickedRemovefilter)
	ON_EN_CHANGE(IDC_IP, &CIPFiltersDlg::OnEnChangeIp)
	ON_EN_CHANGE(IDC_MASK, &CIPFiltersDlg::OnEnChangeMask)
	ON_CBN_SELCHANGE(IDC_TYPECOMBO, &CIPFiltersDlg::OnCbnSelChangeTypeCombo)
	ON_BN_CLICKED(IDC_ADDFILTER, &CIPFiltersDlg::OnBnClickedAddfilter)
	ON_EN_CHANGE(IDC_TEST_IP, &CIPFiltersDlg::OnEnChangeTestIp)
	ON_BN_CLICKED(IDC_TEST_BUTTON, &CIPFiltersDlg::OnBnClickedTestButton)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_IPFILTERS_LIST, &CIPFiltersDlg::OnLvnItemchangedIpfiltersList)
	ON_NOTIFY(NM_DBLCLK, IDC_IPFILTERS_LIST, &CIPFiltersDlg::OnNMDblclkIpfiltersList)
END_MESSAGE_MAP()

void CIPFiltersDlg::SetAccessManager(QuantumGate::AccessManager* am) noexcept
{
	m_AccessManager = am;
}

BOOL CIPFiltersDlg::OnInitDialog()
{
	CDialogBase::OnInitDialog();

	// Init filter type combo
	auto tcombo = (CComboBox*)GetDlgItem(IDC_TYPECOMBO);
	auto pos = tcombo->AddString(L"Allowed");
	tcombo->SetItemData(pos, static_cast<DWORD_PTR>(QuantumGate::IPFilterType::Allowed));
	pos = tcombo->AddString(L"Blocked");
	tcombo->SetItemData(pos, static_cast<DWORD_PTR>(QuantumGate::IPFilterType::Blocked));

	// Init filter list
	auto flctrl = (CListCtrl*)GetDlgItem(IDC_IPFILTERS_LIST);
	flctrl->SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
	flctrl->InsertColumn(0, _T("IP"), LVCFMT_LEFT, GetApp()->GetScaledWidth(125));
	flctrl->InsertColumn(1, _T("Mask"), LVCFMT_LEFT, GetApp()->GetScaledWidth(125));
	flctrl->InsertColumn(2, _T("Type"), LVCFMT_LEFT, GetApp()->GetScaledWidth(100));
	flctrl->InsertColumn(3, _T("ID"), LVCFMT_LEFT, 0); // Hidden

	UpdateIPFilterList();
	UpdateControls();

	return TRUE;
}

void CIPFiltersDlg::UpdateIPFilterList() noexcept
{
	auto flctrl = (CListCtrl*)GetDlgItem(IDC_IPFILTERS_LIST);
	flctrl->DeleteAllItems();

	if (const auto result = m_AccessManager->GetAllIPFilters(); result.Succeeded())
	{
		for (auto& flt : *result)
		{
			const auto pos = flctrl->InsertItem(0, flt.Address.GetString().c_str());
			if (pos != -1)
			{
				flctrl->SetItemText(pos, 1, flt.Mask.GetString().c_str());

				CString type = L"Allowed";
				if (flt.Type == QuantumGate::IPFilterType::Blocked) type = L"Blocked";
				flctrl->SetItemText(pos, 2, type);

				flctrl->SetItemText(pos, 3, std::to_wstring(flt.FilterID).c_str());
			}
		}
	}
}

void CIPFiltersDlg::UpdateControls() noexcept
{
	const auto ip = GetTextValue(IDC_IP);
	const auto mask = GetTextValue(IDC_MASK);
	const auto sel = ((CComboBox*)GetDlgItem(IDC_TYPECOMBO))->GetCurSel();

	if (ip.GetLength() > 0 && mask.GetLength() > 0 && sel != -1)
	{
		GetDlgItem(IDC_ADDFILTER)->EnableWindow(true);
	}
	else
	{
		GetDlgItem(IDC_ADDFILTER)->EnableWindow(false);
	}

	auto tip = GetTextValue(IDC_TEST_IP);
	if (tip.GetLength() > 0)
	{
		GetDlgItem(IDC_TEST_BUTTON)->EnableWindow(true);
	}
	else
	{
		GetDlgItem(IDC_TEST_BUTTON)->EnableWindow(false);
	}

	const auto flctrl = (CListCtrl*)GetDlgItem(IDC_IPFILTERS_LIST);
	if (flctrl->GetSelectedCount() > 0)
	{
		GetDlgItem(IDC_REMOVEFILTER)->EnableWindow(true);
	}
	else
	{
		GetDlgItem(IDC_REMOVEFILTER)->EnableWindow(false);
	}
}

CString CIPFiltersDlg::GetIPRange(const CString& ip, const CString& mask) const noexcept
{
	QuantumGate::IPAddress ipaddr;
	if (QuantumGate::IPAddress::TryParse(ip.GetString(), ipaddr))
	{
		QuantumGate::IPAddress ipmask;
		if (QuantumGate::IPAddress::TryParseMask(ipaddr.GetFamily(), mask.GetString(), ipmask))
		{
			Dbg(L"ip %s %s", Util::ToBinaryString(ipaddr.GetBinary().UInt64s[1]).c_str(),
				Util::ToBinaryString(ipaddr.GetBinary().UInt64s[0]).c_str());

			Dbg(L"ma %s %s", Util::ToBinaryString(ipmask.GetBinary().UInt64s[1]).c_str(),
				Util::ToBinaryString(ipmask.GetBinary().UInt64s[0]).c_str());

			const auto StartAddress = ipaddr.GetBinary() & ipmask.GetBinary();
			const auto EndAddress = ipaddr.GetBinary() | ~ipmask.GetBinary();

			const auto start = QuantumGate::IPAddress(StartAddress);
			const auto end = QuantumGate::IPAddress(EndAddress);
			const auto str = start.GetString() + L" - " + end.GetString();

			return str.c_str();
		}
	}

	return L"None";
}

void CIPFiltersDlg::UpdateIPRange() noexcept
{
	auto ip = GetTextValue(IDC_IP);
	auto mask = GetTextValue(IDC_MASK);

	SetValue(IDC_IP_RANGE, GetIPRange(ip, mask));
}

void CIPFiltersDlg::OnEnChangeIp()
{
	UpdateControls();
	UpdateIPRange();
}

void CIPFiltersDlg::OnEnChangeMask()
{
	UpdateControls();
	UpdateIPRange();
}

void CIPFiltersDlg::OnCbnSelChangeTypeCombo()
{
	UpdateControls();
}

void CIPFiltersDlg::OnBnClickedAddfilter()
{
	const auto ip = GetTextValue(IDC_IP);
	const auto mask = GetTextValue(IDC_MASK);
	const auto sel = ((CComboBox*)GetDlgItem(IDC_TYPECOMBO))->GetCurSel();
	const auto type = static_cast<QuantumGate::IPFilterType>(((CComboBox*)GetDlgItem(IDC_TYPECOMBO))->GetItemData(sel));

	const auto result = m_AccessManager->AddIPFilter((LPCTSTR)ip, (LPCTSTR)mask, type);
	if (result.Failed())
	{
		AfxMessageBox(L"Couldn't add the IP address to the filters; check the format of IP and Mask and try again.",
					  MB_ICONERROR);
	}
	else
	{
		SetValue(IDC_IP, L"");
		SetValue(IDC_MASK, L"");
		((CComboBox*)GetDlgItem(IDC_TYPECOMBO))->SetCurSel(-1);

		UpdateIPFilterList();
	}
}

void CIPFiltersDlg::OnEnChangeTestIp()
{
	UpdateControls();
}

void CIPFiltersDlg::OnBnClickedTestButton()
{
	auto ip = GetTextValue(IDC_TEST_IP);
	const auto result = m_AccessManager->IsIPAllowed((LPCTSTR)ip, QuantumGate::AccessCheck::IPFilters);
	if (result.Succeeded())
	{
		if (*result)
		{
			SetValue(IDC_IP_TEST_RESULT, L"The IP address is allowed.");
		}
		else
		{
			SetValue(IDC_IP_TEST_RESULT, L"The IP address is NOT allowed.");
		}
	}
	else if (result == QuantumGate::ResultCode::AddressInvalid)
	{
		SetValue(IDC_IP_TEST_RESULT, L"Invalid IP address specified!");
	}
}

void CIPFiltersDlg::OnBnClickedRemovefilter()
{
	const auto flctrl = (CListCtrl*)GetDlgItem(IDC_IPFILTERS_LIST);
	if (flctrl->GetSelectedCount() > 0)
	{
		auto position = flctrl->GetFirstSelectedItemPosition();
		const auto pos = flctrl->GetNextSelectedItem(position);
		const auto ip = flctrl->GetItemText(pos, 0);
		const auto mask = flctrl->GetItemText(pos, 1);
		const auto type = flctrl->GetItemText(pos, 2);
		const auto id = flctrl->GetItemText(pos, 3);

		auto ftype = QuantumGate::IPFilterType::Allowed;
		if (type == L"Blocked") ftype = QuantumGate::IPFilterType::Blocked;

		if (m_AccessManager->RemoveIPFilter(std::stoull(id.GetString()), ftype) != QuantumGate::ResultCode::Succeeded)
		{
			AfxMessageBox(L"Couldn't remove the IP address from the filters.", MB_ICONERROR);
		}
		else
		{
			UpdateIPFilterList();
			UpdateControls();
		}
	}
}

void CIPFiltersDlg::OnLvnItemchangedIpfiltersList(NMHDR* pNMHDR, LRESULT* pResult)
{
	auto flctrl = (CListCtrl*)GetDlgItem(IDC_IPFILTERS_LIST);
	if (flctrl->GetSelectedCount() > 0)
	{
		auto position = flctrl->GetFirstSelectedItemPosition();
		const auto pos = flctrl->GetNextSelectedItem(position);
		const auto ip = flctrl->GetItemText(pos, 0);
		const auto mask = flctrl->GetItemText(pos, 1);

		SetValue(IDC_IP_RANGE2, GetIPRange(ip, mask));
	}

	UpdateControls();

	*pResult = 0;
}

void CIPFiltersDlg::OnNMDblclkIpfiltersList(NMHDR* pNMHDR, LRESULT* pResult)
{
	const auto flctrl = (CListCtrl*)GetDlgItem(IDC_IPFILTERS_LIST);
	if (flctrl->GetSelectedCount() > 0)
	{
		auto position = flctrl->GetFirstSelectedItemPosition();
		const auto pos = flctrl->GetNextSelectedItem(position);
		const auto ip = flctrl->GetItemText(pos, 0);
		const auto mask = flctrl->GetItemText(pos, 1);
		const auto type = flctrl->GetItemText(pos, 2);

		SetValue(IDC_IP, ip);
		SetValue(IDC_MASK, mask);

		const auto combo = (CComboBox*)GetDlgItem(IDC_TYPECOMBO);
		const auto spos = combo->FindStringExact(-1, type);
		if (spos != CB_ERR) combo->SetCurSel(spos);
		else combo->SetCurSel(-1);
	}

	UpdateControls();

	*pResult = 0;
}
