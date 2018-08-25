// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SocketBase.h"

namespace QuantumGate::Implementation::Network
{
	class Export Socket : public SocketBase
	{
	public:
		Socket() noexcept;
		Socket(SOCKET s) noexcept;
		Socket(const IPAddressFamily af, const Int32 type, const Int32 protocol) noexcept;
		Socket(const Socket&) = delete;
		Socket(Socket&& other) noexcept;
		virtual ~Socket();
		Socket& operator=(const Socket&) = delete;
		Socket& operator=(Socket&& other) noexcept;

		[[nodiscard]] const bool Listen(const IPEndpoint& endpoint, const bool cond_accept,
										const bool nat_traversal) noexcept;

		[[nodiscard]] const bool Accept(Socket& s, const bool cond_accept = false,
										const LPCONDITIONPROC cond_func = nullptr, void* cbdata = nullptr) noexcept;

		[[nodiscard]] const bool BeginConnect(const IPEndpoint& endpoint) noexcept override;
		[[nodiscard]] const bool CompleteConnect() noexcept override;

		[[nodiscard]] const bool Send(Buffer& buffer) noexcept override;
		[[nodiscard]] const bool Receive(Buffer& buffer) noexcept override;

		void Close(const bool linger = false) noexcept override;

		inline const SocketIOStatus& GetIOStatus() const noexcept override { return m_IOStatus; }
		[[nodiscard]] const bool UpdateIOStatus(const std::chrono::milliseconds& mseconds) noexcept override;

		const SystemTime GetConnectedTime() const noexcept override;
		inline const SteadyTime& GetConnectedSteadyTime() const noexcept override { return m_ConnectedSteadyTime; }
		inline const Size GetBytesReceived() const noexcept override { return m_BytesReceived; }
		inline const Size GetBytesSent() const noexcept override { return m_BytesSent; }

		inline const IPEndpoint& GetLocalEndpoint() const noexcept override { return m_LocalEndpoint; }
		inline const IPAddress& GetLocalIPAddress() const noexcept override { return m_LocalEndpoint.GetIPAddress(); }
		inline const String GetLocalName() const noexcept override { return m_LocalEndpoint.GetString(); }
		inline const UInt32 GetLocalPort() const noexcept override { return m_LocalEndpoint.GetPort(); }

		inline const IPEndpoint& GetPeerEndpoint() const noexcept override { return m_PeerEndpoint; }
		inline const IPAddress& GetPeerIPAddress() const noexcept override { return m_PeerEndpoint.GetIPAddress(); }
		inline const UInt32 GetPeerPort() const noexcept override { return m_PeerEndpoint.GetPort(); }
		inline const String GetPeerName() const noexcept override { return m_PeerEndpoint.GetString(); }

		inline void SetOnConnectingCallback(SocketOnConnectingCallback&& callback) noexcept override
		{
			m_OnConnectingCallback = std::move(callback);
		}

		inline void SetOnAcceptCallback(SocketOnAcceptCallback&& callback) noexcept override
		{
			m_OnAcceptCallback = std::move(callback);
		}

		inline void SetOnConnectCallback(SocketOnConnectCallback&& callback) noexcept override
		{
			m_OnConnectCallback = std::move(callback);
		}

		inline void SetOnCloseCallback(SocketOnCloseCallback&& callback) noexcept override
		{
			m_OnCloseCallback = std::move(callback);
		}

		[[nodiscard]] static const bool SockAddrFill(sockaddr_storage& addr, const IPEndpoint& endpoint) noexcept;
		[[nodiscard]] static const bool SockAddrGetIPEndpoint(const sockaddr_storage* addr, IPEndpoint& endpoint) noexcept;

		static IPAddressFamily SockOptGetAddressFamily(const SOCKET s) noexcept;
		static const int SockOptGetProtocol(const SOCKET s) noexcept;
		static const int SockOptGetType(const SOCKET s) noexcept;
		static const int SockOptGetMaxDGramMsgSize(const SOCKET s) noexcept;
		static const int SockOptGetSendBufferSize(const SOCKET s) noexcept;
		static const int SockOptGetReceiveBufferSize(const SOCKET s) noexcept;
		static const int SockOptGetExclusiveAddressUse(const SOCKET s) noexcept;
		static const int SockOptGetError(const SOCKET s) noexcept;
		static const int SockOptGetInt(const SOCKET s, const int optname) noexcept;

		static constexpr std::chrono::seconds LingerTime{ 10 };
		static constexpr Size ReadWriteBufferSize{ 65'536 }; //64Kb

	protected:
		inline const SOCKET GetSocket() const noexcept { return m_Socket; }

	private:
		[[nodiscard]] const bool SetSocket(const SOCKET s, const bool excl_addr_use = true,
										   const bool blocking = false) noexcept;
		void UpdateSocketInfo() noexcept;

		Buffer& GetReceiveBuffer() const noexcept;

		const bool SockOptSetBlockingMode(const bool blocking) noexcept;
		const bool SockOptSetExclusiveAddressUse(const bool exclusive) noexcept;
		const bool SockOptSetReuseAddress(const bool reuse) noexcept;
		const bool SockOptSetLinger(const std::chrono::seconds& seconds) noexcept;

	private:
		SOCKET m_Socket{ INVALID_SOCKET };
		SocketIOStatus m_IOStatus;

		Size m_BytesReceived{ 0 };
		Size m_BytesSent{ 0 };

		IPEndpoint m_LocalEndpoint;
		IPEndpoint m_PeerEndpoint;
		IPAddressFamily m_AddressFamily{ IPAddressFamily::Unknown };
		Int32 m_Protocol{ 0 };
		Int32 m_Type{ 0 };
		Size m_MaxDGramMsgSize{ 0 };

		SteadyTime m_ConnectedSteadyTime;

		SocketOnConnectingCallback m_OnConnectingCallback{ []() noexcept {} };
		SocketOnAcceptCallback m_OnAcceptCallback{ []() noexcept {} };
		SocketOnConnectCallback m_OnConnectCallback{ []() noexcept -> const bool { return true; } };
		SocketOnCloseCallback m_OnCloseCallback{ []() noexcept {} };
	};
}
