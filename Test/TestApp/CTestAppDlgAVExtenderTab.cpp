// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "TestApp.h"
#include "CTestAppDlgAVExtenderTab.h"

#include <Console.h>
#include <Common\Util.h>
#include <Common\ScopeGuard.h>

using namespace QuantumGate::Implementation;

IMPLEMENT_DYNAMIC(CTestAppDlgAVExtenderTab, CTabBase)

CTestAppDlgAVExtenderTab::CTestAppDlgAVExtenderTab(QuantumGate::Local& local, CWnd* pParent /*=nullptr*/)
	: CTabBase(IDD_QGTESTAPP_DIALOG_AVEXTENDER_TAB, pParent), m_QuantumGate(local)
{}

CTestAppDlgAVExtenderTab::~CTestAppDlgAVExtenderTab()
{}

void CTestAppDlgAVExtenderTab::DoDataExchange(CDataExchange* pDX)
{
	CTabBase::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTestAppDlgAVExtenderTab, CTabBase)
	ON_MESSAGE(static_cast<UINT>(QuantumGate::AVExtender::WindowsMessage::PeerEvent), &CTestAppDlgAVExtenderTab::OnPeerEvent)
	ON_MESSAGE(static_cast<UINT>(QuantumGate::AVExtender::WindowsMessage::ExtenderInit), &CTestAppDlgAVExtenderTab::OnExtenderInit)
	ON_MESSAGE(static_cast<UINT>(QuantumGate::AVExtender::WindowsMessage::ExtenderDeinit), &CTestAppDlgAVExtenderTab::OnExtenderDeInit)
	ON_MESSAGE(static_cast<UINT>(QuantumGate::AVExtender::WindowsMessage::AcceptIncomingCall), &CTestAppDlgAVExtenderTab::OnAcceptIncomingCall)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_COMMAND(ID_AVEXTENDER_LOAD, &CTestAppDlgAVExtenderTab::OnAVExtenderLoad)
	ON_COMMAND(ID_AVEXTENDER_USECOMPRESSION, &CTestAppDlgAVExtenderTab::OnAVExtenderUseCompression)
	ON_UPDATE_COMMAND_UI(ID_AVEXTENDER_LOAD, &CTestAppDlgAVExtenderTab::OnUpdateAVExtenderLoad)
	ON_UPDATE_COMMAND_UI(ID_AVEXTENDER_USECOMPRESSION, &CTestAppDlgAVExtenderTab::OnUpdateAVExtenderUseCompression)
	ON_LBN_SELCHANGE(IDC_PEERLIST, &CTestAppDlgAVExtenderTab::OnLbnSelChangePeerList)
	ON_BN_CLICKED(IDC_SEND_VIDEO_CHECK, &CTestAppDlgAVExtenderTab::OnBnClickedSendVideoCheck)
	ON_BN_CLICKED(IDC_SEND_AUDIO_CHECK, &CTestAppDlgAVExtenderTab::OnBnClickedSendAudioCheck)
	ON_BN_CLICKED(IDC_CALL_BUTTON, &CTestAppDlgAVExtenderTab::OnBnClickedCallButton)
	ON_BN_CLICKED(IDC_HANGUP_BUTTON, &CTestAppDlgAVExtenderTab::OnBnClickedHangupButton)
	ON_CBN_SELCHANGE(IDC_AUDIO_DEVICES_COMBO, &CTestAppDlgAVExtenderTab::OnCbnSelChangeAudioDevicesCombo)
	ON_CBN_SELCHANGE(IDC_VIDEO_DEVICES_COMBO, &CTestAppDlgAVExtenderTab::OnCbnSelChangeVideoDevicesCombo)
	ON_BN_CLICKED(IDC_PREVIEW_VIDEO, &CTestAppDlgAVExtenderTab::OnBnClickedPreviewVideo)
	ON_BN_CLICKED(IDC_PREVIEW_AUDIO, &CTestAppDlgAVExtenderTab::OnBnClickedPreviewAudio)
	ON_CBN_SELCHANGE(IDC_VIDEO_SIZE_COMBO, &CTestAppDlgAVExtenderTab::OnCbnSelchangeVideoSizeCombo)
	ON_BN_CLICKED(IDC_VIDEO_COMPRESSION_CHECK, &CTestAppDlgAVExtenderTab::OnBnClickedVideoCompressionCheck)
	ON_BN_CLICKED(IDC_VIDEO_FILL_CHECK, &CTestAppDlgAVExtenderTab::OnBnClickedVideoFillCheck)
	ON_BN_CLICKED(IDC_AUDIO_COMPRESSION_CHECK, &CTestAppDlgAVExtenderTab::OnBnClickedAudioCompressionCheck)
