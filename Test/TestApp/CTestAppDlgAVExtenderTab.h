// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CTabBase.h"

#include "..\AVExtender\AVExtender.h"
#include "..\AVExtender\VideoSourceReader.h"
#include "..\AVExtender\VideoWindow.h"
#include "..\AVExtender\AudioSourceReader.h"
#include "..\AVExtender\AudioRenderer.h"

constexpr auto AVEXTENDER_PEER_ACTIVITY_TIMER = 10;

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
	LRESULT OnAcceptIncomingCall(WPARAM w, LPARAM l);

	void LoadAVExtender() noexcept;
	void UnloadAVExtender() noexcept;
	void UpdateVideoDeviceCombo() noexcept;
	void UpdateAudioDeviceCombo() noexcept;
	void UpdateCallInformation(const QuantumGate::AVExtender::Call* call) noexcept;
	void UpdatePeerActivity() noexcept;
	void UpdateSelectedPeer() noexcept;

	void UpdateAVAudioDevice() noexcept;
	void UpdateAVVideoDevice() noexcept;

	void OnAudioSample(const UInt64 timestamp, IMFSample* sample);
	void OnVideoSample(const UInt64 timestamp, IMFSample* sample);

	virtual BOOL OnInitDialog();

	afx_msg void OnBnClickedInitializeAv();
	afx_msg void OnBnClickedInitializeAudio();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnAVExtenderLoad();
	afx_msg void OnAVExtenderUseCompression();
	afx_msg void OnUpdateAVExtenderLoad(CCmdUI* pCmdUI);
	afx_msg void OnUpdateAVExtenderUseCompression(CCmdUI* pCmdUI);
	afx_msg void OnLbnSelChangePeerList();
	afx_msg void OnBnClickedSendVideoCheck();
	afx_msg void OnBnClickedSendAudioCheck();
	afx_msg void OnBnClickedCallButton();
	afx_msg void OnBnClickedHangupButton();
	afx_msg void OnCbnSelChangeAudioDevicesCombo();
	afx_msg void OnCbnSelChangeVideoDevicesCombo();

private:
	QuantumGate::Local& m_QuantumGate;

	std::optional<PeerLUID> m_SelectedPeerLUID;
	UINT_PTR m_PeerActivityTimer{ 0 };

	std::shared_ptr<QuantumGate::AVExtender::Extender> m_AVExtender{ nullptr };

	QuantumGate::AVExtender::VideoSourceReader* m_VideoSourceReader{ nullptr };
	QuantumGate::AVExtender::VideoWindow m_VideoWindow;
	QuantumGate::AVExtender::CaptureDeviceVector m_VideoCaptureDevices;

	QuantumGate::AVExtender::AudioSourceReader* m_AudioSourceReader{ nullptr };
	QuantumGate::AVExtender::CaptureDeviceVector m_AudioCaptureDevices;
	QuantumGate::AVExtender::AudioRenderer_ThS m_AudioRenderer;
};
