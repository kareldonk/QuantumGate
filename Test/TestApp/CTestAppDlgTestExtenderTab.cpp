// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestApp.h"
#include "CTestAppDlgTestExtenderTab.h"

#include <Console.h>
#include <Common\Util.h>
#include <Common\ScopeGuard.h>

using namespace QuantumGate::Implementation;

IMPLEMENT_DYNAMIC(CTestAppDlgTestExtenderTab, CTabBase)

CTestAppDlgTestExtenderTab::CTestAppDlgTestExtenderTab(QuantumGate::Local& local, CWnd* pParent /*=nullptr*/)
	: CTabBase(IDD_QGTESTAPP_DIALOG_TESTEXTENDER_TAB, pParent), m_QuantumGate(local)
{}

CTestAppDlgTestExtenderTab::~CTestAppDlgTestExtenderTab()
{}

void CTestAppDlgTestExtenderTab::UpdateControls() noexcept
{
	auto peerselected = false;
	const auto lbox = (CListBox*)GetDlgItem(IDC_PEERLIST);
	if (lbox->GetCurSel() != LB_ERR) peerselected = true;

	GetDlgItem(IDC_SENDTEXT)->EnableWindow(m_QuantumGate.IsRunning());
	GetDlgItem(IDC_SENDBUTTON)->EnableWindow(m_QuantumGate.IsRunning() && peerselected);
	GetDlgItem(IDC_SENDCHECK)->EnableWindow(m_QuantumGate.IsRunning() && (peerselected || m_SendThread != nullptr));
	GetDlgItem(IDC_SENDSECONDS)->EnableWindow(m_QuantumGate.IsRunning() && m_SendThread == nullptr);
	
	const auto ping_active = (m_TestExtender != nullptr ? m_TestExtender->IsPingActive() : false);

	GetDlgItem(IDC_PING)->EnableWindow(m_QuantumGate.IsRunning() && peerselected && !ping_active);
	GetDlgItem(IDC_PING_NUM_BYTES)->EnableWindow(m_QuantumGate.IsRunning() && !ping_active);

	GetDlgItem(IDC_SENDFILE)->EnableWindow(m_QuantumGate.IsRunning() && peerselected);
	GetDlgItem(IDC_AUTO_SENDFILE)->EnableWindow(m_QuantumGate.IsRunning() && peerselected);
	GetDlgItem(IDC_START_BENCHMARK)->EnableWindow(m_QuantumGate.IsRunning() && peerselected);

	GetDlgItem(IDC_SENDSTRESS)->EnableWindow(m_QuantumGate.IsRunning() && peerselected);
	GetDlgItem(IDC_NUMSTRESSMESS)->EnableWindow(m_QuantumGate.IsRunning());

	GetDlgItem(IDC_SEND_PRIORITY)->EnableWindow(m_QuantumGate.IsRunning() && peerselected);
	GetDlgItem(IDC_PRIORITY_COMBO)->EnableWindow(m_QuantumGate.IsRunning());
	GetDlgItem(IDC_SEND_DELAY)->EnableWindow(m_QuantumGate.IsRunning());
}