END_MESSAGE_MAP()

void CTestAppDlgAVExtenderTab::UpdateControls() noexcept
{
	GetDlgItem(IDC_PREVIEW_VIDEO)->EnableWindow(m_AVExtender != nullptr);
	GetDlgItem(IDC_PREVIEW_AUDIO)->EnableWindow(m_AVExtender != nullptr);

	GetDlgItem(IDC_VIDEO_COMPRESSION_CHECK)->EnableWindow(m_AVExtender != nullptr);
	GetDlgItem(IDC_AUDIO_COMPRESSION_CHECK)->EnableWindow(m_AVExtender != nullptr);
	GetDlgItem(IDC_VIDEO_FILL_CHECK)->EnableWindow(m_AVExtender != nullptr);

	const auto compress_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_VIDEO_COMPRESSION_CHECK));
	const auto fill_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_VIDEO_FILL_CHECK));
	const auto compress_audio_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_AUDIO_COMPRESSION_CHECK));

	if (m_AVExtender != nullptr)
	{
		compress_video_check->SetCheck(m_AVExtender->IsUsingVideoCompression() ? BST_CHECKED : BST_UNCHECKED);
		compress_audio_check->SetCheck(m_AVExtender->IsUsingAudioCompression() ? BST_CHECKED : BST_UNCHECKED);
		fill_video_check->SetCheck(m_AVExtender->GetFillVideoScreen() ? BST_CHECKED : BST_UNCHECKED);
	}
	else
	{
		compress_video_check->SetCheck(BST_UNCHECKED);
		compress_audio_check->SetCheck(BST_UNCHECKED);
		fill_video_check->SetCheck(BST_UNCHECKED);
	}
}

BOOL CTestAppDlgAVExtenderTab::OnInitDialog()
{
	CTabBase::OnInitDialog();

	UpdateVideoDeviceCombo();
	UpdateAudioDeviceCombo();

	RECT rect{ 0 };
	GetDlgItem(IDC_VIDEO_PREVIEW)->GetWindowRect(&rect);
	ScreenToClient(&rect);

	if (!m_VideoRenderer.Create(L"Preview", NULL, WS_CHILD, rect.left, rect.top,
								rect.right - rect.left, rect.bottom - rect.top, true, GetSafeHwnd()))
	{
		AfxMessageBox(L"Failed to create video preview window.", MB_ICONERROR);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CTestAppDlgAVExtenderTab::StartAudioPreview() noexcept
{
	auto audiocb = QuantumGate::MakeCallback(this, &CTestAppDlgAVExtenderTab::OnAudioOutSample);
	auto result = m_AVExtender->StartAudioPreview(std::move(audiocb));
	if (result.Succeeded())
	{
		auto audio_renderer = m_AudioRenderer.WithUniqueLock();

		if (audio_renderer->Create(result.GetValue()))
		{
			if (audio_renderer->Play())
			{
				auto preview_audio_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_PREVIEW_AUDIO));
				preview_audio_check->SetCheck(BST_CHECKED);
			}
			else
			{
				audio_renderer->Close();
			}
		}
	}
}

void CTestAppDlgAVExtenderTab::StopAudioPreview() noexcept
{
	m_AVExtender->StopAudioPreview();

	m_AudioRenderer.WithUniqueLock()->Close();

	auto preview_audio_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_PREVIEW_AUDIO));
	preview_audio_check->SetCheck(BST_UNCHECKED);
}

