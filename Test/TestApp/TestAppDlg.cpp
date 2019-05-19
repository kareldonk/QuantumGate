// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "TestApp.h"
#include "TestAppDlg.h"

#include "Common\Util.h"
#include "Crypto\Crypto.h"
#include "Common\Endian.h"
#include "Network\Ping.h"

#include "Benchmarks.h"
#include "Attacks.h"
#include "Stress.h"

#include "CEndpointDlg.h"
#include "CIPFiltersDlg.h"
#include "CIPSubnetLimitsDlg.h"
#include "CIPReputationsDlg.h"
#include "CSecurityDlg.h"
#include "CAuthenticationDlg.h"
#include "CUUIDDialog.h"
#include "CPeerAccessDlg.h"
#include "CAlgorithmsDlg.h"
#include "CSettingsDlg.h"
#include "CInformationDlg.h"
#include "CPingDlg.h"

using namespace nlohmann;
using namespace QuantumGate::Implementation;

// CTestAppDlg dialog

CTestAppDlg::CTestAppDlg(CWnd* pParent) : CDialogBase(CTestAppDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_StartupParameters.SupportedAlgorithms.Hash = { Algorithm::Hash::BLAKE2S256,
		Algorithm::Hash::BLAKE2B512, Algorithm::Hash::SHA256, Algorithm::Hash::SHA512, };

	m_StartupParameters.SupportedAlgorithms.PrimaryAsymmetric = {
		Algorithm::Asymmetric::ECDH_X25519, Algorithm::Asymmetric::ECDH_SECP521R1 };

	m_StartupParameters.SupportedAlgorithms.SecondaryAsymmetric = {
		Algorithm::Asymmetric::KEM_NEWHOPE, Algorithm::Asymmetric::KEM_NTRUPRIME };

	m_StartupParameters.SupportedAlgorithms.Symmetric = { Algorithm::Symmetric::CHACHA20_POLY1305,
		Algorithm::Symmetric::AES256_GCM };

	m_StartupParameters.SupportedAlgorithms.Compression = { Algorithm::Compression::ZSTANDARD,
		Algorithm::Compression::DEFLATE };
}

void CTestAppDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogBase::DoDataExchange(pDX);
}

Set<UInt16> CTestAppDlg::GetPorts(const CString ports)
{
	Set<UInt16> portsv;
	int pos{ 0 };
	int start{ 0 };

	while ((pos = ports.Find(L";", start)) != -1)
	{
		const CString port = ports.Mid(start, pos - start);
		const auto portn = static_cast<UInt16>(_wtoi((LPCWSTR)port));

		if (portn > 0) portsv.emplace(portn);

		start = pos + 1;
	}

	CString end = ports.Mid(start, ports.GetLength() - start);
	if (end.GetLength() > 0)
	{
		const auto portn = static_cast<UInt16>(_wtoi((LPCWSTR)end));
		if (portn > 0) portsv.emplace(portn);
	}

	return portsv;
}

void CTestAppDlg::UpdateControls()
{
	m_MainTab.UpdateControls();
	m_TestExtenderTab.UpdateControls();
	m_AVExtenderTab.UpdateControls();
}

