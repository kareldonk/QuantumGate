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

	void SetIPAddress(const String& ip) noexcept;

	inline void SetPort(const UInt16 port) noexcept { m_Port = port; }
	inline void SetProtocol(const IPEndpoint::Protocol protocol) noexcept { m_Protocol = protocol; }
	inline void SetRelayHops(const RelayHop hops) noexcept { m_Hops = hops; }
	inline void SetRelayGatewayPeer(const PeerLUID pluid) noexcept { m_RelayGatewayPeer = pluid; }
	inline void SetReuseConnection(const bool reuse) noexcept { m_ReuseConnection = reuse; }

	inline void SetShowRelay(const bool show) noexcept { m_ShowRelay = show; }
	inline void SetProtocolSelection(const bool select) noexcept { m_ProtocolSelection = select; }

	inline const IPAddress& GetIPAddress() const noexcept { return m_IPAddress; }
	inline UInt16 GetPort() const noexcept { return m_Port; }
	inline IPEndpoint::Protocol GetProtocol() const noexcept { return m_Protocol; }
	inline const CString& GetPassPhrase() const noexcept { return m_PassPhrase; }
	inline RelayHop GetRelayHops() const noexcept { return m_Hops; }
	inline const std::optional<PeerLUID>& GetRelayGatewayPeer() const noexcept { return m_RelayGatewayPeer; }
	inline bool GetReuseConnection() noexcept { return m_ReuseConnection; }

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();

private:
	IPAddress m_IPAddress;
	UInt16 m_Port{ 9000 };
	IPEndpoint::Protocol m_Protocol{ IPEndpoint::Protocol::Unspecified };
	CString m_PassPhrase;
	RelayHop m_Hops{ 0 };
	std::optional<PeerLUID> m_RelayGatewayPeer;
	bool m_ReuseConnection{ true };
	bool m_ShowRelay{ false };
	bool m_ProtocolSelection{ true };
};