void CTestAppDlgAVExtenderTab::StartVideoPreview() noexcept
{
	auto videocb = QuantumGate::MakeCallback(this, &CTestAppDlgAVExtenderTab::OnVideoOutSample);
	auto result = m_AVExtender->StartVideoPreview(std::move(videocb));
	if (result.Succeeded())
	{
		if (m_VideoRenderer.SetInputFormat(result.GetValue()))
		{
			auto preview_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_PREVIEW_VIDEO));
			preview_video_check->SetCheck(BST_CHECKED);
		}
		else
		{
			AfxMessageBox(L"An error occured while trying to set the input format for the video window.", MB_ICONERROR);
		}
	}
}

void CTestAppDlgAVExtenderTab::StopVideoPreview() noexcept
{
	m_AVExtender->StopVideoPreview();

	m_VideoRenderer.Redraw();

	auto preview_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_PREVIEW_VIDEO));
	preview_video_check->SetCheck(BST_UNCHECKED);
}

void CTestAppDlgAVExtenderTab::UpdateVideoDeviceCombo() noexcept
{
	const auto vdcombo = (CComboBox*)GetDlgItem(IDC_VIDEO_DEVICES_COMBO);
	vdcombo->ResetContent();

	const auto vdscombo = (CComboBox*)GetDlgItem(IDC_VIDEO_SIZE_COMBO);
	vdscombo->ResetContent();

	auto result = AVExtender::CaptureDevices::Enum(AVExtender::CaptureDevice::Type::Video);
	if (result.Succeeded())
	{
		m_VideoCaptureDevices = std::move(*result);

		for (auto x = 0u; x < m_VideoCaptureDevices.size(); ++x)
		{
			const auto pos = vdcombo->AddString(m_VideoCaptureDevices[x].DeviceNameString);
			vdcombo->SetItemData(pos, static_cast<DWORD_PTR>(x));
		}

		if (vdcombo->GetCount() > 0)
		{
			vdcombo->SelectString(0, m_VideoCaptureDevices[0].DeviceNameString);
		}

		int size{ 1088 };
		while (size >= 80)
		{
			const auto pos = vdscombo->AddString(Util::FormatString(L"%dp", size).c_str());
			vdscombo->SetItemData(pos, static_cast<DWORD_PTR>(size));

			size = static_cast<int>(static_cast<float>(size)* (2.f/3.f));
			size = size - (size % 16);
		}

		vdscombo->SelectString(0, L"80p");
	}

	UpdateAVVideoDevice();
}

void CTestAppDlgAVExtenderTab::UpdateAudioDeviceCombo() noexcept
{
	const auto vdcombo = (CComboBox*)GetDlgItem(IDC_AUDIO_DEVICES_COMBO);
	vdcombo->ResetContent();

	auto result = AVExtender::CaptureDevices::Enum(AVExtender::CaptureDevice::Type::Audio);
	if (result.Succeeded())
	{
		m_AudioCaptureDevices = std::move(*result);

		for (auto x = 0u; x < m_AudioCaptureDevices.size(); ++x)
		{
			const auto pos = vdcombo->AddString(m_AudioCaptureDevices[x].DeviceNameString);
			vdcombo->SetItemData(pos, static_cast<DWORD_PTR>(x));
		}

		if (vdcombo->GetCount() > 0)
		{
			vdcombo->SelectString(0, m_AudioCaptureDevices[0].DeviceNameString);
		}

		UpdateAVAudioDevice();
	}
}

void CTestAppDlgAVExtenderTab::OnCbnSelChangeAudioDevicesCombo()
{
	UpdateAVAudioDevice();
}

void CTestAppDlgAVExtenderTab::OnCbnSelChangeVideoDevicesCombo()
{
	UpdateAVVideoDevice();
}

void CTestAppDlgAVExtenderTab::OnCbnSelchangeVideoSizeCombo()
{
	UpdateAVVideoDevice();
}

