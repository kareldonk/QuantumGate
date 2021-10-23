// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CTabCtrlPage.h"

class CTabCtrlEx : public CTabCtrl
{
	DECLARE_DYNAMIC(CTabCtrlEx)

	struct TabPage final
	{
		int DlgID{ 0 };
		std::wstring TabTitle;
		bool HasCommands{ false };
		std::unique_ptr<CTabCtrlPage> TabWnd;
	};

	using TabPages = std::vector<std::unique_ptr<TabPage>>;

public:
	CTabCtrlEx() noexcept {}
	virtual ~CTabCtrlEx() {}

	bool AddPage(CRuntimeClass* rclass, const int dlgresid, std::wstring&& tabtitle) noexcept;
	bool Initialize() noexcept;
	CTabCtrlPage* GetTab(const CRuntimeClass* rclass) const noexcept;
	CTabCtrlPage* GetTab(const int idx) const noexcept;
	int SetCurSel(const int idx) noexcept;
	int SetCurSel(const CTabCtrlPage* obj) noexcept;

	template<typename F>
	void ForEachTab(F&& function) noexcept(noexcept(function(std::declval<CTabCtrlPage*>())))
	{
		for (auto& tab : m_TabPages)
		{
			function(tab->TabWnd.get());
		}
	}

	virtual void UpdateControls() noexcept;

	virtual bool LoadData() noexcept;
	virtual bool SaveData() noexcept;
	
	bool ForwardOnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

protected:
	DECLARE_MESSAGE_MAP()

	void PositionTabPages() noexcept;
	void UpdateSelection() noexcept;

	afx_msg void OnSelectionChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

private:
	TabPages m_TabPages;
};


