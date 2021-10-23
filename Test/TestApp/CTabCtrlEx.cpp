// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "CTabCtrlEx.h"

IMPLEMENT_DYNAMIC(CTabCtrlEx, CTabCtrl)

BEGIN_MESSAGE_MAP(CTabCtrlEx, CTabCtrl)
	ON_NOTIFY_REFLECT(TCN_SELCHANGE, OnSelectionChange)
	ON_WM_SIZE()
	ON_WM_KEYDOWN()
END_MESSAGE_MAP()

bool CTabCtrlEx::AddPage(CRuntimeClass* rclass, const int dlgresid, std::wstring&& tabtitle) noexcept
{
	try
	{
		std::unique_ptr<CTabCtrlPage> tabwnd(dynamic_cast<CTabCtrlPage*>(rclass->CreateObject()));
		if (!tabwnd->IsKindOf(RUNTIME_CLASS(CTabCtrlPage)))
		{
			return false;
		}

		auto tabpage = std::make_unique<TabPage>();
		tabpage->DlgID = dlgresid;
		tabpage->TabTitle = std::move(tabtitle);
		tabpage->TabWnd = std::move(tabwnd);

		m_TabPages.push_back(std::move(tabpage));
		return true;
	}
	catch (...) {}

	return false;
}

bool CTabCtrlEx::Initialize() noexcept
{
	int idx = 0;

	for (auto& tabpage : m_TabPages)
	{
		const auto pos = InsertItem(idx, tabpage->TabTitle.c_str());
		if (pos != -1)
		{
			if (!tabpage->TabWnd->Create(tabpage->DlgID, this))
			{
				DeleteItem(pos);
				return false;
			}
		}
		else return false;

		++idx;
	}

	PositionTabPages();

	GetParent()->SetFocus();

	return true;
}

void CTabCtrlEx::PositionTabPages() noexcept
{
	CRect tabRect, itemRect;

	GetClientRect(&tabRect);
	GetItemRect(0, &itemRect);

	const auto nX = tabRect.left + 4;
	const auto nY = itemRect.bottom + 4;
	const auto nXc = tabRect.right - nX - 6;
	const auto nYc = tabRect.bottom - nY - 5;

	const auto cursel = GetCurSel();

	for (int idx = 0; idx < m_TabPages.size(); ++idx)
	{
		auto tabpage = m_TabPages[idx]->TabWnd.get();
		
		tabpage->SetWindowPos(nullptr, nX, nY, nXc, nYc, SWP_NOZORDER);
		
		if (cursel == idx) tabpage->ShowWindow(SW_SHOW);
		else tabpage->ShowWindow(SW_HIDE);
	}
}

CTabCtrlPage* CTabCtrlEx::GetTab(const CRuntimeClass* rclass) const noexcept
{
	for (auto& tabpage : m_TabPages)
	{
		if (tabpage->TabWnd->IsKindOf(rclass)) return tabpage->TabWnd.get();
	}

	return nullptr;
}

CTabCtrlPage* CTabCtrlEx::GetTab(const int idx) const noexcept
{
	if (idx >= 0 && idx < m_TabPages.size())
	{
		return m_TabPages[idx]->TabWnd.get();
	}

	return nullptr;
}

void CTabCtrlEx::OnSelectionChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	UpdateSelection();

	SetFocus();

	*pResult = 0;
}

int CTabCtrlEx::SetCurSel(const int idx) noexcept
{
	return CTabCtrl::SetCurSel(idx);
}

int CTabCtrlEx::SetCurSel(const CTabCtrlPage* obj) noexcept
{
	for (int idx = 0; idx < m_TabPages.size(); ++idx)
	{
		if (m_TabPages[idx]->TabWnd.get() == obj)
		{
			return SetCurSel(idx);
		}
	}

	return -1;
}

void CTabCtrlEx::UpdateSelection() noexcept
{
	const auto cursel = GetCurSel();
	if (cursel != -1)
	{
		for (int idx = 0; idx < m_TabPages.size(); ++idx)
		{
			CTabCtrlPage* tabpage = m_TabPages[idx]->TabWnd.get();
			if (cursel == idx) tabpage->ShowWindow(SW_SHOW);
			else tabpage->ShowWindow(SW_HIDE);
		}
	}
}

bool CTabCtrlEx::ForwardOnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// Don't send close system command to tab
	if (nID != 2 && nID != IDOK && nID != IDCANCEL)
	{
		for (auto& tabpage : m_TabPages)
		{
			if (tabpage->TabWnd->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) return true;
		}
	}

	return false;
}

BOOL CTabCtrlEx::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_TAB)
		{
			if (!m_TabPages.empty())
			{
				const auto cursel = GetCurSel();
				if (cursel != -1)
				{
					m_TabPages[cursel]->TabWnd->SetFocus();
					return TRUE;
				}
			}
		}
	}

	// Check first if tabs can handle the message
	for (auto& tabpage : m_TabPages)
	{
		if (tabpage->TabWnd->PreTranslateMessage(pMsg)) return TRUE;
	}

	return CTabCtrl::PreTranslateMessage(pMsg);
}

BOOL CTabCtrlEx::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.style |= WS_CLIPCHILDREN;

	return CTabCtrl::PreCreateWindow(cs);
}

bool CTabCtrlEx::LoadData() noexcept
{
	for (auto& tabpage : m_TabPages)
	{
		if (!tabpage->TabWnd->LoadData()) return false;
	}

	return true;
}

bool CTabCtrlEx::SaveData() noexcept
{
	for (auto& tabpage : m_TabPages)
	{
		if (!tabpage->TabWnd->SaveData()) return false;
	}

	return true;
}

void CTabCtrlEx::UpdateControls() noexcept
{
	for (auto& tabpage : m_TabPages)
	{
		tabpage->TabWnd->UpdateControls();
	}
}

void CTabCtrlEx::OnSize(UINT nType, int cx, int cy)
{
	CTabCtrl::OnSize(nType, cx, cy);

	PositionTabPages();
}