BEGIN_MESSAGE_MAP(CTestAppDlg, CDialogBase)
	ON_MESSAGE(WM_UPDATE_CONTROLS, &CTestAppDlg::OnQGUpdateControls)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_HSCROLL()
	ON_WM_CLOSE()
	ON_COMMAND(ID_LOCAL_INITIALIZE, &CTestAppDlg::OnLocalInitialize)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_DEINITIALIZE, &CTestAppDlg::OnUpdateLocalDeinitialize)
	ON_COMMAND(ID_LOCAL_DEINITIALIZE, &CTestAppDlg::OnLocalDeinitialize)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_INITIALIZE, &CTestAppDlg::OnUpdateLocalInitialize)
	ON_COMMAND(ID_LOCAL_IPFILTERS, &CTestAppDlg::OnLocalIPFilters)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_IPFILTERS, &CTestAppDlg::OnUpdateLocalIPFilters)
	ON_COMMAND(ID_SECURITYLEVEL_ONE, &CTestAppDlg::OnSecuritylevelOne)
	ON_COMMAND(ID_SECURITYLEVEL_TWO, &CTestAppDlg::OnSecuritylevelTwo)
	ON_COMMAND(ID_SECURITYLEVEL_THREE, &CTestAppDlg::OnSecuritylevelThree)
	ON_COMMAND(ID_SECURITYLEVEL_FOUR, &CTestAppDlg::OnSecuritylevelFour)
	ON_COMMAND(ID_SECURITYLEVEL_FIVE, &CTestAppDlg::OnSecuritylevelFive)
	ON_UPDATE_COMMAND_UI(ID_SECURITYLEVEL_ONE, &CTestAppDlg::OnUpdateSecuritylevelOne)
	ON_UPDATE_COMMAND_UI(ID_SECURITYLEVEL_TWO, &CTestAppDlg::OnUpdateSecuritylevelTwo)
	ON_UPDATE_COMMAND_UI(ID_SECURITYLEVEL_THREE, &CTestAppDlg::OnUpdateSecuritylevelThree)
	ON_UPDATE_COMMAND_UI(ID_SECURITYLEVEL_FOUR, &CTestAppDlg::OnUpdateSecuritylevelFour)
	ON_UPDATE_COMMAND_UI(ID_SECURITYLEVEL_FIVE, &CTestAppDlg::OnUpdateSecuritylevelFive)
	ON_COMMAND(ID_BENCHMARKS_CALLBACKS, &CTestAppDlg::OnBenchmarksDelegates)
	ON_COMMAND(ID_BENCHMARKS_MUTEXES, &CTestAppDlg::OnBenchmarksMutexes)
	ON_COMMAND(ID_ATTACKS_CONNECTWITHGARBAGE, &CTestAppDlg::OnAttacksConnectWithGarbage)
	ON_UPDATE_COMMAND_UI(ID_ATTACKS_CONNECTWITHGARBAGE, &CTestAppDlg::OnUpdateAttacksConnectWithGarbage)
	ON_COMMAND(ID_LOCAL_LISTENERSENABLED, &CTestAppDlg::OnLocalListenersEnabled)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_LISTENERSENABLED, &CTestAppDlg::OnUpdateLocalListenersEnabled)
	ON_COMMAND(ID_LOCAL_EXTENDERSENABLED, &CTestAppDlg::OnLocalExtendersEnabled)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_EXTENDERSENABLED, &CTestAppDlg::OnUpdateLocalExtendersEnabled)
	ON_COMMAND(ID_BENCHMARKS_THREADLOCALCACHE, &CTestAppDlg::OnBenchmarksThreadLocalCache)
	ON_COMMAND(ID_STRESS_INITANDDEINITEXTENDERS, &CTestAppDlg::OnStressInitAndDeinitExtenders)
	ON_UPDATE_COMMAND_UI(ID_STRESS_INITANDDEINITEXTENDERS, &CTestAppDlg::OnUpdateStressInitAndDeinitExtenders)
	ON_COMMAND(ID_STRESS_CONNECTANDDISCONNECT, &CTestAppDlg::OnStressConnectAndDisconnect)
	ON_UPDATE_COMMAND_UI(ID_STRESS_CONNECTANDDISCONNECT, &CTestAppDlg::OnUpdateStressConnectAndDisconnect)
	ON_COMMAND(ID_LOCAL_CUSTOMSECURITYSETTINGS, &CTestAppDlg::OnLocalCustomSecuritySettings)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_CUSTOMSECURITYSETTINGS, &CTestAppDlg::OnUpdateLocalCustomSecuritySettings)
	ON_COMMAND(ID_BENCHMARKS_COMPRESSION, &CTestAppDlg::OnBenchmarksCompression)
	ON_COMMAND(ID_SOCKS5EXTENDER_LOAD, &CTestAppDlg::OnSocks5ExtenderLoad)
	ON_UPDATE_COMMAND_UI(ID_SOCKS5EXTENDER_LOAD, &CTestAppDlg::OnUpdateSocks5ExtenderLoad)
	ON_COMMAND(ID_SOCKS5EXTENDER_AUTHENTICATION, &CTestAppDlg::OnSocks5ExtenderAuthentication)
	ON_UPDATE_COMMAND_UI(ID_SOCKS5EXTENDER_AUTHENTICATION, &CTestAppDlg::OnUpdateSocks5ExtenderAuthentication)
	ON_COMMAND(ID_SOCKS5EXTENDER_ACCEPTINCOMINGCONNECTIONS, &CTestAppDlg::OnSocks5ExtenderAcceptIncomingConnections)
	ON_UPDATE_COMMAND_UI(ID_SOCKS5EXTENDER_ACCEPTINCOMINGCONNECTIONS, &CTestAppDlg::OnUpdateSocks5ExtenderAcceptIncomingConnections)
	ON_COMMAND(ID_EXTENDERS_LOADFROMMODULE, &CTestAppDlg::OnExtendersLoadFromModule)
	ON_COMMAND(ID_EXTENDERS_UNLOADFROMMODULE, &CTestAppDlg::OnExtendersUnloadFromModule)
	ON_COMMAND(ID_SOCKS5EXTENDER_USECOMPRESSION, &CTestAppDlg::OnSocks5ExtenderUseCompression)
	ON_UPDATE_COMMAND_UI(ID_SOCKS5EXTENDER_USECOMPRESSION, &CTestAppDlg::OnUpdateSocks5ExtenderUseCompression)
	ON_COMMAND(ID_LOCAL_IPSUBNETLIMITS, &CTestAppDlg::OnLocalIpsubnetlimits)
	ON_COMMAND(ID_UTILS_UUIDGENERATIONANDVALIDATION, &CTestAppDlg::OnUtilsUUIDGenerationAndValidation)
	ON_COMMAND(ID_LOCAL_ALLOWUNAUTHENTICATEDPEERS, &CTestAppDlg::OnLocalAllowUnauthenticatedPeers)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_ALLOWUNAUTHENTICATEDPEERS, &CTestAppDlg::OnUpdateLocalAllowUnauthenticatedPeers)
	ON_COMMAND(ID_PEERACCESSSETTINGS_ADD, &CTestAppDlg::OnPeerAccessSettingsAdd)
	ON_COMMAND(ID_LOCAL_RELAYS_ENABLED, &CTestAppDlg::OnLocalRelaysEnabled)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_RELAYS_ENABLED, &CTestAppDlg::OnUpdateLocalRelaysEnabled)
	ON_COMMAND(ID_LOCAL_CONNECT, &CTestAppDlg::OnLocalConnect)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_CONNECT, &CTestAppDlg::OnUpdateLocalConnect)
	ON_COMMAND(ID_LOCAL_CONNECT_RELAYED, &CTestAppDlg::OnLocalConnectRelayed)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_CONNECT_RELAYED, &CTestAppDlg::OnUpdateLocalConnectRelayed)
	ON_WM_CTLCOLOR()
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_CTRL, &CTestAppDlg::OnTcnSelchangeTabCtrl)
	ON_WM_SHOWWINDOW()
	ON_COMMAND(ID_LOCAL_SUPPORTEDALGORITHMS, &CTestAppDlg::OnLocalSupportedAlgorithms)
	ON_UPDATE_COMMAND_UI(ID_LOCAL_SUPPORTEDALGORITHMS, &CTestAppDlg::OnUpdateLocalSupportedAlgorithms)
	ON_COMMAND(ID_SETTINGS_GENERAL, &CTestAppDlg::OnSettingsGeneral)
	ON_UPDATE_COMMAND_UI(ID_SETTINGS_GENERAL, &CTestAppDlg::OnUpdateSettingsGeneral)
	ON_COMMAND(ID_BENCHMARKS_CONSOLE, &CTestAppDlg::OnBenchmarksConsole)
	ON_COMMAND(ID_STRESS_MULTIPLEINSTANCES, &CTestAppDlg::OnStressMultipleInstances)
	ON_UPDATE_COMMAND_UI(ID_STRESS_MULTIPLEINSTANCES, &CTestAppDlg::OnUpdateStressMultipleInstances)
	ON_COMMAND(ID_BENCHMARKS_MEMORY, &CTestAppDlg::OnBenchmarksMemory)
	ON_COMMAND(ID_UTILS_LOGPOOLALLOCATORSTATISTICS, &CTestAppDlg::OnUtilsLogPoolAllocatorStatistics)
	ON_COMMAND(ID_LOCAL_IPREPUTATIONS, &CTestAppDlg::OnLocalIPReputations)
	ON_COMMAND(ID_ATTACKS_CONNECTANDDISCONNECT, &CTestAppDlg::OnAttacksConnectAndDisconnect)
	ON_COMMAND(ID_ATTACKS_CONNECTANDWAIT, &CTestAppDlg::OnAttacksConnectAndWait)
	ON_UPDATE_COMMAND_UI(ID_ATTACKS_CONNECTANDDISCONNECT, &CTestAppDlg::OnUpdateAttacksConnectAndDisconnect)
	ON_UPDATE_COMMAND_UI(ID_ATTACKS_CONNECTANDWAIT, &CTestAppDlg::OnUpdateAttacksConnectAndWait)
	ON_COMMAND(ID_LOCAL_ENVIRONMENTINFO, &CTestAppDlg::OnLocalEnvironmentInfo)
	ON_COMMAND(ID_UTILS_PING, &CTestAppDlg::OnUtilsPing)
END_MESSAGE_MAP()

