// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CTabBase.h"

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

	virtual BOOL OnInitDialog();

	afx_msg void OnBnClickedInitializeAv();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);

private:
	void UpdateVideoDeviceCombo() noexcept;

private:
	QuantumGate::Local& m_QuantumGate;

	QuantumGate::AVExtender::VideoSourceReader* m_VideoSourceReader{ nullptr };
	QuantumGate::AVExtender::VideoWindow m_VideoWindow;
	QuantumGate::AVExtender::VideoCaptureDeviceVector m_VideoCaptureDevices;
};
