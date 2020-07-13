// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

#include "..\Socks5Extender\Socks5Extender.h"

#include "CTestAppDlgMainTab.h"
#include "CTestAppDlgTestExtenderTab.h"

#ifdef INCLUDE_AVEXTENDER
#include "CTestAppDlgAVExtenderTab.h"
#endif

#define WM_UPDATE_CONTROLS	(WM_USER+100)

// CTestAppDlg dialog
class CTestAppDlg final : public CDialogBase
{
// Construction
public:
	CTestAppDlg(CWnd* pParent = NULL);	// standard constructor

	void CreateRelayedConnection(const std::optional<PeerLUID>& gateway_pluid);

// Dialog Data
	enum { IDD = IDD_QGTESTAPP_DIALOG };

protected:
	void InitializeTabCtrl();
	void UpdateTabCtrl();

	void LoadSettings();
	void SaveSettings();
	
	void LoadSocks5Extender();
	void UnloadSocks5Extender();

	Set<UInt16> GetPorts(const CString ports);

	void UpdateControls();

	void OnPeerConnected(PeerLUID pluid, Result<ConnectDetails> result);

	void SetSecurityLevel(const QuantumGate::SecurityLevel level);

	bool GenerateGlobalSharedSecret(CString& passphrase, ProtectedBuffer& buffer) const noexcept;

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()

	LRESULT OnQGUpdateControls(WPARAM w, LPARAM l);

	// Generated message map functions
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnClose();
	afx_msg void OnLocalInitialize();
	afx_msg void OnUpdateLocalDeinitialize(CCmdUI* pCmdUI);
	afx_msg void OnLocalDeinitialize();
	afx_msg void OnUpdateLocalInitialize(CCmdUI* pCmdUI);
	afx_msg void OnLocalIPFilters();
	afx_msg void OnUpdateLocalIPFilters(CCmdUI* pCmdUI);
	afx_msg void OnSecuritylevelOne();
	afx_msg void OnSecuritylevelTwo();
	afx_msg void OnSecuritylevelThree();
	afx_msg void OnSecuritylevelFour();
	afx_msg void OnSecuritylevelFive();
	afx_msg void OnUpdateSecuritylevelOne(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSecuritylevelTwo(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSecuritylevelThree(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSecuritylevelFour(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSecuritylevelFive(CCmdUI* pCmdUI);
	afx_msg void OnBenchmarksDelegates();
	afx_msg void OnBenchmarksMutexes();
	afx_msg void OnAttacksConnectWithGarbage();
	afx_msg void OnUpdateAttacksConnectWithGarbage(CCmdUI* pCmdUI);
	afx_msg void OnLocalListenersEnabled();
	afx_msg void OnUpdateLocalListenersEnabled(CCmdUI* pCmdUI);
	afx_msg void OnLocalExtendersEnabled();
	afx_msg void OnUpdateLocalExtendersEnabled(CCmdUI* pCmdUI);
	afx_msg void OnBenchmarksThreadLocalCache();
	afx_msg void OnStressInitAndDeinitExtenders();
	afx_msg void OnUpdateStressInitAndDeinitExtenders(CCmdUI* pCmdUI);
	afx_msg void OnStressConnectAndDisconnect();
	afx_msg void OnUpdateStressConnectAndDisconnect(CCmdUI* pCmdUI);
	afx_msg void OnLocalCustomSecuritySettings();
	afx_msg void OnUpdateLocalCustomSecuritySettings(CCmdUI* pCmdUI);
	afx_msg void OnBenchmarksCompression();
	afx_msg void OnSocks5ExtenderLoad();
	afx_msg void OnUpdateSocks5ExtenderLoad(CCmdUI* pCmdUI);
	afx_msg void OnSocks5ExtenderAuthentication();
	afx_msg void OnUpdateSocks5ExtenderAuthentication(CCmdUI* pCmdUI);
	afx_msg void OnSocks5ExtenderAcceptIncomingConnections();
	afx_msg void OnUpdateSocks5ExtenderAcceptIncomingConnections(CCmdUI* pCmdUI);
	afx_msg void OnExtendersLoadFromModule();
	afx_msg void OnExtendersUnloadFromModule();
	afx_msg void OnSocks5ExtenderUseCompression();
	afx_msg void OnUpdateSocks5ExtenderUseCompression(CCmdUI* pCmdUI);
	afx_msg void OnLocalIpsubnetlimits();
	afx_msg void OnUtilsUUIDGenerationAndValidation();
	afx_msg void OnLocalAllowUnauthenticatedPeers();
	afx_msg void OnUpdateLocalAllowUnauthenticatedPeers(CCmdUI* pCmdUI);
	afx_msg void OnPeerAccessSettingsAdd();
	afx_msg void OnLocalRelaysEnabled();
	afx_msg void OnUpdateLocalRelaysEnabled(CCmdUI* pCmdUI);
	afx_msg void OnLocalConnect();
	afx_msg void OnUpdateLocalConnect(CCmdUI* pCmdUI);
	afx_msg void OnLocalConnectRelayed();
	afx_msg void OnUpdateLocalConnectRelayed(CCmdUI* pCmdUI);
	afx_msg void OnTcnSelchangeTabCtrl(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnLocalSupportedAlgorithms();
	afx_msg void OnUpdateLocalSupportedAlgorithms(CCmdUI* pCmdUI);
	afx_msg void OnSettingsGeneral();
	afx_msg void OnUpdateSettingsGeneral(CCmdUI* pCmdUI);
	afx_msg void OnBenchmarksConsole();
	afx_msg void OnStressMultipleInstances();
	afx_msg void OnUpdateStressMultipleInstances(CCmdUI* pCmdUI);
	afx_msg void OnBenchmarksMemory();
	afx_msg void OnUtilsLogAllocatorStatistics();
	afx_msg void OnLocalIPReputations();
	afx_msg void OnAttacksConnectAndDisconnect();
	afx_msg void OnAttacksConnectAndWait();
	afx_msg void OnUpdateAttacksConnectAndDisconnect(CCmdUI* pCmdUI);
	afx_msg void OnUpdateAttacksConnectAndWait(CCmdUI* pCmdUI);
	afx_msg void OnLocalEnvironmentInfo();
	afx_msg void OnUtilsPing();
	afx_msg void OnLocalFreeUnusedMemory();
	afx_msg void OnBenchmarksThreadPause();
	afx_msg void OnSocks5ExtenderConfiguration();
	afx_msg void OnUpdateSocks5ExtenderConfiguration(CCmdUI* pCmdUI);

protected:
	QuantumGate::StartupParameters m_StartupParameters;
	QuantumGate::Local m_QuantumGate;

	HICON m_hIcon{ 0 };

	CTestAppDlgMainTab m_MainTab{ m_QuantumGate };
	CTestAppDlgTestExtenderTab m_TestExtenderTab{ m_QuantumGate };

#ifdef INCLUDE_AVEXTENDER
	CTestAppDlgAVExtenderTab m_AVExtenderTab{ m_QuantumGate };
#endif

	std::atomic_bool m_ConnectStressThreadStop{ false };
	std::unique_ptr<std::thread> m_ConnectStressThread;

	String m_DefaultIP;
	UInt16 m_DefaultPort{ 999 };
};