void CTestAppDlgTestExtenderTab::DoDataExchange(CDataExchange* pDX)
{
	CTabBase::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTestAppDlgTestExtenderTab, CTabBase)
	ON_MESSAGE(static_cast<UINT>(TestExtender::WindowsMessage::PeerEvent), &CTestAppDlgTestExtenderTab::OnPeerEvent)
	ON_MESSAGE(static_cast<UINT>(TestExtender::WindowsMessage::FileAccept), &CTestAppDlgTestExtenderTab::OnPeerFileAccept)
	ON_MESSAGE(static_cast<UINT>(TestExtender::WindowsMessage::ExtenderInit), &CTestAppDlgTestExtenderTab::OnExtenderInit)
	ON_MESSAGE(static_cast<UINT>(TestExtender::WindowsMessage::ExtenderDeinit), &CTestAppDlgTestExtenderTab::OnExtenderDeInit)
	ON_MESSAGE(static_cast<UINT>(TestExtender::WindowsMessage::PingResult), &CTestAppDlgTestExtenderTab::OnPingResult)
	ON_BN_CLICKED(IDC_SENDBUTTON, &CTestAppDlgTestExtenderTab::OnBnClickedSendbutton)
	ON_BN_CLICKED(IDC_SENDCHECK, &CTestAppDlgTestExtenderTab::OnBnClickedSendcheck)
	ON_BN_CLICKED(IDC_SENDFILE, &CTestAppDlgTestExtenderTab::OnBnClickedSendfile)
	ON_COMMAND(ID_STRESSEXTENDER_LOAD, &CTestAppDlgTestExtenderTab::OnStressExtenderLoad)
	ON_UPDATE_COMMAND_UI(ID_STRESSEXTENDER_LOAD, &CTestAppDlgTestExtenderTab::OnUpdateStressextenderLoad)
	ON_COMMAND(ID_STRESSEXTENDER_USE, &CTestAppDlgTestExtenderTab::OnStressExtenderUse)
	ON_UPDATE_COMMAND_UI(ID_STRESSEXTENDER_USE, &CTestAppDlgTestExtenderTab::OnUpdateStressExtenderUse)
	ON_COMMAND(ID_STRESSEXTENDER_MESSAGES, &CTestAppDlgTestExtenderTab::OnStressextenderMessages)
	ON_UPDATE_COMMAND_UI(ID_STRESSEXTENDER_MESSAGES, &CTestAppDlgTestExtenderTab::OnUpdateStressextenderMessages)
	ON_COMMAND(ID_TESTEXTENDER_LOAD, &CTestAppDlgTestExtenderTab::OnTestExtenderLoad)
	ON_UPDATE_COMMAND_UI(ID_TESTEXTENDER_LOAD, &CTestAppDlgTestExtenderTab::OnUpdateTestExtenderLoad)
	ON_COMMAND(ID_TESTEXTENDER_USECOMPRESSION, &CTestAppDlgTestExtenderTab::OnTestExtenderUseCompression)
	ON_UPDATE_COMMAND_UI(ID_TESTEXTENDER_USECOMPRESSION, &CTestAppDlgTestExtenderTab::OnUpdateTestExtenderUseCompression)
	ON_COMMAND(ID_STRESSEXTENDER_USECOMPRESSION, &CTestAppDlgTestExtenderTab::OnStressExtenderUseCompression)
	ON_UPDATE_COMMAND_UI(ID_STRESSEXTENDER_USECOMPRESSION, &CTestAppDlgTestExtenderTab::OnUpdateStressExtenderUseCompression)
	ON_BN_CLICKED(IDC_SENDSTRESS, &CTestAppDlgTestExtenderTab::OnBnClickedSendStress)
	ON_LBN_SELCHANGE(IDC_PEERLIST, &CTestAppDlgTestExtenderTab::OnLbnSelChangePeerList)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_COMMAND(ID_EXCEPTIONTEST_STARTUP, &CTestAppDlgTestExtenderTab::OnExceptiontestStartup)
	ON_UPDATE_COMMAND_UI(ID_EXCEPTIONTEST_STARTUP, &CTestAppDlgTestExtenderTab::OnUpdateExceptiontestStartup)
	ON_COMMAND(ID_EXCEPTIONTEST_POSTSTARTUP, &CTestAppDlgTestExtenderTab::OnExceptiontestPoststartup)
	ON_UPDATE_COMMAND_UI(ID_EXCEPTIONTEST_POSTSTARTUP, &CTestAppDlgTestExtenderTab::OnUpdateExceptiontestPoststartup)
	ON_COMMAND(ID_EXCEPTIONTEST_PRESHUTDOWN, &CTestAppDlgTestExtenderTab::OnExceptiontestPreshutdown)
	ON_UPDATE_COMMAND_UI(ID_EXCEPTIONTEST_PRESHUTDOWN, &CTestAppDlgTestExtenderTab::OnUpdateExceptiontestPreshutdown)
	ON_COMMAND(ID_EXCEPTIONTEST_SHUTDOWN, &CTestAppDlgTestExtenderTab::OnExceptiontestShutdown)
	ON_UPDATE_COMMAND_UI(ID_EXCEPTIONTEST_SHUTDOWN, &CTestAppDlgTestExtenderTab::OnUpdateExceptiontestShutdown)
	ON_COMMAND(ID_EXCEPTIONTEST_PEEREVENT, &CTestAppDlgTestExtenderTab::OnExceptiontestPeerevent)
	ON_UPDATE_COMMAND_UI(ID_EXCEPTIONTEST_PEEREVENT, &CTestAppDlgTestExtenderTab::OnUpdateExceptiontestPeerevent)
	ON_COMMAND(ID_EXCEPTIONTEST_PEERMESSAGE, &CTestAppDlgTestExtenderTab::OnExceptiontestPeermessage)
	ON_UPDATE_COMMAND_UI(ID_EXCEPTIONTEST_PEERMESSAGE, &CTestAppDlgTestExtenderTab::OnUpdateExceptiontestPeermessage)
	ON_BN_CLICKED(IDC_BROWSE, &CTestAppDlgTestExtenderTab::OnBnClickedBrowse)
	ON_BN_CLICKED(IDC_AUTO_SENDFILE, &CTestAppDlgTestExtenderTab::OnBnClickedAutoSendfile)
	ON_BN_CLICKED(IDC_SEND_PRIORITY, &CTestAppDlgTestExtenderTab::OnBnClickedSendPriority)
	ON_BN_CLICKED(IDC_START_BENCHMARK, &CTestAppDlgTestExtenderTab::OnBnClickedStartBenchmark)
	ON_BN_CLICKED(IDC_PING, &CTestAppDlgTestExtenderTab::OnBnClickedPing)
