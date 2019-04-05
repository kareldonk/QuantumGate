// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CTabBase.h"

#include "..\AVExtender\AVExtender.h"
#include "..\AVExtender\VideoSourceReader.h"
#include "..\AVExtender\VideoWindow.h"

class CTestAppDlgAVExtenderTab : public CTabBase
{
	DECLARE_DYNAMIC(CTestAppDlgAVExtenderTab)

public:
	CTestAppDlgAVExtenderTab(QuantumGate::Local& local, CWnd* pParent = nullptr);
	virtual ~CTestAppDlgAVExtenderTab();

	void UpdateControls() noexcept;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_QGTESTAPP_DIALOG_AVEXTENDER_TAB };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	LRESULT OnPeerEvent(WPARAM w, LPARAM l);
	LRESULT OnExtenderInit(WPARAM w, LPARAM l);
	LRESULT OnExtenderDeInit(WPARAM w, LPARAM l);

	void LoadAVExtender() noexcept;
	void UnloadAVExtender() noexcept;

	virtual BOOL OnInitDialog();

	afx_msg void OnBnClickedInitializeAv();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnAVExtenderLoad();
	afx_msg void OnAVExtenderUseCompression();
	afx_msg void OnUpdateAVExtenderLoad(CCmdUI* pCmdUI);
	afx_msg void OnUpdateAVExtenderUseCompression(CCmdUI* pCmdUI);

private:
	void UpdateVideoDeviceCombo() noexcept;

private:
	QuantumGate::Local& m_QuantumGate;

	std::shared_ptr<QuantumGate::AVExtender::Extender> m_AVExtender{ nullptr };

	QuantumGate::AVExtender::VideoSourceReader* m_VideoSourceReader{ nullptr };
	QuantumGate::AVExtender::VideoWindow m_VideoWindow;
	QuantumGate::AVExtender::VideoCaptureDeviceVector m_VideoCaptureDevices;
};