void CTestAppDlgAVExtenderTab::UpdateAVAudioDevice() noexcept
{
	if (m_AVExtender != nullptr)
	{
		const auto vdcombo = (CComboBox*)GetDlgItem(IDC_AUDIO_DEVICES_COMBO);
		const auto sel = vdcombo->GetCurSel();
		if (sel != CB_ERR)
		{
			const auto preview_audio_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_PREVIEW_AUDIO));
			const auto preview_audio = (preview_audio_check->GetCheck() == BST_CHECKED);

			if (preview_audio) StopAudioPreview();

			const auto idx = vdcombo->GetItemData(sel);
			const auto success = m_AVExtender->SetAudioEndpointID(m_AudioCaptureDevices[idx].EndpointID);

			if (success && preview_audio) StartAudioPreview();
		}
		else
		{
			DiscardReturnValue(m_AVExtender->SetAudioEndpointID(L""));
		}
	}
}

void CTestAppDlgAVExtenderTab::UpdateAVVideoDevice() noexcept
{
	if (m_AVExtender != nullptr)
	{
		const auto vdcombo = (CComboBox*)GetDlgItem(IDC_VIDEO_DEVICES_COMBO);
		const auto sel = vdcombo->GetCurSel();
		if (sel != CB_ERR)
		{
			const auto vdscombo = (CComboBox*)GetDlgItem(IDC_VIDEO_SIZE_COMBO);
			const auto sel2 = vdscombo->GetCurSel();
			if (sel2 != CB_ERR)
			{
				const auto preview_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_PREVIEW_VIDEO));
				const auto preview_video = (preview_video_check->GetCheck() == BST_CHECKED);

				if (preview_video) StopVideoPreview();

				const auto idx = vdcombo->GetItemData(sel);
				const auto size = vdscombo->GetItemData(sel2);

				const auto success = m_AVExtender->SetVideoSymbolicLink(m_VideoCaptureDevices[idx].SymbolicLink,
																		static_cast<UInt16>(size));

				if (success && preview_video) StartVideoPreview();
			}
		}
		else
		{
			DiscardReturnValue(m_AVExtender->SetVideoSymbolicLink(L"", 0));
		}
	}
}

void CTestAppDlgAVExtenderTab::OnBnClickedPreviewVideo()
{
	if (m_AVExtender != nullptr)
	{
		auto preview_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_PREVIEW_VIDEO));
		if (preview_video_check->GetCheck() == BST_CHECKED)
		{
			StartVideoPreview();
		}
		else
		{
			StopVideoPreview();
		}
	}
}

void CTestAppDlgAVExtenderTab::OnBnClickedPreviewAudio()
{
	if (m_AVExtender != nullptr)
	{
		auto preview_audio_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_PREVIEW_AUDIO));
		if (preview_audio_check->GetCheck() == BST_CHECKED)
		{
			StartAudioPreview();
		}
		else
		{
			StopAudioPreview();
		}
	}
}

void CTestAppDlgAVExtenderTab::OnVideoOutSample(const UInt64 timestamp, IMFSample* sample)
{
	DiscardReturnValue(m_VideoRenderer.Render(sample));
}

void CTestAppDlgAVExtenderTab::OnAudioOutSample(const UInt64 timestamp, IMFSample* sample)
{
	auto audio_renderer = m_AudioRenderer.WithUniqueLock();

	if (!audio_renderer->IsOpen()) return;

	DiscardReturnValue(audio_renderer->Render(sample));
}

void CTestAppDlgAVExtenderTab::OnDestroy()
{
	UnloadAVExtender();

	m_VideoRenderer.Close();

	CTabBase::OnDestroy();
}

void CTestAppDlgAVExtenderTab::OnTimer(UINT_PTR nIDEvent)
{
	if (IsWindowVisible())
	{
		if (nIDEvent == AVEXTENDER_PEER_ACTIVITY_TIMER)
		{
			UpdatePeerActivity();
		}
	}

	CTabBase::OnTimer(nIDEvent);
}