END_MESSAGE_MAP()

BOOL CTestAppDlgTestExtenderTab::OnInitDialog()
{
	CTabBase::OnInitDialog();

	SetValue(IDC_SENDTEXT, L"Hello world");
	SetValue(IDC_SENDSECONDS, L"10");
	SetValue(IDC_NUMSTRESSMESS, L"100000");
	SetValue(IDC_SEND_DELAY, L"2000");
	SetValue(IDC_PING_NUM_BYTES, L"32");
	SetValue(IDC_BENCHMARK_SIZE, L"100000000");

	auto lctrl = (CListCtrl*)GetDlgItem(IDC_FILETRANSFER_LIST);
	lctrl->SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
	lctrl->InsertColumn(0, _T("ID"), LVCFMT_LEFT, 0);
	lctrl->InsertColumn(1, _T("Filename"), LVCFMT_LEFT, GetApp()->GetScaledWidth(200));
	lctrl->InsertColumn(2, _T("Progress"), LVCFMT_LEFT, GetApp()->GetScaledWidth(75));
	lctrl->InsertColumn(3, _T("Status"), LVCFMT_LEFT, GetApp()->GetScaledWidth(100));

	// Init send priority combo
	const auto tcombo = (CComboBox*)GetDlgItem(IDC_PRIORITY_COMBO);
	auto pos = tcombo->AddString(L"Normal");
	tcombo->SetItemData(pos, static_cast<DWORD_PTR>(QuantumGate::SendParameters::PriorityOption::Normal));
	pos = tcombo->AddString(L"Delayed");
	tcombo->SetItemData(pos, static_cast<DWORD_PTR>(QuantumGate::SendParameters::PriorityOption::Delayed));
	pos = tcombo->AddString(L"Expedited");
	tcombo->SetItemData(pos, static_cast<DWORD_PTR>(QuantumGate::SendParameters::PriorityOption::Expedited));
	tcombo->SelectString(0, L"Normal");

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CTestAppDlgTestExtenderTab::UpdatePeerActivity()
{
	if (m_SelectedPeerLUID.has_value() && m_TestExtender != nullptr)
	{
		m_TestExtender->GetPeers()->IfSharedLock([&](const TestExtender::Peers& peers)
		{
			const auto peer = peers.find(*m_SelectedPeerLUID);
			if (peer != peers.end())
			{
				peer->second->FileTransfers.IfSharedLock([&](const TestExtender::FileTransfers& filetransfers)
				{
					UpdateFileTransfers(filetransfers);
				});
			}
		});
	}
	else
	{
		auto lctrl = (CListCtrl*)GetDlgItem(IDC_FILETRANSFER_LIST);
		lctrl->DeleteAllItems();
	}
}

void CTestAppDlgTestExtenderTab::UpdateFileTransfers(const TestExtender::FileTransfers& filetransfers)
{
	auto lctrl = (CListCtrl*)GetDlgItem(IDC_FILETRANSFER_LIST);

	for (auto& filetransfer : filetransfers)
	{
		const auto id = filetransfer.second->GetID();

		const auto perc = ((double)filetransfer.second->GetNumBytesTransferred() / (double)filetransfer.second->GetFileSize()) * 100.0;
		const auto percstr = Util::FormatString(L"%.2f%%", perc);
		const auto status = filetransfer.second->GetStatusString();

		const auto index = GetFileTransferIndex(id);
		if (index != -1)
		{
			lctrl->SetItemText(index, 2, percstr.c_str());
			lctrl->SetItemText(index, 3, status);
		}
		else
		{
			const auto idstr = Util::FormatString(L"%llu", id);

			const auto pos = lctrl->InsertItem(0, idstr.c_str());
			if (pos != -1)
			{
				lctrl->SetItemText(pos, 1, filetransfer.second->GetFileName().c_str());
				lctrl->SetItemText(pos, 2, percstr.c_str());
				lctrl->SetItemText(pos, 3, status);
			}
		}
	}

	for (int x = 0; x < lctrl->GetItemCount(); ++x)
	{
		wchar_t* end = nullptr;
		TestExtender::FileTransferID id = wcstoull(lctrl->GetItemText(x, 0), &end, 10);

		if (filetransfers.find(id) == filetransfers.end())
		{
			lctrl->DeleteItem(x);
			--x;
		}
	}
}

const int CTestAppDlgTestExtenderTab::GetFileTransferIndex(const TestExtender::FileTransferID id)
{
	const auto lctrl = (CListCtrl*)GetDlgItem(IDC_FILETRANSFER_LIST);

	for (int x = 0; x < lctrl->GetItemCount(); ++x)
	{
		wchar_t* end = nullptr;
		TestExtender::FileTransferID fid = wcstoull(lctrl->GetItemText(x, 0), &end, 10);

		if (id == fid) return x;
	}

	return -1;
}

void CTestAppDlgTestExtenderTab::LoadTestExtender()
{
	if (m_TestExtender == nullptr)
	{
		m_TestExtender = std::make_shared<TestExtender::Extender>(GetSafeHwnd());
		m_TestExtender->SetAutoFileTransferPath(GetApp()->GetFolder());
		auto extp = std::static_pointer_cast<Extender>(m_TestExtender);
		if (!m_QuantumGate.AddExtender(extp))
		{
			LogErr(L"Failed to add TestExtender");
			m_TestExtender.reset();
		}
	}
}

void CTestAppDlgTestExtenderTab::UnloadTestExtender()
{
	if (m_TestExtender != nullptr)
	{
		auto extp = std::static_pointer_cast<Extender>(m_TestExtender);
		if (!m_QuantumGate.RemoveExtender(extp))
		{
			LogErr(L"Failed to remove TestExtender");
		}
		else m_TestExtender.reset();
	}
}

void CTestAppDlgTestExtenderTab::LoadStressExtender()
{
	if (m_StressExtender == nullptr)
	{
		m_StressExtender = std::make_shared<StressExtender::Extender>();
		auto extp = std::static_pointer_cast<Extender>(m_StressExtender);
		if (!m_QuantumGate.AddExtender(extp))
		{
			LogErr(L"Failed to add StressExtender");
			m_StressExtender.reset();
		}
	}
}

void CTestAppDlgTestExtenderTab::UnloadStressExtender()
{
	if (m_StressExtender != nullptr)
	{
		m_UseStressExtender = false;

		auto extp = std::static_pointer_cast<Extender>(m_StressExtender);
		if (!m_QuantumGate.RemoveExtender(extp))
		{
			LogErr(L"Failed to remove StressExtender");
		}
		else m_StressExtender.reset();
	}
}

void CTestAppDlgTestExtenderTab::UpdateStressExtenderExceptionTest(CCmdUI* pCmdUI, const bool test) const noexcept
{
	pCmdUI->Enable(m_StressExtender != nullptr);
	pCmdUI->SetCheck(m_StressExtender != nullptr && test);
}

void CTestAppDlgTestExtenderTab::OnBnClickedSendbutton()
{
	SendMsgToPeer(*m_SelectedPeerLUID, GetTextValue(IDC_SENDTEXT).GetString(),
				  QuantumGate::SendParameters::PriorityOption::Normal, std::chrono::milliseconds(0));
}

void CTestAppDlgTestExtenderTab::OnBnClickedSendPriority()
{
	const auto combo = (CComboBox*)GetDlgItem(IDC_PRIORITY_COMBO);

	const auto sel = combo->GetCurSel();
	if (sel == CB_ERR)
	{
		AfxMessageBox(L"Please select a signing algorithm first.", MB_ICONINFORMATION);
		return;
	}

	const auto priority = static_cast<QuantumGate::SendParameters::PriorityOption>(combo->GetItemData(sel));

	const auto delay = GetTextValue(IDC_SEND_DELAY);
	int ndelay{ 0 };
	if (delay.GetLength() > 0)
	{
		ndelay = _wtoi((LPCWSTR)delay);
	}

	SendMsgToPeer(*m_SelectedPeerLUID, GetTextValue(IDC_SENDTEXT).GetString(), priority, std::chrono::milliseconds(ndelay));
}

void CTestAppDlgTestExtenderTab::OnBnClickedSendcheck()
{
	const CButton* check = (CButton*)GetDlgItem(IDC_SENDCHECK);
	if (check->GetCheck() == BST_CHECKED)
	{
		StartSendThread();
	}
	else StopSendThread();
}

void CTestAppDlgTestExtenderTab::StartSendThread()
{
	if (m_SendThread == nullptr)
	{
		m_SendThreadStop = false;

		const auto ms = static_cast<int>(GetInt64Value(IDC_SENDSECONDS));
		const auto txt = GetTextValue(IDC_SENDTEXT);

		m_SendThread = std::make_unique<std::thread>(CTestAppDlgTestExtenderTab::SendThreadProc, this, ms,
													 *m_SelectedPeerLUID, txt);

		const auto check = (CButton*)GetDlgItem(IDC_SENDCHECK);
		check->SetCheck(BST_CHECKED);

		UpdateControls();
	}
}

void CTestAppDlgTestExtenderTab::StopSendThread()
{
	if (m_SendThread != nullptr)
	{
		m_SendThreadStop = true;
		if (m_SendThread->joinable()) m_SendThread->join();
		m_SendThread.reset();

		auto check = (CButton*)GetDlgItem(IDC_SENDCHECK);
		check->SetCheck(BST_UNCHECKED);

		UpdateControls();
	}
}

void CTestAppDlgTestExtenderTab::SendThreadProc(CTestAppDlgTestExtenderTab* dlg, const int interval,
												const PeerLUID pluid, CString txt)
{
	while (!dlg->m_SendThreadStop)
	{
		dlg->SendMsgToPeer(pluid, txt.GetString(), QuantumGate::SendParameters::PriorityOption::Normal, std::chrono::milliseconds(0));

		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}
}

bool CTestAppDlgTestExtenderTab::SendMsgToPeer(const PeerLUID pluid, const String& txt,
											   const QuantumGate::SendParameters::PriorityOption priority,
											   const std::chrono::milliseconds delay)
{
	if (m_UseStressExtender) return m_StressExtender->SendMessage(pluid, txt, priority, delay);
	else if (m_TestExtender != nullptr) return m_TestExtender->SendMessage(pluid, txt, priority, delay);

	return false;
}

void CTestAppDlgTestExtenderTab::OnBnClickedSendfile()
{
	const auto path = GetApp()->BrowseForFile(GetSafeHwnd(), false);
	if (path)
	{
		CWaitCursor wait;

		m_TestExtender->SendFile(*m_SelectedPeerLUID, path->GetString(), false, false, 0);
	}
}

void CTestAppDlgTestExtenderTab::OnStressExtenderLoad()
{
	if (m_StressExtender == nullptr) LoadStressExtender();
	else UnloadStressExtender();
}

void CTestAppDlgTestExtenderTab::OnUpdateStressextenderLoad(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_StressExtender != nullptr);
}

