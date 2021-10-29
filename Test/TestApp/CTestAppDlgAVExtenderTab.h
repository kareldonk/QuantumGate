// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CTestAppDlgTabCtrlPage.h"

#include "..\AVExtender\AVExtender.h"
#include "..\AVExtender\VideoSourceReader.h"
#include "..\AVExtender\VideoRenderer.h"
#include "..\AVExtender\AudioSourceReader.h"
#include "..\AVExtender\AudioRenderer.h"

constexpr auto AVEXTENDER_PEER_ACTIVITY_TIMER = 10;

class CTestAppDlgAVExtenderTab : public CTestAppDlgTabCtrlPage
{
	DECLARE_DYNCREATE(CTestAppDlgAVExtenderTab)

public:
	CTestAppDlgAVExtenderTab() noexcept {}
	virtual ~CTestAppDlgAVExtenderTab() {}

	void UpdateControls() noexcept override;

	void OnPreDeinitializeQuantumGate() noexcept;

	// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_QGTESTAPP_DIALOG_AVEXTENDER_TAB };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	void StartAudioPreview() noexcept;
	void StopAudioPreview() noexcept;
	void StartVideoPreview() noexcept;
	void StopVideoPreview() noexcept;

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

	void OnAudioOutSample(const UInt64 timestamp, IMFSample* sample);
	void OnVideoOutSample(const UInt64 timestamp, IMFSample* sample);

	virtual BOOL OnInitDialog();

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
	afx_msg void OnBnClickedPreviewVideo();
	afx_msg void OnBnClickedPreviewAudio();
	afx_msg void OnCbnSelchangeVideoSizeCombo();
	afx_msg void OnBnClickedVideoCompressionCheck();
	afx_msg void OnBnClickedVideoFillCheck();
	afx_msg void OnBnClickedAudioCompressionCheck();
	afx_msg void OnBnClickedVideoSizeForce();

private:
	std::optional<PeerLUID> m_SelectedPeerLUID;
	UINT_PTR m_PeerActivityTimer{ 0 };

	std::shared_ptr<QuantumGate::AVExtender::Extender> m_AVExtender{ nullptr };

	QuantumGate::AVExtender::CaptureDeviceVector m_AudioCaptureDevices;
	QuantumGate::AVExtender::CaptureDeviceVector m_VideoCaptureDevices;

	QuantumGate::AVExtender::AudioRenderer_ThS m_AudioRenderer;
	QuantumGate::AVExtender::VideoRenderer m_VideoRenderer;
};