LRESULT CTestAppDlgAVExtenderTab::OnPeerEvent(WPARAM w, LPARAM l)
{
	auto event = reinterpret_cast<AVExtender::Event*>(w);

	// Make sure we delete the event when we return
	const auto sg = MakeScopeGuard([&]() noexcept { delete event; });

	if (event->Type == PeerEventType::Connected)
	{
		auto lbox = reinterpret_cast<CListBox*>(GetDlgItem(IDC_PEERLIST));
		lbox->InsertString(-1, Util::FormatString(L"%llu", event->PeerLUID).c_str());

		UpdateSelectedPeer();
		UpdateControls();
		UpdatePeerActivity();
	}
	else if (event->Type == PeerEventType::Disconnected)
	{
		CString pluid = Util::FormatString(L"%llu", event->PeerLUID).c_str();

		const auto lbox = reinterpret_cast<CListBox*>(GetDlgItem(IDC_PEERLIST));
		const auto pos = lbox->FindString(-1, pluid);
		if (pos != LB_ERR) lbox->DeleteString(pos);

		UpdateSelectedPeer();
		UpdateControls();
		UpdatePeerActivity();
	}
	else
	{
		LogWarn(L"Unhandled peer event from %llu: %d", event->PeerLUID, event->Type);
	}

	return 0;
}

LRESULT CTestAppDlgAVExtenderTab::OnExtenderInit(WPARAM w, LPARAM l)
{
	m_PeerActivityTimer = SetTimer(AVEXTENDER_PEER_ACTIVITY_TIMER, 500, NULL);

	return 0;
}

LRESULT CTestAppDlgAVExtenderTab::OnExtenderDeInit(WPARAM w, LPARAM l)
{
	if (m_PeerActivityTimer != 0)
	{
		KillTimer(m_PeerActivityTimer);
		m_PeerActivityTimer = 0;
	}

	auto lbox = reinterpret_cast<CListBox*>(GetDlgItem(IDC_PEERLIST));
	lbox->ResetContent();

	m_SelectedPeerLUID.reset();

	UpdateControls();
	UpdatePeerActivity();

	return 0;
}

LRESULT CTestAppDlgAVExtenderTab::OnAcceptIncomingCall(WPARAM w, LPARAM l)
{
	auto ca = reinterpret_cast<AVExtender::CallAccept*>(w);
	const auto pluid = ca->PeerLUID;

	// Delete allocated object from extender
	delete ca;

	const auto retval = AfxMessageBox(Util::FormatString(L"Do you want to accept an incoming call from peer %llu?", pluid).c_str(),
									  MB_ICONQUESTION | MB_YESNO);
	if (retval == IDYES)
	{
		if (!m_AVExtender->AcceptCall(pluid))
		{
			AfxMessageBox(L"Failed to accept call.", MB_ICONERROR);
		}
	}
	else
	{
		if (!m_AVExtender->DeclineCall(pluid))
		{
			AfxMessageBox(L"Failed to decline call.", MB_ICONERROR);
		}
	}

	return 0;
}

void CTestAppDlgAVExtenderTab::OnAVExtenderLoad()
{
	if (m_AVExtender == nullptr) LoadAVExtender();
	else UnloadAVExtender();

	UpdateControls();
}

void CTestAppDlgAVExtenderTab::OnUpdateAVExtenderLoad(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_AVExtender != nullptr);
}

void CTestAppDlgAVExtenderTab::OnAVExtenderUseCompression()
{
	if (m_AVExtender != nullptr)
	{
		m_AVExtender->SetUseCompression(!m_AVExtender->IsUsingCompression());
	}
}

void CTestAppDlgAVExtenderTab::OnUpdateAVExtenderUseCompression(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_AVExtender != nullptr);
	pCmdUI->SetCheck(m_AVExtender != nullptr && m_AVExtender->IsUsingCompression());
}