void CTestAppDlgTestExtenderTab::OnStressExtenderUse()
{
	m_UseStressExtender = !m_UseStressExtender;
}

void CTestAppDlgTestExtenderTab::OnUpdateStressExtenderUse(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_StressExtender != nullptr);
	pCmdUI->SetCheck(m_UseStressExtender);
}


void CTestAppDlgTestExtenderTab::OnStressextenderMessages()
{
	if (m_SelectedPeerLUID.has_value()) m_StressExtender->BenchmarkSendMessage(*m_SelectedPeerLUID);
	else AfxMessageBox(L"Select a connected peer first from the list.", MB_ICONINFORMATION);
}

void CTestAppDlgTestExtenderTab::OnUpdateStressextenderMessages(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_QuantumGate.IsRunning() && m_StressExtender != nullptr);
}

void CTestAppDlgTestExtenderTab::OnStressExtenderUseCompression()
{
	if (m_StressExtender != nullptr)
	{
		m_StressExtender->SetUseCompression(!m_StressExtender->IsUsingCompression());
	}
}

void CTestAppDlgTestExtenderTab::OnUpdateStressExtenderUseCompression(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_StressExtender != nullptr);
	pCmdUI->SetCheck(m_StressExtender != nullptr && m_StressExtender->IsUsingCompression());
}

