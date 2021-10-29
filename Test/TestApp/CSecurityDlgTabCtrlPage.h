// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CTabCtrlPage.h"

class CSecurityDlgTabCtrlPage : public CTabCtrlPage
{
	DECLARE_DYNCREATE(CSecurityDlgTabCtrlPage)

public:
	CSecurityDlgTabCtrlPage(UINT nIDTemplate = 0, CWnd* pParent = NULL);
	virtual ~CSecurityDlgTabCtrlPage();

	inline void SetQuantumGateInstance(QuantumGate::Local* local) noexcept { m_QuantumGate = local; }
	[[nodiscard]] inline QuantumGate::Local* GetQuantumGateInstance() noexcept { return m_QuantumGate; }

	inline void SetSecurityParameters(QuantumGate::SecurityParameters* params) noexcept { m_SecurityParameters = params; }
	[[nodiscard]] inline QuantumGate::SecurityParameters* GetSecurityParameters() noexcept { return m_SecurityParameters; }

protected:
	DECLARE_MESSAGE_MAP()

private:
	QuantumGate::Local* m_QuantumGate{ nullptr };
	QuantumGate::SecurityParameters* m_SecurityParameters{ nullptr };
};
