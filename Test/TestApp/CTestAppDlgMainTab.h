// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CTabBase.h"
#include "TestAppConsole.h"

#define CONSOLE_TIMER	1
#define PEER_ACTIVITY_TIMER	4

enum class ConsoleState { Disabled, Enabled, EnabledWindow };

class CTestAppDlgMainTab final : public CTabBase
{
	DECLARE_DYNAMIC(CTestAppDlgMainTab)

public:
	CTestAppDlgMainTab(QuantumGate::Local& local, CWnd* pParent = nullptr);
	virtual ~CTestAppDlgMainTab();

	void UpdateControls() noexcept;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_QGTESTAPP_DIALOG_MAIN_TAB };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	virtual BOOL OnInitDialog();

	int GetPeerIndex(const PeerLUID pluid);
	PeerLUID GetSelectedPeerLUID();
	void UpdatePeers();

	void UpdateConsole();
	void UpdateConsoleState();

	void LogPeerDetails(const QuantumGate::Peer& peer);

	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnPeerlistViewDetails();
	afx_msg void OnNMRClickAllPeersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMDblclkAllPeersList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPeerlistDisconnect();
	afx_msg void OnPeerlistCreateRelay();
	afx_msg void OnConsoleEnabled();
	afx_msg void OnUpdateConsoleEnabled(CCmdUI* pCmdUI);
	afx_msg void OnConsoleTerminalwindow();
	afx_msg void OnUpdateConsoleTerminalwindow(CCmdUI* pCmdUI);
	afx_msg void OnUpdateVerbositySilent(CCmdUI* pCmdUI);
	afx_msg void OnUpdateVerbosityMinimal(CCmdUI* pCmdUI);
	afx_msg void OnUpdateVerbosityNormal(CCmdUI* pCmdUI);
	afx_msg void OnUpdateVerbosityVerbose(CCmdUI* pCmdUI);
	afx_msg void OnUpdateVerbosityDebug(CCmdUI* pCmdUI);
	afx_msg void OnVerbositySilent();
	afx_msg void OnVerbosityMinimal();
	afx_msg void OnVerbosityNormal();
	afx_msg void OnVerbosityVerbose();
	afx_msg void OnVerbosityDebug();
	afx_msg void OnUpdatePeerlistViewDetails(CCmdUI* pCmdUI);
	afx_msg void OnUpdatePeerlistDisconnect(CCmdUI* pCmdUI);
	afx_msg void OnDestroy();
	afx_msg void OnBnClickedOnlyRelayedCheck();
	afx_msg void OnBnClickedOnlyAuthenticatedCheck();
	afx_msg void OnBnClickedExcludeInboundCheck();
	afx_msg void OnBnClickedExcludeOutboundCheck();
	afx_msg void OnBnClickedCreateUuid();
	afx_msg void OnBnClickedHasTestExtender();
	afx_msg void OnBnClickedHasStressExtender();
	afx_msg void OnUpdatePeerlistCreateRelay(CCmdUI* pCmdUI);

private:
	QuantumGate::Local& m_QuantumGate;
	CBrush m_ConsoleBrush;
	CFont m_ConsoleFont;

	Vector<PeerLUID> m_PeerLUIDs;
	PeerQueryParameters m_PeerQueryParams;

	UINT_PTR m_PeerActivityTimer{ 0 };

	UINT_PTR m_ConsoleTimer{ 0 };
	std::shared_ptr<TestAppConsole> m_Console;
	ConsoleState m_ConsoleState{ ConsoleState::Enabled };
};