void CTestAppDlgTestExtenderTab::OnTestExtenderLoad()
{
	if (m_TestExtender == nullptr) LoadTestExtender();
	else UnloadTestExtender();
}

void CTestAppDlgTestExtenderTab::OnUpdateTestExtenderLoad(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_TestExtender != nullptr);
}

void CTestAppDlgTestExtenderTab::OnTestExtenderUseCompression()
{
	if (m_TestExtender != nullptr)
	{
		m_TestExtender->SetUseCompression(!m_TestExtender->IsUsingCompression());
	}
}

void CTestAppDlgTestExtenderTab::OnUpdateTestExtenderUseCompression(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_TestExtender != nullptr);
	pCmdUI->SetCheck(m_TestExtender != nullptr && m_TestExtender->IsUsingCompression());
}

void CTestAppDlgTestExtenderTab::ProcessMessages()
{
	MSG msg{ 0 };
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void CTestAppDlgTestExtenderTab::OnBnClickedSendStress()
{
	if (m_TestExtender == nullptr) return;

	CWaitCursor wait;

	const auto pluid = *m_SelectedPeerLUID;

	const auto txto = GetTextValue(IDC_SENDTEXT);
	const auto num = GetTextValue(IDC_NUMSTRESSMESS);

	SetValue(IDC_STRESSRESULT, L"--");

	const int nmess = _wtoi((LPCWSTR)num);

	const auto begin = std::chrono::high_resolution_clock::now();

	if (!m_TestExtender->SendBenchmarkStart(pluid)) return;

	String txt;

	for (int x = 0; x < nmess; ++x)
	{
		try
		{
			txt = Util::FormatString(L"%s #%d", txto.GetString(), x);

			if (!m_TestExtender->SendMessage(pluid, txt,
											 QuantumGate::SendParameters::PriorityOption::Normal,
											 std::chrono::milliseconds(0)))
			{
				LogErr(L"Could not send message %d to peer", x);
				break;
			}

			ProcessMessages();
		}
		catch (...)
		{
			AfxMessageBox(L"Exception thrown");
		}
	}

	m_TestExtender->SendBenchmarkEnd(pluid);

	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);

	SetValue(IDC_STRESSRESULT, Util::FormatString(L"%lldms", ms.count()));
}