void CTestAppDlgAVExtenderTab::LoadAVExtender() noexcept
{
	if (m_AVExtender == nullptr)
	{
		try
		{
			m_AVExtender = std::make_shared<QuantumGate::AVExtender::Extender>(GetSafeHwnd());
			auto extp = std::static_pointer_cast<Extender>(m_AVExtender);
			if (!m_QuantumGate.AddExtender(extp))
			{
				LogErr(L"Failed to add AVExtender");
				m_AVExtender.reset();
			}

			UpdateControls();

			UpdateAVAudioDevice();
			UpdateAVVideoDevice();
		}
		catch (...)
		{
			LogErr(L"Failed to add AVExtender due to exception");
		}
	}
}

void CTestAppDlgAVExtenderTab::UnloadAVExtender() noexcept
{
	if (m_AVExtender != nullptr)
	{
		StopVideoPreview();
		StopAudioPreview();

		if (m_AVExtender->IsRunning()) m_AVExtender->HangupAllCalls();

		m_AVExtender->StopAVSourceReaders();

		auto extp = std::static_pointer_cast<Extender>(m_AVExtender);
		if (!m_QuantumGate.RemoveExtender(extp))
		{
			LogErr(L"Failed to remove AVExtender");
		}
		else m_AVExtender.reset();

		UpdateControls();
	}
}

void CTestAppDlgAVExtenderTab::UpdateCallInformation(const QuantumGate::AVExtender::Call* call) noexcept
{
	auto send_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_SEND_VIDEO_CHECK));
	auto send_audio_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_SEND_AUDIO_CHECK));

	if (call != nullptr)
	{
		SetValue(IDC_CALL_STATUS, call->GetStatusString());
		SetValue(IDC_CALL_DURATION,
				 Util::FormatString(L"%llu seconds",
									std::chrono::duration_cast<std::chrono::seconds>(call->GetDuration()).count()));

		GetDlgItem(IDC_CALL_BUTTON)->EnableWindow(m_QuantumGate.IsRunning() && call->IsDisconnected());
		GetDlgItem(IDC_HANGUP_BUTTON)->EnableWindow(m_QuantumGate.IsRunning() && !call->IsDisconnected());

		send_video_check->EnableWindow(m_QuantumGate.IsRunning());
		if (call->GetSendVideo())
		{
			send_video_check->SetCheck(BST_CHECKED);
		}
		else send_video_check->SetCheck(BST_UNCHECKED);

		send_audio_check->EnableWindow(m_QuantumGate.IsRunning());
		if (call->GetSendAudio())
		{
			send_audio_check->SetCheck(BST_CHECKED);
		}
		else send_audio_check->SetCheck(BST_UNCHECKED);
	}
	else
	{
		SetValue(IDC_CALL_STATUS, L"Unknown");
		SetValue(IDC_CALL_DURATION, L"Unknown");

		GetDlgItem(IDC_CALL_BUTTON)->EnableWindow(false);
		GetDlgItem(IDC_HANGUP_BUTTON)->EnableWindow(false);
		send_video_check->EnableWindow(false);
		send_video_check->SetCheck(BST_UNCHECKED);
		send_audio_check->EnableWindow(false);
		send_audio_check->SetCheck(BST_UNCHECKED);
	}
}

void CTestAppDlgAVExtenderTab::UpdatePeerActivity() noexcept
{
	if (m_SelectedPeerLUID.has_value() && m_AVExtender != nullptr)
	{
		m_AVExtender->GetPeers().IfSharedLock([&](const AVExtender::Peers& peers)
		{
			const auto peer = peers.find(*m_SelectedPeerLUID);
			if (peer != peers.end())
			{
				peer->second->Call->WithSharedLock([&](const AVExtender::Call& call)
				{
					UpdateCallInformation(&call);
				});
			}
		});
	}
	else
	{
		UpdateCallInformation(nullptr);
	}
}

void CTestAppDlgAVExtenderTab::UpdateSelectedPeer() noexcept
{
	m_SelectedPeerLUID.reset();

	const auto lbox = reinterpret_cast<CListBox*>(GetDlgItem(IDC_PEERLIST));
	const auto cursel = lbox->GetCurSel();
	if (cursel != LB_ERR)
	{
		CString pluidtxt;
		lbox->GetText(cursel, pluidtxt);
		if (pluidtxt.GetLength() != 0)
		{
			wchar_t* end = nullptr;
			m_SelectedPeerLUID = wcstoull(pluidtxt, &end, 10);
		}
	}
}

