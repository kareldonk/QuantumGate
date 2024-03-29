// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestApp.h"
#include "CAddressReputationsDlg.h"
#include "Common\Util.h"

using namespace QuantumGate::Implementation;

IMPLEMENT_DYNAMIC(CAddressReputationsDlg, CDialogBase)

CAddressReputationsDlg::CAddressReputationsDlg(CWnd* pParent) : CDialogBase(IDD_ADDRESS_REPUTATIONS_DIALOG, pParent)
{}

CAddressReputationsDlg::~CAddressReputationsDlg()
{}

void CAddressReputationsDlg::SetAccessManager(QuantumGate::Access::Manager * am) noexcept
{
	m_AccessManager = am;
}

void CAddressReputationsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogBase::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAddressReputationsDlg, CDialogBase)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_ADDRESS_REPUTATIONS_LIST, &CAddressReputationsDlg::OnLvnItemChangedAddressReputationsList)
	ON_EN_CHANGE(IDC_ADDRESS, &CAddressReputationsDlg::OnEnChangeAddress)
	ON_EN_CHANGE(IDC_REPUTATION, &CAddressReputationsDlg::OnEnChangeReputation)
	ON_BN_CLICKED(IDC_SET_REPUTATION, &CAddressReputationsDlg::OnBnClickedSetReputation)
	ON_BN_CLICKED(IDC_RESET_ALL, &CAddressReputationsDlg::OnBnClickedResetAll)
	ON_BN_CLICKED(IDC_RESET_SELECTED, &CAddressReputationsDlg::OnBnClickedResetSelected)
	ON_BN_CLICKED(IDC_REFRESH, &CAddressReputationsDlg::OnBnClickedRefresh)
END_MESSAGE_MAP()

BOOL CAddressReputationsDlg::OnInitDialog()
{
	CDialogBase::OnInitDialog();

	// Init reputation list
	auto irctrl = (CListCtrl*)GetDlgItem(IDC_ADDRESS_REPUTATIONS_LIST);
	irctrl->SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
	irctrl->InsertColumn(0, _T("Address"), LVCFMT_LEFT, GetApp()->GetScaledWidth(125));
	irctrl->InsertColumn(1, _T("Score"), LVCFMT_LEFT, GetApp()->GetScaledWidth(75));
	irctrl->InsertColumn(2, _T("Last Update Time"), LVCFMT_LEFT, GetApp()->GetScaledWidth(125));

	UpdateAddressReputationList();
	UpdateControls();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CAddressReputationsDlg::UpdateAddressReputationList() noexcept
{
	auto flctrl = (CListCtrl*)GetDlgItem(IDC_ADDRESS_REPUTATIONS_LIST);
	flctrl->DeleteAllItems();

	if (const auto result = m_AccessManager->GetAllAddressReputations(); result.Succeeded())
	{
		for (const auto& rep : *result)
		{
			const auto pos = flctrl->InsertItem(0, rep.Address.GetString().c_str());
			if (pos != -1)
			{
				flctrl->SetItemText(pos, 1, Util::FormatString(L"%d", rep.Score).c_str());

				tm time_tm{ 0 };
				std::array<WChar, 100> timestr{ 0 };

				if (localtime_s(&time_tm, &*rep.LastUpdateTime) == 0)
				{
					if (std::wcsftime(timestr.data(), timestr.size(), L"%d/%m/%Y %H:%M:%S", &time_tm) != 0)
					{
						flctrl->SetItemText(pos, 2, timestr.data());
					}
				}
			}
		}
	}
}

void CAddressReputationsDlg::OnLvnItemChangedAddressReputationsList(NMHDR* pNMHDR, LRESULT* pResult)
{
	auto irctrl = (CListCtrl*)GetDlgItem(IDC_ADDRESS_REPUTATIONS_LIST);
	if (irctrl->GetSelectedCount() > 0)
	{
		auto position = irctrl->GetFirstSelectedItemPosition();
		const auto pos = irctrl->GetNextSelectedItem(position);
		const auto addr = irctrl->GetItemText(pos, 0);
		const auto rep = irctrl->GetItemText(pos, 1);

		SetValue(IDC_ADDRESS, addr);
		SetValue(IDC_REPUTATION, rep);
	}

	UpdateControls();

	*pResult = 0;
}

void CAddressReputationsDlg::UpdateControls() noexcept
{
	const auto addr = GetTextValue(IDC_ADDRESS);
	const auto rep = GetTextValue(IDC_REPUTATION);

	if (addr.GetLength() > 0 && rep.GetLength() > 0)
	{
		GetDlgItem(IDC_SET_REPUTATION)->EnableWindow(true);
	}
	else
	{
		GetDlgItem(IDC_SET_REPUTATION)->EnableWindow(false);
	}

	const auto irctrl = (CListCtrl*)GetDlgItem(IDC_ADDRESS_REPUTATIONS_LIST);
	if (irctrl->GetSelectedCount() > 0)
	{
		GetDlgItem(IDC_RESET_SELECTED)->EnableWindow(true);
	}
	else
	{
		GetDlgItem(IDC_RESET_SELECTED)->EnableWindow(false);
	}
}

void CAddressReputationsDlg::OnEnChangeAddress()
{
	UpdateControls();
}


void CAddressReputationsDlg::OnEnChangeReputation()
{
	UpdateControls();
}

void CAddressReputationsDlg::OnBnClickedSetReputation()
{
	const auto addr_str = GetTextValue(IDC_ADDRESS);

	Access::AddressReputation addr_rep{
		.Score = static_cast<Int16>(GetInt64Value(IDC_REPUTATION))
	};

	Address addr;
	if (Address::TryParse(addr_str.GetString(), addr))
	{
		addr_rep.Address = addr;
	}
	else
	{
		AfxMessageBox(L"Invalid address specified!", MB_ICONERROR);
		return;
	}

	if (m_AccessManager->SetAddressReputation(addr_rep).Succeeded())
	{
		UpdateAddressReputationList();
		UpdateControls();
	}
	else AfxMessageBox(L"Failed to set address reputation!", MB_ICONERROR);
}

void CAddressReputationsDlg::OnBnClickedResetAll()
{
	m_AccessManager->ResetAllAddressReputations();
	UpdateAddressReputationList();
	UpdateControls();
}

void CAddressReputationsDlg::OnBnClickedResetSelected()
{
	auto irctrl = (CListCtrl*)GetDlgItem(IDC_ADDRESS_REPUTATIONS_LIST);
	if (irctrl->GetSelectedCount() > 0)
	{
		auto position = irctrl->GetFirstSelectedItemPosition();
		const auto pos = irctrl->GetNextSelectedItem(position);
		const auto addr = irctrl->GetItemText(pos, 0);
		
		if (m_AccessManager->ResetAddressReputation(addr.GetString()).Succeeded())
		{
			UpdateAddressReputationList();
			UpdateControls();
		}
		else AfxMessageBox(L"Failed to reset address reputation!", MB_ICONERROR);
	}
}

void CAddressReputationsDlg::OnBnClickedRefresh()
{
	UpdateAddressReputationList();
	UpdateControls();
}
