// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CDialogBase.h"

class CEndpointDlg final : public CDialogBase
{
public:
	CEndpointDlg(CWnd* pParent = NULL);
	virtual ~CEndpointDlg();

	enum { IDD = IDD_ENDPOINT_DLG };

	void SetAddress(const String& addr) noexcept;
	void SetAddressHistory(const String& addrs) noexcept { m_AddressHistory = addrs; }

	inline void SetPort(const UInt16 port) noexcept { m_Port = port; }
	inline void SetProtocol(const Endpoint::Protocol protocol) noexcept { m_Protocol = protocol; }
	inline void SetRelayHops(const RelayHop hops) noexcept { m_Hops = hops; }
	inline void SetRelayGatewayPeer(const PeerLUID pluid) noexcept { m_RelayGatewayPeer = pluid; }
	inline void SetReuseConnection(const bool reuse) noexcept { m_ReuseConnection = reuse; }
	inline void SetBTHAuthentication(const bool auth) noexcept { m_BTHAuthentication = auth; }

	inline void SetShowRelay(const bool show) noexcept { m_ShowRelay = show; }

	inline void RemoveProtocol(const Endpoint::Protocol protocol) noexcept { m_Protocols.erase(protocol); }

	inline const Address& GetAddress() const noexcept { return m_Address; }
	inline const String& GetAddressHistory() const noexcept { return m_AddressHistory; }
	inline UInt16 GetPort() const noexcept { return m_Port; }
	inline Endpoint::Protocol GetProtocol() const noexcept { return m_Protocol; }
	inline const String& GetPassPhrase() const noexcept { return m_PassPhrase; }
	inline RelayHop GetRelayHops() const noexcept { return m_Hops; }
	inline const std::optional<PeerLUID>& GetRelayGatewayPeer() const noexcept { return m_RelayGatewayPeer; }
	inline bool GetReuseConnection() noexcept { return m_ReuseConnection; }
	inline bool GetBTHAuthentication() noexcept { return m_BTHAuthentication; }
	Endpoint GetEndpoint() const noexcept;

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();
	afx_msg void OnCbnSelChangeProtocolCombo();
	afx_msg void OnBnClickedBthServiceButton();

private:
	Address m_Address;
	String m_AddressHistory;
	UInt16 m_Port{ 999 };
	GUID m_ServiceClassID{ 0 };
	Endpoint::Protocol m_Protocol{ Endpoint::Protocol::Unspecified };
	String m_PassPhrase;
	RelayHop m_Hops{ 0 };
	std::optional<PeerLUID> m_RelayGatewayPeer;
	bool m_BTHAuthentication{ true };
	bool m_ReuseConnection{ true };
	bool m_ShowRelay{ false };
	std::set<Endpoint::Protocol> m_Protocols{ Endpoint::Protocol::TCP, Endpoint::Protocol::UDP, Endpoint::Protocol::BTH };
};

