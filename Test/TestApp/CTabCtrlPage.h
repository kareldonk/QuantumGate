// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

class CTabCtrlPage : public CDialogBase
{
	DECLARE_DYNCREATE(CTabCtrlPage)

public:
	CTabCtrlPage(UINT nIDTemplate = 0, CWnd *pParent = NULL);
	virtual ~CTabCtrlPage();

	virtual void UpdateControls() noexcept {}

	virtual bool LoadData() noexcept { return true; }
	virtual bool SaveData() noexcept { return true; }

	// Prevent dialog from closing when pressing Enter or Esc
	virtual void OnOK() { /* Do nothing */ };
	virtual void OnCancel() { /* Do nothing */ };

protected:
	DECLARE_MESSAGE_MAP()
};

