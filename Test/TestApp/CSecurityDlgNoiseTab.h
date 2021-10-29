// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CSecurityDlgTabCtrlPage.h"

class CSecurityDlgNoiseTab : public CSecurityDlgTabCtrlPage
{
	DECLARE_DYNCREATE(CSecurityDlgNoiseTab)

	struct NoiseBasedOnBandwidth
	{
		bool Use{ false };
		bool Saturate{ false };
		std::chrono::seconds TimeInterval{ 60 };
		Size MinimumBandwidth{ 100'000 };
		Size MaximumBandwidth{ 1'000'000 };
	};

public:
	CSecurityDlgNoiseTab(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CSecurityDlgNoiseTab();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SECURITY_SETTINGS_NOISE_TAB };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedNoiseAutoUse();
	afx_msg void OnEnChangeNoiseAutoSeconds();
	afx_msg void OnEnChangeNoiseAutoMinBandwidth();
	afx_msg void OnEnChangeNoiseAutoMaxBandwidth();
	afx_msg void OnBnClickedNoiseAutoSaturate();

private:
	bool LoadData() noexcept override;
	bool SaveData() noexcept override;
	void UpdateControls() noexcept override;
	void CalculateNoiseSettings() noexcept;

private:
	bool m_CanCalculateNoiseSettings{ false };
	inline static NoiseBasedOnBandwidth m_NoiseBasedOnBandwidth;
};