void CTestAppDlgAVExtenderTab::OnLbnSelChangePeerList()
{
	UpdateSelectedPeer();
	UpdateControls();
	UpdatePeerActivity();
}

void CTestAppDlgAVExtenderTab::OnBnClickedSendVideoCheck()
{
	const auto send_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_SEND_VIDEO_CHECK));
	const auto send_video = (send_video_check->GetCheck() == BST_CHECKED);

	if (m_SelectedPeerLUID.has_value() && m_AVExtender != nullptr)
	{
		m_AVExtender->UpdateSendVideo(*m_SelectedPeerLUID, send_video);
	}
}

void CTestAppDlgAVExtenderTab::OnBnClickedSendAudioCheck()
{
	const auto send_audio_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_SEND_AUDIO_CHECK));
	const auto send_audio = (send_audio_check->GetCheck() == BST_CHECKED);

	if (m_SelectedPeerLUID.has_value() && m_AVExtender != nullptr)
	{
		m_AVExtender->UpdateSendAudio(*m_SelectedPeerLUID, send_audio);
	}
}

void CTestAppDlgAVExtenderTab::OnBnClickedCallButton()
{
	if (m_AVExtender != nullptr && m_SelectedPeerLUID.has_value())
	{
		const auto send_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_SEND_VIDEO_CHECK));
		const auto send_video = (send_video_check->GetCheck() == BST_CHECKED);

		const auto send_audio_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_SEND_AUDIO_CHECK));
		const auto send_audio = (send_audio_check->GetCheck() == BST_CHECKED);

		if (!m_AVExtender->BeginCall(*m_SelectedPeerLUID, send_video, send_audio))
		{
			AfxMessageBox(L"Failed to call peer.", MB_ICONERROR);
		}
	}
}

void CTestAppDlgAVExtenderTab::OnBnClickedHangupButton()
{
	if (m_AVExtender != nullptr && m_SelectedPeerLUID.has_value())
	{
		if (!m_AVExtender->HangupCall(*m_SelectedPeerLUID))
		{
			AfxMessageBox(L"Failed to hangup call.", MB_ICONERROR);
		}
	}
}

void CTestAppDlgAVExtenderTab::OnPreDeinitializeQuantumGate() noexcept
{
	if (m_AVExtender != nullptr)
	{
		m_AVExtender->HangupAllCalls();
	}
}

void CTestAppDlgAVExtenderTab::OnBnClickedVideoCompressionCheck()
{
	if (m_AVExtender != nullptr)
	{
		const auto compress_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_VIDEO_COMPRESSION_CHECK));
		const auto compress_video = (compress_video_check->GetCheck() == BST_CHECKED);

		m_AVExtender->SetUseVideoCompression(compress_video);
	}
}

void CTestAppDlgAVExtenderTab::OnBnClickedVideoFillCheck()
{
	if (m_AVExtender != nullptr)
	{
		const auto fill_video_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_VIDEO_FILL_CHECK));
		const auto fill_video = (fill_video_check->GetCheck() == BST_CHECKED);

		m_AVExtender->SetFillVideoScreen(fill_video);

		m_VideoRenderer.SetRenderSize(fill_video ? AVExtender::VideoRenderer::RenderSize::Cover :
									  AVExtender::VideoRenderer::RenderSize::Fit);
	}
}

void CTestAppDlgAVExtenderTab::OnBnClickedAudioCompressionCheck()
{
	if (m_AVExtender != nullptr)
	{
		const auto compress_audio_check = reinterpret_cast<CButton*>(GetDlgItem(IDC_AUDIO_COMPRESSION_CHECK));
		const auto compress_audio = (compress_audio_check->GetCheck() == BST_CHECKED);

		m_AVExtender->SetUseAudioCompression(compress_audio);
	}
}
