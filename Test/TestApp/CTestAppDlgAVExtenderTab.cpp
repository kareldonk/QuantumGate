// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "TestApp.h"
#include "CTestAppDlgAVExtenderTab.h"

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
	ON_BN_CLICKED(IDC_INITIALIZE_AV, &CTestAppDlgAVExtenderTab::OnBnClickedInitializeAv)
	ON_WM_DESTROY()
	ON_WM_TIMER()
END_MESSAGE_MAP()

void CTestAppDlgAVExtenderTab::UpdateControls() noexcept
{}

BOOL CTestAppDlgAVExtenderTab::OnInitDialog()
{
	CTabBase::OnInitDialog();

	m_VideoSourceReader = new QuantumGate::AVExtender::VideoSourceReader();

	UpdateVideoDeviceCombo();
	
	RECT rect{ 0 };
	GetDlgItem(IDC_VIDEO_PREVIEW)->GetWindowRect(&rect);
	ScreenToClient(&rect);

	if (!m_VideoWindow.Create(NULL, WS_CHILD, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, GetSafeHwnd()))
	//if (!m_VideoWindow.Create(NULL, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
	//						  640, 480, GetSafeHwnd()))
	{

	}

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CTestAppDlgAVExtenderTab::UpdateVideoDeviceCombo() noexcept
{
	const auto vdcombo = (CComboBox*)GetDlgItem(IDC_VIDEO_DEVICES_COMBO);
	vdcombo->ResetContent();

	if (m_VideoSourceReader == nullptr) return;

	auto result = m_VideoSourceReader->EnumCaptureDevices();
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
	}
}

void CTestAppDlgAVExtenderTab::OnBnClickedInitializeAv()
{
	const auto vdcombo = (CComboBox*)GetDlgItem(IDC_VIDEO_DEVICES_COMBO);
	const auto sel = vdcombo->GetCurSel();
	if (sel != CB_ERR)
	{
		const auto idx = vdcombo->GetItemData(sel);
		const auto result = m_VideoSourceReader->Open(m_VideoCaptureDevices[idx]);
		if (result.Succeeded())
		{
			SetTimer(1, 100, NULL);
		}
		else
		{
			CString error = L"An error occured while trying to open the video capture device '";
			error += m_VideoCaptureDevices[idx].DeviceNameString;
			error += L"'.\r\n\r\n";
			error += result.GetErrorString().data();
			AfxMessageBox(error, MB_ICONERROR);
		}
	}
}

void CTestAppDlgAVExtenderTab::OnDestroy()
{
	KillTimer(1);

	if (m_VideoSourceReader)
	{
		m_VideoSourceReader->Close();
		m_VideoSourceReader->Release();
		m_VideoSourceReader = nullptr;
	}

	CTabBase::OnDestroy();
}

void CTestAppDlgAVExtenderTab::OnTimer(UINT_PTR nIDEvent)
{
	const auto dim = m_VideoSourceReader->GetSampleDimensions();

	QuantumGate::AVExtender::BGRAPixel* bgraBuffer = new QuantumGate::AVExtender::BGRAPixel[dim.first * dim.second];

	m_VideoSourceReader->GetSample(bgraBuffer);
	m_VideoWindow.Render(reinterpret_cast<Byte*>(bgraBuffer), dim.first, dim.second);

	delete bgraBuffer;

	CTabBase::OnTimer(nIDEvent);
}
