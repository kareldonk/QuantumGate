// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "QuantumGate.h"

using namespace QuantumGate;

class CDialogBase : public CDialogEx
{
public:
	CDialogBase(UINT nIDTemplate, CWnd *pParent = NULL);
	virtual ~CDialogBase();

	Int64 GetInt64Value(const int id, const Int64 def = 0) const noexcept;
	UInt64 GetUInt64Value(const int id, const UInt64 def = 0) const noexcept;
	Size GetSizeValue(const int id, const Size def = 0) const noexcept;
	CString GetTextValue(const int id, const CString& def = L"") const noexcept;
	bool GetBoolValue(const int id, const bool def = false) noexcept;

	void SetValue(const int id, const int val) noexcept;
	void SetValue(const int id, const Int64 val) noexcept;
	void SetValue(const int id, const UInt32 val) noexcept;
	void SetValue(const int id, const UInt64 val) noexcept;
	void SetValue(const int id, const CString& val) noexcept;
	void SetValue(const int id, const bool val) noexcept;
	void SetValue(const int id, const std::chrono::seconds val) noexcept;
	void SetValue(const int id, const std::chrono::milliseconds val) noexcept;
	void SetValue(const int id, const std::wstring& val) noexcept;
	void SetValue(const int id, const std::string& val) noexcept;
	void SetValue(const int id, const wchar_t* val) noexcept;

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
};