BOOL CTestAppDlg::OnInitDialog()
{
	CDialogBase::OnInitDialog();

	// Set the icon for this dialog. The framework does this automatically
	// when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	InitializeTabCtrl();

	LoadSettings();

	UpdateControls();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
// to draw the icon. For MFC applications using the document/view model,
// this is automatically done for you by the framework.

void CTestAppDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		const int cxIcon = GetSystemMetrics(SM_CXICON);
		const int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		const int x = (rect.Width() - cxIcon + 1) / 2;
		const int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogBase::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
// the minimized window.
HCURSOR CTestAppDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTestAppDlg::InitializeTabCtrl()
{
	auto tabctrl = (CTabCtrl*)GetDlgItem(IDC_TAB_CTRL);

	m_MainTab.Create(IDD_QGTESTAPP_DIALOG_MAIN_TAB, tabctrl);
	m_TestExtenderTab.Create(IDD_QGTESTAPP_DIALOG_TESTEXTENDER_TAB, tabctrl);
	m_AVExtenderTab.Create(IDD_QGTESTAPP_DIALOG_AVEXTENDER_TAB, tabctrl);

	tabctrl->InsertItem(0, L"Main");
	tabctrl->InsertItem(1, L"Test Extender");
	tabctrl->InsertItem(2, L"AV Extender");

	UpdateTabCtrl();
}

void CTestAppDlg::UpdateTabCtrl()
{
	const auto tabctrl = (CTabCtrl*)GetDlgItem(IDC_TAB_CTRL);

	CRect itemRect;
	tabctrl->GetItemRect(0, &itemRect);

	CRect tabRect;
	tabctrl->GetClientRect(tabRect);

	tabRect.top = itemRect.bottom + 4;
	tabRect.left += 4;
	tabRect.right -= 10;
	tabRect.bottom -= (itemRect.bottom + 9);

	switch (tabctrl->GetCurSel())
	{
		case 0:
			m_MainTab.SetWindowPos(nullptr, tabRect.left, tabRect.top, tabRect.right, tabRect.bottom, SWP_SHOWWINDOW);
			m_TestExtenderTab.SetWindowPos(nullptr, tabRect.left, tabRect.top, tabRect.right, tabRect.bottom, SWP_HIDEWINDOW);
			m_AVExtenderTab.SetWindowPos(nullptr, tabRect.left, tabRect.top, tabRect.right, tabRect.bottom, SWP_HIDEWINDOW);
			m_MainTab.GotoDlgCtrl(m_MainTab.GetDlgItem(IDC_SERVERPORT));
			break;
		case 1:
			m_TestExtenderTab.SetWindowPos(nullptr, tabRect.left, tabRect.top, tabRect.right, tabRect.bottom, SWP_SHOWWINDOW);
			m_MainTab.SetWindowPos(nullptr, tabRect.left, tabRect.top, tabRect.right, tabRect.bottom, SWP_HIDEWINDOW);
			m_AVExtenderTab.SetWindowPos(nullptr, tabRect.left, tabRect.top, tabRect.right, tabRect.bottom, SWP_HIDEWINDOW);
			m_TestExtenderTab.GotoDlgCtrl(m_TestExtenderTab.GetDlgItem(IDC_PEERLIST));
			break;
		case 2:
			m_MainTab.SetWindowPos(nullptr, tabRect.left, tabRect.top, tabRect.right, tabRect.bottom, SWP_HIDEWINDOW);
			m_TestExtenderTab.SetWindowPos(nullptr, tabRect.left, tabRect.top, tabRect.right, tabRect.bottom, SWP_HIDEWINDOW);
			m_AVExtenderTab.SetWindowPos(nullptr, tabRect.left, tabRect.top, tabRect.right, tabRect.bottom, SWP_SHOWWINDOW);
			m_AVExtenderTab.GotoDlgCtrl(m_AVExtenderTab.GetDlgItem(IDC_PEERLIST));
			break;
		default:
			assert(false);
			break;
	}
}

void CTestAppDlg::OnTcnSelchangeTabCtrl(NMHDR* pNMHDR, LRESULT* pResult)
{
	UpdateTabCtrl();
	*pResult = 0;
}

BOOL CTestAppDlg::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// Don't send close system command to tab
	if (nID != 2)
	{
		if (m_MainTab.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) return TRUE;
		else if (m_TestExtenderTab.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) return TRUE;
		else if (m_AVExtenderTab.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) return TRUE;
	}

	return CDialogBase::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

BOOL CTestAppDlg::PreTranslateMessage(MSG* pMsg)
{
	// Check first if tabs can handle the message
	if (m_MainTab.PreTranslateMessage(pMsg)) return TRUE;
	else if (m_TestExtenderTab.PreTranslateMessage(pMsg)) return TRUE;
	else if (m_AVExtenderTab.PreTranslateMessage(pMsg)) return TRUE;

	return CDialogBase::PreTranslateMessage(pMsg);
}

LRESULT CTestAppDlg::OnQGUpdateControls(WPARAM w, LPARAM l)
{
	UpdateControls();
	return 0;
}

void CTestAppDlg::LoadSettings()
{
	auto filepath = Util::ToStringA(GetApp()->GetFolder()) + "QuantumGate.json";

	// No settings file to load; we'll create one on exit
	if (!std::filesystem::exists(Path(filepath))) return;

	try
	{
		std::ifstream file(filepath);
		json j;
		file >> j;

		try
		{
			const auto it = j.find("Settings");
			if (it != j.end())
			{
				auto& set = it.value();

				if (set.find("LocalPorts") != set.end())
				{
					m_MainTab.SetValue(IDC_SERVERPORT, set["LocalPorts"].get<std::string>());
				}
				else m_MainTab.SetValue(IDC_SERVERPORT, L"999");

				if (set.find("LocalUUID") != set.end())
				{
					m_MainTab.SetValue(IDC_LOCAL_UUID, set["LocalUUID"].get<std::string>());
				}

				if (set.find("RequirePeerAuthentication") != set.end())
				{
					m_StartupParameters.RequireAuthentication = set["RequirePeerAuthentication"].get<bool>();
				}

				if (set.find("RelayIPv4ExcludedNetworksCIDRLeadingBits") != set.end())
				{
					m_StartupParameters.Relays.IPv4ExcludedNetworksCIDRLeadingBits = set["RelayIPv4ExcludedNetworksCIDRLeadingBits"].get<int>();
				}

				if (set.find("RelayIPv6ExcludedNetworksCIDRLeadingBits") != set.end())
				{
					m_StartupParameters.Relays.IPv6ExcludedNetworksCIDRLeadingBits = set["RelayIPv6ExcludedNetworksCIDRLeadingBits"].get<int>();
				}

				if (set.find("PeerAccessDefaultAllowed") != set.end())
				{
					m_QuantumGate.GetAccessManager().SetPeerAccessDefault(set["PeerAccessDefaultAllowed"].get<bool>() ?
																		  PeerAccessDefault::Allowed : PeerAccessDefault::NotAllowed);
				}

				if (set.find("ConnectIP") != set.end())
				{
					m_DefaultIP = Util::ToStringW(set["ConnectIP"].get<std::string>()).c_str();
				}

				if (set.find("ConnectPort") != set.end())
				{
					m_DefaultPort = set["ConnectPort"].get<int>();
				}

				if (set.find("AutoFileTransferFile") != set.end())
				{
					m_TestExtenderTab.SetValue(IDC_FILE_PATH, set["AutoFileTransferFile"].get<std::string>());
				}
			}
		}
		catch (const std::exception& e)
		{
			AfxMessageBox(L"Couldn't load settings. Exception: " + CString(e.what()), MB_ICONERROR);
		}

		try
		{
			const auto it = j.find("IPFilters");
			if (it != j.end())
			{
				const auto& flts = it.value();
				for (auto fit = flts.begin(); fit != flts.end(); ++fit)
				{
					auto error = false;
					auto& flt = fit.value();

					if (flt.find("Address") != flt.end() &&
						flt.find("Mask") != flt.end() &&
						flt.find("Allowed") != flt.end())
					{
						auto type = QuantumGate::IPFilterType::Allowed;
						if (!flt["Allowed"].get<bool>()) type = QuantumGate::IPFilterType::Blocked;

						const auto result = m_QuantumGate.GetAccessManager().AddIPFilter(Util::ToStringW(flt["Address"].get<std::string>()),
																						 Util::ToStringW(flt["Mask"].get<std::string>()),
																						 type);
						if (result.Failed())
						{
							error = true;
						}
					}
					else error = true;

					if (error) AfxMessageBox(L"There was an error while loading an IPFilter from the settings file.", MB_ICONERROR);
				}
			}
		}
		catch (const std::exception& e)
		{
			AfxMessageBox(L"Couldn't load IPFilters from settings file. Exception: " + CString(e.what()), MB_ICONERROR);
		}

		try
		{
			const auto it = j.find("IPSubnetLimits");
			if (it != j.end())
			{
				const auto& limits = it.value();
				for (auto lit = limits.begin(); lit != limits.end(); ++lit)
				{
					auto error = false;
					auto& limit = lit.value();

					if (limit.find("AddressFamily") != limit.end() &&
						limit.find("CIDR") != limit.end() &&
						limit.find("MaxConnections") != limit.end())
					{
						auto ftype = QuantumGate::IPAddress::Family::Unspecified;
						if (limit["AddressFamily"].get<std::string>() == "IPv4") ftype = QuantumGate::IPAddress::Family::IPv4;
						else if (limit["AddressFamily"].get<std::string>() == "IPv6") ftype = QuantumGate::IPAddress::Family::IPv6;

						if (ftype != QuantumGate::IPAddress::Family::Unspecified)
						{
							const auto result = m_QuantumGate.GetAccessManager().AddIPSubnetLimit(ftype,
																								  Util::ToStringW(limit["CIDR"].get<std::string>()),
																								  limit["MaxConnections"].get<int>());
							if (result.Failed())
							{
								error = true;
							}
						}
						else error = true;
					}
					else error = true;

					if (error) AfxMessageBox(L"There was an error while loading an IPSubnetLimit from the settings file.", MB_ICONERROR);
				}
			}
		}
		catch (const std::exception& e)
		{
			AfxMessageBox(L"Couldn't load IPSubnetLimits from settings file. Exception: " + CString(e.what()), MB_ICONERROR);
		}

		try
		{
			const auto it = j.find("PeerAccessSettings");
			if (it != j.end())
			{
				const auto& peers = it.value();
				for (auto lit = peers.begin(); lit != peers.end(); ++lit)
				{
					auto success = false;
					auto& peer = lit.value();

					if (peer.find("UUID") != peer.end() &&
						peer.find("PublicKey") != peer.end() &&
						peer.find("AccessAllowed") != peer.end())
					{
						PeerAccessSettings pas;
						pas.AccessAllowed = peer["AccessAllowed"].get<bool>();

						if (QuantumGate::UUID::TryParse(Util::ToStringW(peer["UUID"].get<std::string>()), pas.UUID))
						{
							std::string b64 = peer["PublicKey"].get<std::string>();
							if (!b64.empty())
							{
								auto buf = Util::FromBase64(b64);
								if (buf)
								{
									pas.PublicKey = std::move(*buf);
								}
							}

							const auto result = m_QuantumGate.GetAccessManager().AddPeer(std::move(pas));
							if (result.Succeeded())
							{
								success = true;
							}
						}
					}

					if (!success) AfxMessageBox(L"There was an error while loading a PeerAccessSetting from the settings file.", MB_ICONERROR);
				}
			}
		}
		catch (const std::exception& e)
		{
			AfxMessageBox(L"Couldn't load PeerAccessSettings from settings file. Exception: " + CString(e.what()), MB_ICONERROR);
		}
	}
	catch (const std::exception& e)
	{
		AfxMessageBox(L"Couldn't load settings from settings file. Exception: " + CString(e.what()), MB_ICONERROR);
	}
}

void CTestAppDlg::SaveSettings()
{
	try
	{
		json j = json::object();

		try
		{
			auto localport = m_MainTab.GetTextValue(IDC_SERVERPORT);
			auto luuid = m_MainTab.GetTextValue(IDC_LOCAL_UUID);
			auto autotrf_file = m_TestExtenderTab.GetTextValue(IDC_FILE_PATH);

			j["Settings"] = json::object();
			j["Settings"]["LocalPorts"] = Util::ToStringA((LPCWSTR)localport);
			j["Settings"]["LocalUUID"] = Util::ToStringA((LPCWSTR)luuid);
			j["Settings"]["RequirePeerAuthentication"] = m_StartupParameters.RequireAuthentication;
			j["Settings"]["RelayIPv4ExcludedNetworksCIDRLeadingBits"] = m_StartupParameters.Relays.IPv4ExcludedNetworksCIDRLeadingBits;
			j["Settings"]["RelayIPv6ExcludedNetworksCIDRLeadingBits"] = m_StartupParameters.Relays.IPv6ExcludedNetworksCIDRLeadingBits;

			if (m_QuantumGate.GetAccessManager().GetPeerAccessDefault() == PeerAccessDefault::Allowed)
			{
				j["Settings"]["PeerAccessDefaultAllowed"] = true;
			}
			else j["Settings"]["PeerAccessDefaultAllowed"] = false;

			j["Settings"]["ConnectIP"] = Util::ToStringA(m_DefaultIP);
			j["Settings"]["ConnectPort"] = m_DefaultPort;

			j["Settings"]["AutoFileTransferFile"] = Util::ToStringA((LPCWSTR)autotrf_file);
		}
		catch (const std::exception& e)
		{
			AfxMessageBox(L"Couldn't save settings to settings file. Exception: " + CString(e.what()), MB_ICONERROR);
		}

		try
		{
			j["IPFilters"] = {};

			if (const auto result = m_QuantumGate.GetAccessManager().GetAllIPFilters(); result.Succeeded())
			{
				for (const auto& flt : *result)
				{
					auto jflt = json::object();
					jflt["Address"] = Util::ToStringA(flt.Address.GetString().c_str());
					jflt["Mask"] = Util::ToStringA(flt.Mask.GetString().c_str());

					auto allowed = true;
					if (flt.Type == QuantumGate::IPFilterType::Blocked) allowed = false;

					jflt["Allowed"] = allowed;

					j["IPFilters"].push_back(std::move(jflt));
				}
			}
		}
		catch (const std::exception& e)
		{
			AfxMessageBox(L"Couldn't save IPFilters to settings file. Exception: " + CString(e.what()), MB_ICONERROR);
		}

		try
		{
			j["IPSubnetLimits"] = {};

			if (const auto result = m_QuantumGate.GetAccessManager().GetAllIPSubnetLimits(); result.Succeeded())
			{
				for (const auto& limit : *result)
				{
					auto jflt = json::object();

					auto type = "IPv4";
					if (limit.AddressFamily == QuantumGate::IPAddress::Family::IPv6) type = "IPv6";

					jflt["AddressFamily"] = type;
					jflt["CIDR"] = Util::ToStringA(limit.CIDRLeadingBits);
					jflt["MaxConnections"] = limit.MaximumConnections;

					j["IPSubnetLimits"].push_back(std::move(jflt));
				}
			}
		}
		catch (const std::exception& e)
		{
			AfxMessageBox(L"Couldn't save IPSubnetLimits to settings file. Exception: " + CString(e.what()), MB_ICONERROR);
		}


		try
		{
			j["PeerAccessSettings"] = {};

			if (const auto result = m_QuantumGate.GetAccessManager().GetAllPeers(); result.Succeeded())
			{
				for (const auto& peer : *result)
				{
					auto jflt = json::object();

					std::string b64;

					if (!peer.PublicKey.IsEmpty())
					{
						const auto b64t = Util::ToBase64(peer.PublicKey);
						if (b64t)
						{
							b64 = Util::ToProtectedStringA(*b64t);
						}
					}

					jflt["UUID"] = Util::ToStringA(peer.UUID.GetString());
					jflt["PublicKey"] = b64;
					jflt["AccessAllowed"] = peer.AccessAllowed;

					j["PeerAccessSettings"].push_back(std::move(jflt));
				}
			}
		}
		catch (const std::exception& e)
		{
			AfxMessageBox(L"Couldn't save PeerAccessSettings to settings file. Exception: " + CString(e.what()), MB_ICONERROR);
		}

		std::ofstream file(Util::ToStringA(GetApp()->GetFolder()) + "QuantumGate.json");
		file << std::setw(4) << j << std::endl;
	}
	catch (const std::exception& e)
	{
		AfxMessageBox(L"Couldn't save settings to settings file. Exception: " + CString(e.what()), MB_ICONERROR);
	}
}

void CTestAppDlg::OnTimer(UINT_PTR nIDEvent)
{
	CDialogBase::OnTimer(nIDEvent);
}

void CTestAppDlg::LoadSocks5Extender()
{
	auto extender = m_QuantumGate.GetExtender(Socks5Extender::Extender::UUID).lock();
	if (!extender)
	{
		auto extender = std::make_shared<Socks5Extender::Extender>();
		auto extp = std::static_pointer_cast<Extender>(extender);
		if (const auto result = m_QuantumGate.AddExtender(extp); result.Failed())
		{
			LogErr(L"Failed to add Socks5Extender: %s", result.GetErrorDescription().c_str());
		}
	}
}

void CTestAppDlg::UnloadSocks5Extender()
{
	auto extender = m_QuantumGate.GetExtender(Socks5Extender::Extender::UUID).lock();
	if (extender)
	{
		if (const auto result = m_QuantumGate.RemoveExtender(extender); result.Failed())
		{
			LogErr(L"Failed to add Socks5Extender: %s", result.GetErrorDescription().c_str());
		}
	}
}

void CTestAppDlg::OnClose()
{
	if (m_QuantumGate.IsRunning()) OnLocalDeinitialize();

	Attacks::StopConnectGarbageAttack();
	Attacks::StopConnectAttack();

	Stress::StopMultiInstanceStress();

	SaveSettings();

	CDialogBase::OnClose();
}

void CTestAppDlg::OnLocalInitialize()
{
	auto ports = m_MainTab.GetTextValue(IDC_SERVERPORT);
	if (ports.IsEmpty())
	{
		AfxMessageBox(L"Specify at least one listener port for the local instance.");
		return;
	}

	auto luuid = m_MainTab.GetTextValue(IDC_LOCAL_UUID);
	if (luuid.IsEmpty())
	{
		AfxMessageBox(L"Specify a UUID for the local instance.");
		return;
	}

	StartupParameters params(m_StartupParameters);

	if (!QuantumGate::UUID::TryParse(luuid.GetString(), params.UUID))
	{
		AfxMessageBox(L"Invalid UUID specified for the local instance.", MB_ICONERROR);
		return;
	}

	params.Keys.emplace();

	if (!GetApp()->LoadKey(GetApp()->GetFolder() + L"private_" + luuid.GetString() + L".pem", params.Keys->PrivateKey) ||
		!GetApp()->LoadKey(GetApp()->GetFolder() + L"public_" + luuid.GetString() + L".pem", params.Keys->PublicKey))
	{
		return;
	}

	params.Listeners.Enable = true;
	params.Listeners.TCPPorts = GetPorts(ports);
	params.Listeners.EnableNATTraversal = true;
	params.EnableExtenders = true;
	params.Relays.Enable = true;

	auto passphrase = m_MainTab.GetTextValue(IDC_PASSPHRASE);
	if (passphrase.GetLength() > 0)
	{
		params.GlobalSharedSecret.emplace();

		if (!GenerateGlobalSharedSecret(passphrase, *params.GlobalSharedSecret)) return;
	}

	m_QuantumGate.Startup(params).Failed([](auto& result)
	{
		LogErr(L"Failed to start QuantumGate: %s", result.GetErrorString().c_str());
	});

	UpdateControls();
}

bool CTestAppDlg::GenerateGlobalSharedSecret(CString& passphrase, ProtectedBuffer& buffer) const noexcept
{
	ProtectedBuffer pbuf(reinterpret_cast<Byte*>(passphrase.GetBuffer()), passphrase.GetLength() * sizeof(wchar_t));

	if (Crypto::HKDF(pbuf, buffer, 64, Algorithm::Hash::BLAKE2B512))
	{
		assert(!buffer.IsEmpty());

		Dbg(L"Global shared secret hash: %d bytes - %s", buffer.GetSize(),
			Util::ToBase64(buffer)->c_str());

		return true;
	}
	else LogErr(L"Could not generate a global shared secret from the passphrase");

	return false;
}

void CTestAppDlg::OnUpdateLocalInitialize(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(!m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnUpdateLocalDeinitialize(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnLocalDeinitialize()
{
	Stress::StopExtenderStartupShutdownStress();
	Stress::StopConnectStress();

	m_QuantumGate.Shutdown().Failed([](auto& result)
	{
		LogErr(L"Failed to shut down QuantumGate: %s", result.GetErrorString().c_str());
	});

	UpdateControls();
}

void CTestAppDlg::OnLocalIPFilters()
{
	CIPFiltersDlg dlg;
	dlg.SetAccessManager(&m_QuantumGate.GetAccessManager());
	dlg.DoModal();
}

void CTestAppDlg::OnUpdateLocalIPFilters(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(true);
}

void CTestAppDlg::OnSecuritylevelOne()
{
	SetSecurityLevel(SecurityLevel::One);
}

void CTestAppDlg::OnSecuritylevelTwo()
{
	SetSecurityLevel(SecurityLevel::Two);
}

void CTestAppDlg::OnSecuritylevelThree()
{
	SetSecurityLevel(SecurityLevel::Three);
}

void CTestAppDlg::OnSecuritylevelFour()
{
	SetSecurityLevel(SecurityLevel::Four);
}

void CTestAppDlg::OnSecuritylevelFive()
{
	SetSecurityLevel(SecurityLevel::Five);
}

void CTestAppDlg::OnUpdateSecuritylevelOne(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.GetSecurityLevel() == SecurityLevel::One);
}

void CTestAppDlg::OnUpdateSecuritylevelTwo(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.GetSecurityLevel() == SecurityLevel::Two);
}

void CTestAppDlg::OnUpdateSecuritylevelThree(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.GetSecurityLevel() == SecurityLevel::Three);
}

void CTestAppDlg::OnUpdateSecuritylevelFour(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.GetSecurityLevel() == SecurityLevel::Four);
}

