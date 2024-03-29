// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

class CAddressReputationsDlg final : public CDialogBase
{
	DECLARE_DYNAMIC(CAddressReputationsDlg)

public:
	CAddressReputationsDlg(CWnd* pParent = nullptr);
	virtual ~CAddressReputationsDlg();

	void SetAccessManager(QuantumGate::Access::Manager* am) noexcept;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ADDRESS_REPUTATIONS_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	virtual BOOL OnInitDialog();
	afx_msg void OnLvnItemChangedAddressReputationsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEnChangeAddress();
	afx_msg void OnEnChangeReputation();
	afx_msg void OnBnClickedSetReputation();
	afx_msg void OnBnClickedResetAll();
	afx_msg void OnBnClickedResetSelected();
	afx_msg void OnBnClickedRefresh();

private:
	void UpdateAddressReputationList() noexcept;
	void UpdateControls() noexcept;

private:
	QuantumGate::Access::Manager* m_AccessManager{ nullptr };
};
