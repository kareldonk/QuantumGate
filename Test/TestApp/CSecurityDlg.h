// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

using namespace QuantumGate;

class CSecurityDlg final : public CDialogBase
{
	struct NoiseBasedOnBandwidth
	{
		bool Use{ false };
		bool Saturate{ false };
		std::chrono::seconds TimeInterval{ 60 };
		Size MinimumBandwidth{ 100'000 };
		Size MaximumBandwidth{ 1'000'000 };
	};

public:
	CSecurityDlg(CWnd* pParent = NULL);
	virtual ~CSecurityDlg();

	enum { IDD = IDD_SECURITY_SETTINGS };

	void SetQuantumGate(QuantumGate::Local* qg) noexcept { m_QuantumGate = qg; }

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	DECLARE_MESSAGE_MAP()
	
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedNoiseAutoUse();
	afx_msg void OnEnChangeNoiseAutoSeconds();
	afx_msg void OnEnChangeNoiseAutoMinBandwidth();
	afx_msg void OnEnChangeNoiseAutoMaxBandwidth();
	afx_msg void OnBnClickedNoiseAutoSaturate();

private:
	void UpdateNoiseControls() noexcept;
	void CalculateNoiseSettings() noexcept;

private:
	QuantumGate::Local* m_QuantumGate{ nullptr };

	bool m_CanCalculateNoiseSettings{ false };
	inline static NoiseBasedOnBandwidth m_NoiseBasedOnBandwidth;
};