void CTestAppDlg::OnUpdateSecuritylevelFive(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.GetSecurityLevel() == SecurityLevel::Five);
}

void CTestAppDlg::SetSecurityLevel(const QuantumGate::SecurityLevel level)
{
	m_QuantumGate.SetSecurityLevel(level).Failed([](auto& result)
	{
		LogErr(L"Failed to set QuantumGate security level: %s", result.GetErrorString().c_str());
	});
}

void CTestAppDlg::OnBenchmarksDelegates()
{
	Benchmarks::BenchmarkCallbacks();
}

void CTestAppDlg::OnBenchmarksMutexes()
{
	Benchmarks::BenchmarkMutexes();
}

void CTestAppDlg::OnAttacksConnectWithGarbage()
{
	if (!Attacks::IsConnectGarbageAttackRunning())
	{
		CEndpointDlg dlg;
		dlg.SetIPAddress(m_DefaultIP);
		dlg.SetPort(m_DefaultPort);

		if (dlg.DoModal() == IDOK)
		{
			Attacks::StartConnectGarbageAttack(dlg.GetIPAddress().GetString().c_str(), dlg.GetPort());
		}
	}
	else
	{
		Attacks::StopConnectGarbageAttack();
	}
}

void CTestAppDlg::OnUpdateAttacksConnectWithGarbage(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(Attacks::IsConnectGarbageAttackRunning());
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnAttacksConnectAndDisconnect()
{
	if (!Attacks::IsConnectAttackRunning())
	{
		CEndpointDlg dlg;
		dlg.SetIPAddress(m_DefaultIP);
		dlg.SetPort(m_DefaultPort);

		if (dlg.DoModal() == IDOK)
		{
			Attacks::StartConnectAttack(dlg.GetIPAddress().GetString().c_str(), dlg.GetPort());
		}
	}
	else
	{
		Attacks::StopConnectAttack();
	}
}

void CTestAppDlg::OnUpdateAttacksConnectAndDisconnect(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(Attacks::IsConnectAttackRunning());
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnAttacksConnectAndWait()
{
	if (!Attacks::IsConnectWaitAttackRunning())
	{
		CEndpointDlg dlg;
		dlg.SetIPAddress(m_DefaultIP);
		dlg.SetPort(m_DefaultPort);

		if (dlg.DoModal() == IDOK)
		{
			Attacks::StartConnectWaitAttack(dlg.GetIPAddress().GetString().c_str(), dlg.GetPort());
		}
	}
	else
	{
		Attacks::StopConnectWaitAttack();
	}
}

void CTestAppDlg::OnUpdateAttacksConnectAndWait(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(Attacks::IsConnectWaitAttackRunning());
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnLocalListenersEnabled()
{
	if (!m_QuantumGate.AreListenersEnabled())
	{
		m_QuantumGate.EnableListeners().Failed([](auto& result)
		{
			LogErr(L"Failed to enable listeners: %s", result.GetErrorString().c_str());
		});
	}
	else
	{
		m_QuantumGate.DisableListeners().Failed([](auto& result)
		{
			LogErr(L"Failed to disable listeners: %s", result.GetErrorString().c_str());
		});
	}
}

void CTestAppDlg::OnUpdateLocalListenersEnabled(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.AreListenersEnabled());
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnLocalExtendersEnabled()
{
	if (!m_QuantumGate.AreExtendersEnabled())
	{
		m_QuantumGate.EnableExtenders().Failed([](auto& result)
		{
			LogErr(L"Failed to enable extenders: %s", result.GetErrorString().c_str());
		});
	}
	else
	{
		m_QuantumGate.DisableExtenders().Failed([](auto& result)
		{
			LogErr(L"Failed to disable extenders: %s", result.GetErrorString().c_str());
		});
	}
}

void CTestAppDlg::OnUpdateLocalExtendersEnabled(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.AreExtendersEnabled());
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnBenchmarksThreadLocalCache()
{
	Benchmarks::BenchmarkThreadLocalCache();
}

void CTestAppDlg::OnStressInitAndDeinitExtenders()
{
	if (!Stress::IsExtenderStartupShutdownStressRunning())
	{
		Stress::StartExtenderStartupShutdownStress(m_QuantumGate);
	}
	else Stress::StopExtenderStartupShutdownStress();
}

void CTestAppDlg::OnUpdateStressInitAndDeinitExtenders(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(Stress::IsExtenderStartupShutdownStressRunning());
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnStressConnectAndDisconnect()
{
	if (!Stress::IsConnectStressRunning())
	{
		CEndpointDlg dlg;
		dlg.SetIPAddress(m_DefaultIP);
		dlg.SetPort(m_DefaultPort);
		dlg.SetShowRelay(true);

		if (dlg.DoModal() == IDOK)
		{
			auto passphrase = dlg.GetPassPhrase();

			ProtectedBuffer gsecret;

			if (passphrase.GetLength() > 0)
			{
				if (!GenerateGlobalSharedSecret(passphrase, gsecret)) return;
			}

			Stress::StartConnectStress(m_QuantumGate, dlg.GetIPAddress().GetString().c_str(), dlg.GetPort(),
									   dlg.GetRelayHops(), dlg.GetRelayPeer(), gsecret);
		}
	}
	else Stress::StopConnectStress();
}

void CTestAppDlg::OnUpdateStressConnectAndDisconnect(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(Stress::IsConnectStressRunning());
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnLocalCustomSecuritySettings()
{
	CSecurityDlg dlg;
	dlg.SetQuantumGate(&m_QuantumGate);
	dlg.DoModal();
}

void CTestAppDlg::OnUpdateLocalCustomSecuritySettings(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.GetSecurityLevel() == SecurityLevel::Custom);
}

void CTestAppDlg::OnBenchmarksCompression()
{
	Benchmarks::BenchmarkCompression();
}

void CTestAppDlg::OnSocks5ExtenderLoad()
{
	if (!m_QuantumGate.HasExtender(Socks5Extender::Extender::UUID))
	{
		LoadSocks5Extender();
	}
	else UnloadSocks5Extender();
}

void CTestAppDlg::OnUpdateSocks5ExtenderLoad(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.HasExtender(Socks5Extender::Extender::UUID));
}

void CTestAppDlg::OnSocks5ExtenderAuthentication()
{
	auto extender = m_QuantumGate.GetExtender(Socks5Extender::Extender::UUID).lock();
	if (extender)
	{
		auto socks5ext = std::static_pointer_cast<Socks5Extender::Extender>(extender);
		if (socks5ext)
		{
			CAuthenticationDlg dlg;
			if (dlg.DoModal() == IDOK)
			{
				auto usr = ProtectedString{ dlg.GetUsername().GetString() };
				auto pwd = ProtectedString{ dlg.GetPassword().GetString() };

				if (!socks5ext->SetCredentials(Util::ToProtectedStringA(usr),
											   Util::ToProtectedStringA(pwd)))
				{
					AfxMessageBox(L"Couldn't set credentials for Socks5 Extender.", MB_ICONERROR);
				}
			}
		}
	}
}

void CTestAppDlg::OnUpdateSocks5ExtenderAuthentication(CCmdUI* pCmdUI)
{
	auto extender = m_QuantumGate.GetExtender(Socks5Extender::Extender::UUID).lock();
	if (extender)
	{
		auto socks5ext = std::static_pointer_cast<Socks5Extender::Extender>(extender);
		if (socks5ext)
		{
			pCmdUI->Enable(true);
			pCmdUI->SetCheck(socks5ext->IsAuthenticationRequired());
			return;
		}
	}

	pCmdUI->Enable(false);
	pCmdUI->SetCheck(false);
}

void CTestAppDlg::OnSocks5ExtenderAcceptIncomingConnections()
{
	auto extender = m_QuantumGate.GetExtender(Socks5Extender::Extender::UUID).lock();
	if (extender)
	{
		auto socks5ext = std::static_pointer_cast<Socks5Extender::Extender>(extender);
		if (socks5ext)
		{
			socks5ext->SetAcceptIncomingConnections(!socks5ext->IsAcceptingIncomingConnections());
		}
	}
}

void CTestAppDlg::OnUpdateSocks5ExtenderAcceptIncomingConnections(CCmdUI* pCmdUI)
{
	auto extender = m_QuantumGate.GetExtender(Socks5Extender::Extender::UUID).lock();
	if (extender)
	{
		auto socks5ext = std::static_pointer_cast<Socks5Extender::Extender>(extender);
		if (socks5ext)
		{
			pCmdUI->Enable(true);
			pCmdUI->SetCheck(socks5ext->IsAcceptingIncomingConnections());
			return;
		}
	}

	pCmdUI->Enable(false);
	pCmdUI->SetCheck(false);
}

void CTestAppDlg::OnSocks5ExtenderUseCompression()
{
	auto extender = m_QuantumGate.GetExtender(Socks5Extender::Extender::UUID).lock();
	if (extender)
	{
		auto socks5ext = std::static_pointer_cast<Socks5Extender::Extender>(extender);
		if (socks5ext)
		{
			socks5ext->SetUseCompression(!socks5ext->IsUsingCompression());
		}
	}
}

void CTestAppDlg::OnUpdateSocks5ExtenderUseCompression(CCmdUI* pCmdUI)
{
	auto extender = m_QuantumGate.GetExtender(Socks5Extender::Extender::UUID).lock();
	if (extender)
	{
		auto socks5ext = std::static_pointer_cast<Socks5Extender::Extender>(extender);
		if (socks5ext)
		{
			pCmdUI->Enable(true);
			pCmdUI->SetCheck(socks5ext->IsUsingCompression());
			return;
		}
	}

	pCmdUI->Enable(false);
	pCmdUI->SetCheck(false);
}

void CTestAppDlg::OnExtendersLoadFromModule()
{
	const auto path = GetApp()->BrowseForFile(GetSafeHwnd(), false);
	if (path)
	{
		m_QuantumGate.AddExtenderModule(Path(path->GetString())).Failed([](auto& result)
		{
			LogErr(L"Failed to add extender module: %s", result.GetErrorString().c_str());
		});
	}
}

void CTestAppDlg::OnExtendersUnloadFromModule()
{
	const auto path = GetApp()->BrowseForFile(GetSafeHwnd(), false);
	if (path)
	{
		m_QuantumGate.RemoveExtenderModule(Path(path->GetString())).Failed([](auto& result)
		{
			LogErr(L"Failed to remove extender module: %s", result.GetErrorString().c_str());
		});
	}
}

void CTestAppDlg::OnLocalIpsubnetlimits()
{
	CIPSubnetLimitsDlg dlg;
	dlg.SetAccessManager(&m_QuantumGate.GetAccessManager());
	dlg.DoModal();
}

void CTestAppDlg::OnUtilsUUIDGenerationAndValidation()
{
	CUUIDDialog dlg;
	dlg.DoModal();
}

void CTestAppDlg::OnLocalAllowUnauthenticatedPeers()
{
	m_StartupParameters.RequireAuthentication = !m_StartupParameters.RequireAuthentication;
}

void CTestAppDlg::OnUpdateLocalAllowUnauthenticatedPeers(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_StartupParameters.RequireAuthentication);
	pCmdUI->Enable(!m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnPeerAccessSettingsAdd()
{
	CPeerAccessDlg dlg;
	dlg.SetAccessManager(&m_QuantumGate.GetAccessManager());
	dlg.DoModal();
}

void CTestAppDlg::OnLocalRelaysEnabled()
{
	if (!m_QuantumGate.AreRelaysEnabled())
	{
		m_QuantumGate.EnableRelays().Failed([](const auto& result)
		{
			LogErr(L"Failed to enable relays: %s", result.GetErrorString().c_str());
		});
	}
	else
	{
		m_QuantumGate.DisableRelays().Failed([](const auto& result)
		{
			LogErr(L"Failed to disable relays: %s", result.GetErrorString().c_str());
		});
	}
}

void CTestAppDlg::OnUpdateLocalRelaysEnabled(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_QuantumGate.AreRelaysEnabled());
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnLocalConnect()
{
	CEndpointDlg dlg;
	dlg.SetIPAddress(m_DefaultIP);
	dlg.SetPort(m_DefaultPort);

	if (dlg.DoModal() == IDOK)
	{
		m_DefaultIP = dlg.GetIPAddress().GetString();
		m_DefaultPort = dlg.GetPort();
		auto passphrase = dlg.GetPassPhrase();

		ConnectParameters params;
		params.PeerIPEndpoint = IPEndpoint(IPAddress(m_DefaultIP), m_DefaultPort);

		params.GlobalSharedSecret.emplace();

		if (passphrase.GetLength() > 0)
		{
			if (!GenerateGlobalSharedSecret(passphrase, *params.GlobalSharedSecret)) return;
		}

		const auto result = m_QuantumGate.ConnectTo(std::move(params), MakeCallback(this, &CTestAppDlg::OnPeerConnected));
		if (!result)
		{
			LogErr(L"Failed to connect: %s", result.GetErrorDescription().c_str());
		}
	}
}

void CTestAppDlg::OnUpdateLocalConnect(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnPeerConnected(PeerLUID pluid, Result<ConnectDetails> result)
{
	if (result.Succeeded())
	{
		LogInfo(L"Successfully connected to peer LUID %llu (%s, %s)",
				pluid, result->IsAuthenticated ? L"Authenticated" : L"NOT Authenticated",
				result->IsRelayed ? L"Relayed" : L"NOT Relayed");

		// Using PostMessage because the current QuantumGate worker thread should NOT be calling directly to the UI;
		// only the thread that created the Window should do that, to avoid deadlocks
		PostMessage(WM_UPDATE_CONTROLS, 0, 0);
	}
	else
	{
		LogErr(L"Could not connect to peer LUID %llu (%s)", pluid, result.GetErrorString().c_str());
	}
}

void CTestAppDlg::OnLocalConnectRelayed()
{
	CEndpointDlg dlg;
	dlg.SetIPAddress(m_DefaultIP);
	dlg.SetPort(m_DefaultPort);
	dlg.SetShowRelay(true);

	if (dlg.DoModal() == IDOK)
	{
		m_DefaultIP = dlg.GetIPAddress().GetString();
		m_DefaultPort = dlg.GetPort();
		auto passphrase = dlg.GetPassPhrase();

		ConnectParameters params;
		params.PeerIPEndpoint = IPEndpoint(IPAddress(m_DefaultIP), m_DefaultPort);
		params.Relay.Hops = dlg.GetRelayHops();
		params.Relay.ViaPeer = dlg.GetRelayPeer();

		params.GlobalSharedSecret.emplace();

		if (passphrase.GetLength() > 0)
		{
			if (!GenerateGlobalSharedSecret(passphrase, *params.GlobalSharedSecret)) return;
		}

		const auto result = m_QuantumGate.ConnectTo(std::move(params),
													[](PeerLUID pluid, Result<ConnectDetails> result) mutable
		{
			if (result.Succeeded())
			{
				LogInfo(L"Relay connected");
			}
			else LogErr(L"Relay connection failed (%s)", result.GetErrorString().c_str());
		});

		if (!result)
		{
			LogErr(L"Failed to connect: %s", result.GetErrorDescription().c_str());
		}
	}
}

void CTestAppDlg::OnUpdateLocalConnectRelayed(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialogBase::OnShowWindow(bShow, nStatus);
	UpdateTabCtrl();
}

void CTestAppDlg::OnLocalSupportedAlgorithms()
{
	CAlgorithmsDlg dlg;
	dlg.SetAlgorithms(&m_StartupParameters.SupportedAlgorithms);
	dlg.DoModal();
}

void CTestAppDlg::OnUpdateLocalSupportedAlgorithms(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(!m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnSettingsGeneral()
{
	CSettingsDlg dlg;
	dlg.SetStartupParameters(&m_StartupParameters);
	dlg.DoModal();
}

void CTestAppDlg::OnUpdateSettingsGeneral(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(!m_QuantumGate.IsRunning());
}

void CTestAppDlg::OnBenchmarksConsole()
{
	const auto result = AfxMessageBox(L"If the terminal window is open, close it first or else this will take a long time. Do you want to continue?", MB_YESNO | MB_ICONQUESTION);
	if (result == IDYES)
	{
		Benchmarks::BenchmarkConsole();
	}
}

void CTestAppDlg::OnStressMultipleInstances()
{
	if (!Stress::IsMultiInstanceStressRunning())
	{
		auto luuid = m_MainTab.GetTextValue(IDC_LOCAL_UUID);
		if (luuid.IsEmpty())
		{
			AfxMessageBox(L"Specify a UUID for the local instance.");
			return;
		}

		CEndpointDlg dlg;
		dlg.SetIPAddress(m_DefaultIP);
		dlg.SetPort(m_DefaultPort);

		if (dlg.DoModal() == IDOK)
		{
			StartupParameters params(m_StartupParameters);

			if (!QuantumGate::UUID::TryParse(luuid.GetString(), params.UUID))
			{
				AfxMessageBox(L"Invalid UUID specified for the local instance.", MB_ICONERROR);
				return;
			}

			params.Keys.emplace();

			if (!GetApp()->LoadKey(GetApp()->GetFolder() + L"private_" + luuid.GetString() + L".pem", params.Keys->PrivateKey) ||
				!GetApp()->LoadKey(GetApp()->GetFolder() + L"public_" + luuid.GetString() + L".pem", params.Keys->PublicKey))
			{
				return;
			}

			params.Listeners.Enable = false;
			params.EnableExtenders = true;
			params.RequireAuthentication = false;

			ProtectedBuffer gsecret;
			auto passphrase = dlg.GetPassPhrase();
			if (passphrase.GetLength() > 0)
			{
				if (!GenerateGlobalSharedSecret(passphrase, gsecret)) return;
			}

			Stress::StartMultiInstanceStress(params, dlg.GetIPAddress().GetString().c_str(), dlg.GetPort(), gsecret);
		}
	}
	else Stress::StopMultiInstanceStress();
}

void CTestAppDlg::OnUpdateStressMultipleInstances(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(Stress::IsMultiInstanceStressRunning());
}

void CTestAppDlg::OnBenchmarksMemory()
{
	Benchmarks::BenchmarkMemory();
}

void CTestAppDlg::OnUtilsLogPoolAllocatorStatistics()
{
	QuantumGate::Implementation::Memory::PoolAllocator<void>::LogStatistics();
}

void CTestAppDlg::OnLocalIPReputations()
{
	CIPReputationsDlg dlg;
	dlg.SetAccessManager(&m_QuantumGate.GetAccessManager());
	dlg.DoModal();
}

void CTestAppDlg::OnLocalEnvironmentInfo()
{
	const auto env = m_QuantumGate.GetEnvironment();
	String info;

	if (const auto result = env.GetHostname(); result.Succeeded())
	{
		info += L"Hostname:\t" + *result + L"\r\n";
	}
	else AfxMessageBox(L"Failed to get hostname!", MB_ICONERROR);

	if (const auto result = env.GetUsername(); result.Succeeded())
	{
		info += L"Username:\t" + *result + L"\r\n";
	}
	else AfxMessageBox(L"Failed to get username!", MB_ICONERROR);

	if (const auto result = env.GetEthernetInterfaces(); result.Succeeded())
	{
		info += L"________________________________________________________\r\n\r\n";
		info += L"Ethernet interfaces:";

		for (const auto& eth : *result)
		{
			info += L"\r\n\r\nName:\t\t" + eth.Name + L"\r\n";
			info += L"Description:\t" + eth.Description + L"\r\n";
			info += L"MAC Address:\t" + eth.MACAddress + L"\r\n";

			String ips;
			for (const auto& ip : eth.IPAddresses)
			{
				if (!ips.empty()) ips += L", ";
				ips += ip.GetString();
			}

			info += L"IP Addresses:\t" + ips + L"\r\n";

			info += L"Operational:\t";
			if (eth.Operational) info += L"Yes";
			else info += L"No";
		}
	}
	else AfxMessageBox(L"Failed to get ethernet interfaces!", MB_ICONERROR);

	if (const auto result = env.GetIPAddresses(); result.Succeeded())
	{
		info += L"\r\n________________________________________________________\r\n\r\n";
		info += L"IP addresses:";

		for (const auto& ipdetails : *result)
		{
			info += L"\r\n\r\nAddress:\t\t\t\t" + ipdetails.IPAddress.GetString() + L"\r\n";

			info += L"On local interface:\t\t\t";
			if (ipdetails.BoundToLocalEthernetInterface) info += L"Yes";
			else info += L"No";

			if (ipdetails.PublicDetails.has_value())
			{
				info += L"\r\n";

				info += L"Reported by peers:\t\t\t";
				if (ipdetails.PublicDetails->ReportedByPeers) info += L"Yes";
				else info += L"No";

				info += L"\r\n";

				info += L"Reported by trusted peers:\t\t";
				if (ipdetails.PublicDetails->ReportedByTrustedPeers) info += L"Yes";
				else info += L"No";

				info += L"\r\n";

				info += L"Number of reporting networks:\t" +
					Util::FormatString(L"%zu", ipdetails.PublicDetails->NumReportingNetworks);

				info += L"\r\n";

				info += L"Verified:\t\t\t\t";
				if (ipdetails.PublicDetails->Verified) info += L"Yes";
				else info += L"No";
			}
		}
	}
	else AfxMessageBox(L"Failed to get IP addresses!", MB_ICONERROR);

	CInformationDlg dlg;
	dlg.SetWindowTitle(L"Local Environment Information");
	dlg.SetInformationText(info.data());
	dlg.DoModal();
}

void CTestAppDlg::OnUtilsPing()
{
	CPingDlg dlg;
	if (dlg.DoModal() == IDOK)
	{
		Network::Ping ping(dlg.GetIPAddress().GetBinary(), dlg.GetBufferSize(),
						   dlg.GetTimeout(), dlg.GetTTL());
		if (ping.Execute())
		{
			SLogInfo(L"Ping: " << ping);
		}
	}
}
