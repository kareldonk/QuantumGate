// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CTabCtrlPage.h"

class CTestAppDlgTabCtrlPage : public CTabCtrlPage
{
	DECLARE_DYNCREATE(CTestAppDlgTabCtrlPage)

public:
	CTestAppDlgTabCtrlPage(UINT nIDTemplate = 0, CWnd* pParent = NULL);
	virtual ~CTestAppDlgTabCtrlPage();

	inline void SetQuantumGateInstance(QuantumGate::Local* local) noexcept { m_QuantumGate = local; }
	[[nodiscard]] inline QuantumGate::Local* GetQuantumGateInstance() noexcept { return m_QuantumGate; }

protected:
	DECLARE_MESSAGE_MAP()

private:
	QuantumGate::Local* m_QuantumGate{ nullptr };
};
