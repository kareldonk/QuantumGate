// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

class CIPFiltersDlg final : public CDialogBase
{
public:
	CIPFiltersDlg(CWnd* pParent = NULL);
	virtual ~CIPFiltersDlg();

	enum { IDD = IDD_IPFILTERS };

	void SetAccessManager(QuantumGate::AccessManager* am) noexcept;

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedRemovefilter();
	afx_msg void OnEnChangeIp();
	afx_msg void OnEnChangeMask();
	afx_msg void OnCbnSelChangeTypeCombo();
	afx_msg void OnBnClickedAddfilter();
	afx_msg void OnEnChangeTestIp();
	afx_msg void OnBnClickedTestButton();
	afx_msg void OnLvnItemchangedIpfiltersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMDblclkIpfiltersList(NMHDR* pNMHDR, LRESULT* pResult);

private:
	void UpdateIPFilterList() noexcept;
	void UpdateControls() noexcept;

	CString GetIPRange(const CString& ip, const CString& mask) const noexcept;
	void UpdateIPRange() noexcept;

private:
	QuantumGate::AccessManager* m_AccessManager{ nullptr };
};

