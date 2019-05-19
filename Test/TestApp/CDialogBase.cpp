// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CDialogBase.h"

#include "Common\Util.h"

using namespace QuantumGate::Implementation;

CDialogBase::CDialogBase(UINT nIDTemplate, CWnd* pParent) : CDialogEx(nIDTemplate, pParent)
{}

CDialogBase::~CDialogBase()
{}

BEGIN_MESSAGE_MAP(CDialogBase, CDialogEx)
	ON_WM_INITMENUPOPUP()
END_MESSAGE_MAP()

Int64 CDialogBase::GetInt64Value(const int id, const Int64 def) const noexcept
{
	CString txt;
	if (GetDlgItemTextW(id, txt) != 0)
	{
		try
		{
			if (txt.GetLength() > 0) return std::stoull(Util::ToStringA((LPCWSTR)txt));
		}
		catch (...) {}
	}

	return def;
}

UInt64 CDialogBase::GetUInt64Value(const int id, const UInt64 def) const noexcept
{
	CString txt;
	if (GetDlgItemTextW(id, txt) != 0)
	{
		try
		{
			if (txt.GetLength() > 0) return std::stoull(Util::ToStringA((LPCWSTR)txt));
		}
		catch (...) {}
	}

	return def;
}

Size CDialogBase::GetSizeValue(const int id, const Size def) const noexcept
{
	return static_cast<Size>(GetUInt64Value(id, static_cast<UInt64>(def)));
}

CString CDialogBase::GetTextValue(const int id, const CString& def) const noexcept
{
	CString txt;
	if (GetDlgItemTextW(id, txt) != 0)
	{
		if (txt.GetLength() > 0) return txt;
	}

	return def;
}

bool CDialogBase::GetBoolValue(const int id, const bool def) noexcept
{
	const auto btn = (CButton*)GetDlgItem(id);
	if (btn)
	{
		if (btn->GetCheck() == BST_CHECKED)
		{
			return true;
		}
		else return false;
	}

	return def;
}

void CDialogBase::SetValue(const int id, const int val) noexcept
{
	SetValue(id, static_cast<Int64>(val));
}

void CDialogBase::SetValue(const int id, const Int64 val) noexcept
{
	SetDlgItemTextW(id, Util::FormatString(L"%lld", val).c_str());
}

void CDialogBase::SetValue(const int id, const UInt32 val) noexcept
{
	SetDlgItemTextW(id, Util::FormatString(L"%lu", val).c_str());
}

void CDialogBase::SetValue(const int id, const UInt64 val) noexcept
{
	SetDlgItemTextW(id, Util::FormatString(L"%llu", val).c_str());
}

void CDialogBase::SetValue(const int id, const CString& val) noexcept
{
	SetDlgItemTextW(id, val);
}

void CDialogBase::SetValue(const int id, const bool val) noexcept
{
	auto btn = (CButton*)GetDlgItem(id);
	if (btn)
	{
		btn->SetCheck(val ? BST_CHECKED : BST_UNCHECKED);
	}
}

void CDialogBase::SetValue(const int id, const std::chrono::seconds val) noexcept
{
	SetValue(id, static_cast<Size>(val.count()));
}

void CDialogBase::SetValue(const int id, const std::chrono::milliseconds val) noexcept
{
	SetValue(id, static_cast<Size>(val.count()));
}

void CDialogBase::SetValue(const int id, const std::wstring& val) noexcept
{
	SetValue(id, CString(val.c_str()));
}

void CDialogBase::SetValue(const int id, const std::string& val) noexcept
{
	SetValue(id, CString(Util::ToStringW(val).c_str()));
}

void CDialogBase::SetValue(const int id, const wchar_t * val) noexcept
{
	SetValue(id, CString(val));
}

BOOL CALLBACK ForwardMenuUIUpdateProc(HWND hwnd, LPARAM lParam)
{
	CCmdUI* state = (CCmdUI*)lParam;
	auto wnd = CWnd::FromHandle(hwnd);
	state->DoUpdate(wnd, FALSE);

	return TRUE;
}

// This is for the menu to handle UI updates
void CDialogBase::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
	ASSERT(pPopupMenu != NULL);
	// Check the enabled state of various menu items.

	CCmdUI state;
	state.m_pMenu = pPopupMenu;
	ASSERT(state.m_pOther == NULL);
	ASSERT(state.m_pParentMenu == NULL);

	// Determine if menu is popup in top-level menu and set m_pOther to
	// it if so (m_pParentMenu == NULL indicates that it is secondary popup).
	HMENU hParentMenu{ nullptr };
	if (AfxGetThreadState()->m_hTrackingMenu == pPopupMenu->m_hMenu)
	{
		state.m_pParentMenu = pPopupMenu;    // Parent == child for tracking popup.
	}
	else if ((hParentMenu = ::GetMenu(m_hWnd)) != NULL)
	{
		CWnd* pParent = this;
		// Child windows don't have menus--need to go to the top!
		if (pParent != NULL && (hParentMenu = ::GetMenu(pParent->m_hWnd)) != NULL)
		{
			const int nIndexMax = ::GetMenuItemCount(hParentMenu);
			for (int nIndex = 0; nIndex < nIndexMax; ++nIndex)
			{
				if (::GetSubMenu(hParentMenu, nIndex) == pPopupMenu->m_hMenu)
				{
					// When popup is found, m_pParentMenu is containing menu.
					state.m_pParentMenu = CMenu::FromHandle(hParentMenu);
					break;
				}
			}
		}
	}

	state.m_nIndexMax = pPopupMenu->GetMenuItemCount();
	for (state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax; ++state.m_nIndex)
	{
		state.m_nID = pPopupMenu->GetMenuItemID(state.m_nIndex);
		if (state.m_nID == 0)
		{
			continue; // Menu separator or invalid cmd - ignore it.
		}

		ASSERT(state.m_pOther == NULL);
		ASSERT(state.m_pMenu != NULL);

		if (state.m_nID == (UINT)-1)
		{
			// Possibly a popup menu, route to first item of that popup.
			state.m_pSubMenu = pPopupMenu->GetSubMenu(state.m_nIndex);
			if (state.m_pSubMenu == NULL ||
				(state.m_nID = state.m_pSubMenu->GetMenuItemID(0)) == 0 ||
				state.m_nID == (UINT)-1)
			{
				continue;       // First item of popup can't be routed to.
			}

			state.DoUpdate(this, TRUE);   // Popups are never auto disabled.

			// Forward to child windows
			EnumChildWindows(this->GetSafeHwnd(), ForwardMenuUIUpdateProc, (LPARAM)&state);
		}
		else
		{
			// Normal menu item.
			// Auto enable/disable if frame window has m_bAutoMenuEnable
			// set and command is _not_ a system command.
			state.m_pSubMenu = NULL;
			state.DoUpdate(this, FALSE);

			// Forward to child windows
			EnumChildWindows(this->GetSafeHwnd(), ForwardMenuUIUpdateProc, (LPARAM)&state);
		}

		// Adjust for menu deletions and additions.
		const UINT nCount = pPopupMenu->GetMenuItemCount();
		if (nCount < state.m_nIndexMax)
		{
			state.m_nIndex -= (state.m_nIndexMax - nCount);
			while (state.m_nIndex < nCount &&
				   pPopupMenu->GetMenuItemID(state.m_nIndex) == state.m_nID)
			{
				++state.m_nIndex;
			}
		}

		state.m_nIndexMax = nCount;
	}
}
