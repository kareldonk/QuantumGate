// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CTabBase.h"

#include "..\TestExtender\TestExtender.h"
#include "..\StressExtender\StressExtender.h"

#define EXTENDER_PEER_ACTIVITY_TIMER	5

class CTestAppDlgTestExtenderTab final : public CTabBase
{
	DECLARE_DYNAMIC(CTestAppDlgTestExtenderTab)

public:
	CTestAppDlgTestExtenderTab(QuantumGate::Local& local, CWnd* pParent = nullptr);
	virtual ~CTestAppDlgTestExtenderTab();

	void UpdateControls() noexcept;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_QGTESTAPP_DIALOG_TESTEXTENDER_TAB };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	LRESULT OnPeerEvent(WPARAM w, LPARAM l);
	LRESULT OnPeerFileAccept(WPARAM w, LPARAM l);
	LRESULT OnExtenderInit(WPARAM w, LPARAM l);
	LRESULT OnExtenderDeInit(WPARAM w, LPARAM l);

	virtual BOOL OnInitDialog();

	void UpdateSelectedPeer() noexcept;
	void UpdatePeerActivity();
	void UpdateFileTransfers(const TestExtender::FileTransfers& filetransfers);
	const int GetFileTransferIndex(const TestExtender::FileTransferID id);

	void ProcessMessages();

	void LoadTestExtender();
	void UnloadTestExtender();

	void LoadStressExtender();
	void UnloadStressExtender();

	bool SendMsgToPeer(PeerLUID pluid, CString txt);
	void StartSendThread();
	void StopSendThread();
	static void SendThreadProc(CTestAppDlgTestExtenderTab* dlg, int interval, PeerLUID pluid, CString txt);

	inline void SetStressExtenderExceptionTest(bool* test) const noexcept { *test = !(*test); }
	void UpdateStressExtenderExceptionTest(CCmdUI* pCmdUI, const bool test) const noexcept;

	afx_msg void OnBnClickedSendbutton();
	afx_msg void OnBnClickedSendcheck();
	afx_msg void OnBnClickedSendfile();
	afx_msg void OnStressExtenderLoad();
	afx_msg void OnUpdateStressextenderLoad(CCmdUI* pCmdUI);
	afx_msg void OnStressExtenderUse();
	afx_msg void OnUpdateStressExtenderUse(CCmdUI* pCmdUI);
	afx_msg void OnStressextenderMessages();
	afx_msg void OnUpdateStressextenderMessages(CCmdUI* pCmdUI);
	afx_msg void OnTestExtenderLoad();
	afx_msg void OnUpdateTestExtenderLoad(CCmdUI* pCmdUI);
	afx_msg void OnTestExtenderUseCompression();
	afx_msg void OnUpdateTestExtenderUseCompression(CCmdUI* pCmdUI);
	afx_msg void OnStressExtenderUseCompression();
	afx_msg void OnUpdateStressExtenderUseCompression(CCmdUI* pCmdUI);
	afx_msg void OnBnClickedSendStress();
	afx_msg void OnLbnSelChangePeerList();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnExceptiontestStartup();
	afx_msg void OnUpdateExceptiontestStartup(CCmdUI* pCmdUI);
	afx_msg void OnExceptiontestPoststartup();
	afx_msg void OnUpdateExceptiontestPoststartup(CCmdUI* pCmdUI);
	afx_msg void OnExceptiontestPreshutdown();
	afx_msg void OnUpdateExceptiontestPreshutdown(CCmdUI* pCmdUI);
	afx_msg void OnExceptiontestShutdown();
	afx_msg void OnUpdateExceptiontestShutdown(CCmdUI* pCmdUI);
	afx_msg void OnExceptiontestPeerevent();
	afx_msg void OnUpdateExceptiontestPeerevent(CCmdUI* pCmdUI);
	afx_msg void OnExceptiontestPeermessage();
	afx_msg void OnUpdateExceptiontestPeermessage(CCmdUI* pCmdUI);
	afx_msg void OnBnClickedBrowse();
	afx_msg void OnBnClickedAutoSendfile();

private:
	QuantumGate::Local& m_QuantumGate;
	
	std::optional<PeerLUID> m_SelectedPeerLUID;
	UINT_PTR m_PeerActivityTimer{ 0 };

	std::shared_ptr<TestExtender::Extender> m_TestExtender{ nullptr };
	std::shared_ptr<StressExtender::Extender> m_StressExtender{ nullptr };
	bool m_UseStressExtender{ false };

	std::atomic_bool m_SendThreadStop{ false };
	std::unique_ptr<std::thread> m_SendThread;
};
