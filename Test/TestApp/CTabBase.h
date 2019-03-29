// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

class CTabBase : public CDialogBase
{
public:
	CTabBase(UINT nIDTemplate, CWnd *pParent = NULL);
	virtual ~CTabBase();

	// Prevent dialog from closing when pressing Enter or Esc
	virtual void OnOK() { /* Do nothing */ };
	virtual void OnCancel() { /* Do nothing */ };

protected:
	DECLARE_MESSAGE_MAP()
};