LRESULT CTestAppDlgTestExtenderTab::OnPeerEvent(WPARAM w, LPARAM l)
{
	auto event = reinterpret_cast<TestExtender::Event*>(w);

	// Make sure we delete the event when we return
	const auto sg = MakeScopeGuard([&]() noexcept { delete event; });

	if (event->Type == QuantumGate::Extender::PeerEvent::Type::Connected)
	{
		LogInfo(L"Peer %llu connected", event->PeerLUID);

		auto lbox = (CListBox*)GetDlgItem(IDC_PEERLIST);
		lbox->InsertString(-1, Util::FormatString(L"%llu", event->PeerLUID).c_str());

		UpdateSelectedPeer();
		UpdateControls();
		UpdatePeerActivity();
	}
	else if (event->Type == QuantumGate::Extender::PeerEvent::Type::Disconnected)
	{
		LogInfo(L"Peer %llu disconnected", event->PeerLUID);

		CString pluid = Util::FormatString(L"%llu", event->PeerLUID).c_str();

		const auto lbox = (CListBox*)GetDlgItem(IDC_PEERLIST);
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

LRESULT CTestAppDlgTestExtenderTab::OnPeerFileAccept(WPARAM w, LPARAM l)
{
	auto fa = reinterpret_cast<TestExtender::FileAccept*>(w);
	const auto pluid = fa->PeerLUID;
	const auto ftid = fa->FileTransferID;

	// Delete allocated object from extender
	delete fa;

	const auto path = GetApp()->BrowseForFile(GetSafeHwnd(), true);

	CWaitCursor wait;

	if (path)
	{
		m_TestExtender->AcceptFile(pluid, ftid, path->GetString());
	}
	else m_TestExtender->AcceptFile(pluid, ftid, L"");

	return 0;
}

LRESULT CTestAppDlgTestExtenderTab::OnExtenderInit(WPARAM w, LPARAM l)
{
	m_PeerActivityTimer = SetTimer(EXTENDER_PEER_ACTIVITY_TIMER, 500, NULL);

	return 0;
}

LRESULT CTestAppDlgTestExtenderTab::OnExtenderDeInit(WPARAM w, LPARAM l)
{
	if (m_PeerActivityTimer != 0)
	{
		KillTimer(m_PeerActivityTimer);
		m_PeerActivityTimer = 0;
	}

	auto lbox = (CListBox*)GetDlgItem(IDC_PEERLIST);
	lbox->ResetContent();

	m_SelectedPeerLUID.reset();

	UpdateControls();
	UpdatePeerActivity();

	return 0;
}

LRESULT CTestAppDlgTestExtenderTab::OnPingResult(WPARAM w, LPARAM l)
{
	if (w == TRUE)
	{
		SetValue(IDC_PING_RESULT, Util::FormatString(L"%lldms", l));
	}
	else
	{
		SetValue(IDC_PING_RESULT, L"timed out");
	}

	UpdateControls();

	return 0;
}

void CTestAppDlgTestExtenderTab::UpdateSelectedPeer() noexcept
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

void CTestAppDlgTestExtenderTab::OnLbnSelChangePeerList()
{
	UpdateSelectedPeer();
	UpdateControls();
	UpdatePeerActivity();
}

void CTestAppDlgTestExtenderTab::OnTimer(UINT_PTR nIDEvent)
{
	if (IsWindowVisible())
	{
		if (nIDEvent == EXTENDER_PEER_ACTIVITY_TIMER)
		{
			UpdatePeerActivity();
		}
	}

	CTabBase::OnTimer(nIDEvent);
}

void CTestAppDlgTestExtenderTab::OnDestroy()
{
	StopSendThread();

	UnloadTestExtender();
	UnloadStressExtender();

	CTabBase::OnDestroy();
}

void CTestAppDlgTestExtenderTab::OnExceptiontestStartup()
{
	if (m_StressExtender != nullptr) SetStressExtenderExceptionTest(&m_StressExtender->GetExceptionTest().Startup);
}

void CTestAppDlgTestExtenderTab::OnUpdateExceptiontestStartup(CCmdUI* pCmdUI)
{
	UpdateStressExtenderExceptionTest(pCmdUI, m_StressExtender != nullptr ?
									  m_StressExtender->GetExceptionTest().Startup : false);
}

void CTestAppDlgTestExtenderTab::OnExceptiontestPoststartup()
{
	if (m_StressExtender != nullptr) SetStressExtenderExceptionTest(&m_StressExtender->GetExceptionTest().PostStartup);
}

void CTestAppDlgTestExtenderTab::OnUpdateExceptiontestPoststartup(CCmdUI* pCmdUI)
{
	UpdateStressExtenderExceptionTest(pCmdUI, m_StressExtender != nullptr ?
									  m_StressExtender->GetExceptionTest().PostStartup : false);
}

void CTestAppDlgTestExtenderTab::OnExceptiontestPreshutdown()
{
	if (m_StressExtender != nullptr) SetStressExtenderExceptionTest(&m_StressExtender->GetExceptionTest().PreShutdown);
}

void CTestAppDlgTestExtenderTab::OnUpdateExceptiontestPreshutdown(CCmdUI* pCmdUI)
{
	UpdateStressExtenderExceptionTest(pCmdUI, m_StressExtender != nullptr ?
									  m_StressExtender->GetExceptionTest().PreShutdown : false);
}

void CTestAppDlgTestExtenderTab::OnExceptiontestShutdown()
{
	if (m_StressExtender != nullptr) SetStressExtenderExceptionTest(&m_StressExtender->GetExceptionTest().Shutdown);
}

void CTestAppDlgTestExtenderTab::OnUpdateExceptiontestShutdown(CCmdUI* pCmdUI)
{
	UpdateStressExtenderExceptionTest(pCmdUI, m_StressExtender != nullptr ?
									  m_StressExtender->GetExceptionTest().Shutdown : false);
}

void CTestAppDlgTestExtenderTab::OnExceptiontestPeerevent()
{
	if (m_StressExtender != nullptr) SetStressExtenderExceptionTest(&m_StressExtender->GetExceptionTest().PeerEvent);
}

void CTestAppDlgTestExtenderTab::OnUpdateExceptiontestPeerevent(CCmdUI* pCmdUI)
{
	UpdateStressExtenderExceptionTest(pCmdUI, m_StressExtender != nullptr ?
									  m_StressExtender->GetExceptionTest().PeerEvent : false);
}

void CTestAppDlgTestExtenderTab::OnExceptiontestPeermessage()
{
	if (m_StressExtender != nullptr) SetStressExtenderExceptionTest(&m_StressExtender->GetExceptionTest().PeerMessage);
}

void CTestAppDlgTestExtenderTab::OnUpdateExceptiontestPeermessage(CCmdUI* pCmdUI)
{
	UpdateStressExtenderExceptionTest(pCmdUI, m_StressExtender != nullptr ?
									  m_StressExtender->GetExceptionTest().PeerMessage : false);
}

void CTestAppDlgTestExtenderTab::OnBnClickedBrowse()
{
	const auto path = GetApp()->BrowseForFile(GetSafeHwnd(), false);
	if (path)
	{
		SetValue(IDC_FILE_PATH, path->GetString());
	}
}

void CTestAppDlgTestExtenderTab::OnBnClickedAutoSendfile()
{
	const auto path = GetTextValue(IDC_FILE_PATH);
	if (path.GetLength() == 0)
	{
		AfxMessageBox(L"Please select a file first!");
		return;
	}

	if (!std::filesystem::exists(Path(path.GetString())))
	{
		AfxMessageBox(L"The file does not exist!");
		return;
	}

	// Disable button
	GetDlgItem(IDC_AUTO_SENDFILE)->EnableWindow(false);

	// Enable button when we return
	const auto sg = MakeScopeGuard([&]() noexcept { UpdateControls(); });

	CWaitCursor wait;

	m_TestExtender->SendFile(*m_SelectedPeerLUID, path.GetString(), true, false, 0);
}

void CTestAppDlgTestExtenderTab::OnBnClickedStartBenchmark()
{
	constexpr Size mins{ 2 << 9 };
	constexpr Size maxs{ 2 << 29 };

	const auto bsize = GetSizeValue(IDC_BENCHMARK_SIZE);
	if (bsize < mins || bsize > maxs)
	{
		AfxMessageBox(Util::FormatString(L"Specify a benchmark size between %zu and %zu.", mins, maxs).c_str());
		return;
	}

	// Disable button
	GetDlgItem(IDC_START_BENCHMARK)->EnableWindow(false);

	// Enable button when we return
	const auto sg = MakeScopeGuard([&]() noexcept { UpdateControls(); });

	CWaitCursor wait;

	m_TestExtender->SendFile(*m_SelectedPeerLUID, L"Benchmark", true, true, bsize);
}

void CTestAppDlgTestExtenderTab::OnBnClickedPing()
{
	constexpr Size mins{ 32 };
	const Size maxs{ m_TestExtender->GetMaxPingSize() };

	const auto psize = GetSizeValue(IDC_PING_NUM_BYTES);
	if (psize < mins || psize > maxs)
	{
		AfxMessageBox(Util::FormatString(L"Specify a ping size between %zu and %zu bytes.", mins, maxs).c_str());
		return;
	}

	CWaitCursor wait;
	
	if (m_TestExtender->Ping(*m_SelectedPeerLUID, psize, std::chrono::milliseconds(5000)))
	{
		SetValue(IDC_PING_RESULT, L"...");

		UpdateControls();
	}
}
