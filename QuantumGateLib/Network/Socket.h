// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SocketBase.h"

// #define USE_WSA_EVENT

namespace QuantumGate::Implementation::Network
{
	class Export Socket : public SocketBase
	{
	public:
		enum class Type
		{
			Unspecified,
			Stream,
			Datagram,
			RAW
		};

		Socket() noexcept;
		Socket(const SOCKET s);
		Socket(const IP::AddressFamily af, const Type type, const IP::Protocol protocol);
		Socket(const Socket&) = delete;
		Socket(Socket&& other) noexcept;
		virtual ~Socket();
		Socket& operator=(const Socket&) = delete;
		Socket& operator=(Socket&& other) noexcept;

		[[nodiscard]] inline SOCKET GetHandle() const noexcept { return m_Socket; }

#ifdef USE_WSA_EVENT
		[[nodiscard]] inline WSAEVENT GetWSAEvent() const noexcept { return m_WSAEvent; }
#endif

		[[nodiscard]] IP::AddressFamily GetAddressFamily() const noexcept;
		[[nodiscard]] Type GetType() const noexcept;
		[[nodiscard]] IP::Protocol GetProtocol() const noexcept;

		[[nodiscard]] bool Bind(const IPEndpoint& endpoint, const bool nat_traversal) noexcept;

		[[nodiscard]] bool Listen(const IPEndpoint& endpoint, const bool cond_accept,
								  const bool nat_traversal) noexcept;

		[[nodiscard]] bool Accept(Socket& s, const bool cond_accept = false,
								  const LPCONDITIONPROC cond_func = nullptr, void* cbdata = nullptr) noexcept;

		[[nodiscard]] bool BeginConnect(const IPEndpoint& endpoint) noexcept override;
		[[nodiscard]] bool CompleteConnect() noexcept override;

		[[nodiscard]] bool Send(Buffer& buffer, const Size max_snd_size = 0) noexcept override;
		[[nodiscard]] bool SendTo(const IPEndpoint& endpoint, Buffer& buffer, const Size max_snd_size = 0) noexcept override;
		[[nodiscard]] bool Receive(Buffer& buffer, const Size max_rcv_size = 0) noexcept override;
		[[nodiscard]] bool ReceiveFrom(IPEndpoint& endpoint, Buffer& buffer, const Size max_rcv_size = 0) noexcept override;

		void Close(const bool linger = false) noexcept override;

		[[nodiscard]] inline const IOStatus& GetIOStatus() const noexcept override { return m_IOStatus; }
		[[nodiscard]] bool UpdateIOStatus(const std::chrono::milliseconds& mseconds,
										  const IOStatus::Update ioupdate = IOStatus::Update::All) noexcept override;

		[[nodiscard]] SystemTime GetConnectedTime() const noexcept override;
		[[nodiscard]] inline const SteadyTime& GetConnectedSteadyTime() const noexcept override { return m_ConnectedSteadyTime; }

		[[nodiscard]] inline Size GetBytesReceived() const noexcept override { return m_BytesReceived; }
		[[nodiscard]] inline Size GetBytesSent() const noexcept override { return m_BytesSent; }

		[[nodiscard]] inline const IPEndpoint& GetLocalEndpoint() const noexcept override { return m_LocalEndpoint; }
		[[nodiscard]] inline const IPAddress& GetLocalIPAddress() const noexcept override { return m_LocalEndpoint.GetIPAddress(); }
		[[nodiscard]] inline UInt32 GetLocalPort() const noexcept override { return m_LocalEndpoint.GetPort(); }
		[[nodiscard]] inline String GetLocalName() const noexcept override { return m_LocalEndpoint.GetString(); }

		[[nodiscard]] inline const IPEndpoint& GetPeerEndpoint() const noexcept override { return m_PeerEndpoint; }
		[[nodiscard]] inline const IPAddress& GetPeerIPAddress() const noexcept override { return m_PeerEndpoint.GetIPAddress(); }
		[[nodiscard]] inline UInt32 GetPeerPort() const noexcept override { return m_PeerEndpoint.GetPort(); }
		[[nodiscard]] inline String GetPeerName() const noexcept override { return m_PeerEndpoint.GetString(); }

		[[nodiscard]] bool SetBlockingMode(const bool blocking) noexcept;

		[[nodiscard]] bool SetReuseAddress(const bool reuse) noexcept;
		[[nodiscard]] bool SetExclusiveAddressUse(const bool exclusive) noexcept;
		[[nodiscard]] bool GetExclusiveAddressUse() const noexcept;

		[[nodiscard]] bool SetSendTimeout(const std::chrono::milliseconds& milliseconds) noexcept;
		[[nodiscard]] bool SetReceiveTimeout(const std::chrono::milliseconds& milliseconds) noexcept;

		[[nodiscard]] bool SetIPTimeToLive(const std::chrono::seconds& seconds) noexcept;

		[[nodiscard]] bool SetLinger(const std::chrono::seconds& seconds) noexcept;
		[[nodiscard]] bool SetNATTraversal(const bool nat_traversal) noexcept;
		[[nodiscard]] bool SetConditionalAccept(const bool cond_accept) noexcept;
		[[nodiscard]] bool SetNoDelay(const bool no_delay) noexcept;

		[[nodiscard]] bool SetSendBufferSize(const int len) noexcept;
		[[nodiscard]] bool SetReceiveBufferSize(const int len) noexcept;

		[[nodiscard]] int GetMaxDatagramMessageSize() const noexcept;
		[[nodiscard]] int GetSendBufferSize() const noexcept;
		[[nodiscard]] int GetReceiveBufferSize() const noexcept;

		inline void SetConnectingCallback(ConnectingCallback&& callback) noexcept override
		{
			m_ConnectingCallback = std::move(callback);
		}

		inline void SetAcceptCallback(AcceptCallback&& callback) noexcept override
		{
			m_AcceptCallback = std::move(callback);
		}

		inline void SetConnectCallback(ConnectCallback&& callback) noexcept override
		{
			m_ConnectCallback = std::move(callback);
		}

		inline void SetCloseCallback(CloseCallback&& callback) noexcept override
		{
			m_CloseCallback = std::move(callback);
		}

		[[nodiscard]] static bool SockAddrSetEndpoint(sockaddr_storage& addr, const IPEndpoint& endpoint) noexcept;
		[[nodiscard]] static bool SockAddrGetIPEndpoint(const sockaddr_storage* addr, IPEndpoint& endpoint) noexcept;

		static constexpr std::chrono::seconds DefaultLingerTime{ 10 };
		static constexpr Size ReadWriteBufferSize{ 65'535 }; //64KB

	private:
		[[nodiscard]] bool SetSocket(const SOCKET s, const bool excl_addr_use = true,
									 const bool blocking = false) noexcept;
		void UpdateSocketInfo() noexcept;

#ifdef USE_WSA_EVENT
		[[nodiscard]] bool CreateWSAEvent() noexcept;
		void CloseWSAEvent() noexcept;
#endif

		[[nodiscard]] Buffer& GetReceiveBuffer() const noexcept;

		[[nodiscard]] int GetError() const noexcept;
		[[nodiscard]] int GetSockOptInt(const int optname) const noexcept;

		template<bool read, bool write, bool exception>
		[[nodiscard]] bool UpdateIOStatusImpl(const std::chrono::milliseconds& mseconds) noexcept;

		template<bool read, bool write, bool exception>
		[[nodiscard]] bool UpdateIOStatusFDSet(const std::chrono::milliseconds& mseconds) noexcept;

#ifdef USE_WSA_EVENT
		template<bool read, bool write, bool exception>
		[[nodiscard]] bool UpdateIOStatusWSAEvent(const std::chrono::milliseconds& mseconds) noexcept;
#endif

	private:
		SOCKET m_Socket{ INVALID_SOCKET };
#ifdef USE_WSA_EVENT
		WSAEVENT m_WSAEvent{ WSA_INVALID_EVENT };
#endif
		IOStatus m_IOStatus;

		Size m_BytesReceived{ 0 };
		Size m_BytesSent{ 0 };

		IPEndpoint m_LocalEndpoint;
		IPEndpoint m_PeerEndpoint;

		SteadyTime m_ConnectedSteadyTime;

		ConnectingCallback m_ConnectingCallback{ []() mutable noexcept {} };
		AcceptCallback m_AcceptCallback{ []() mutable noexcept {} };
		ConnectCallback m_ConnectCallback{ []() mutable noexcept -> bool { return true; } };
		CloseCallback m_CloseCallback{ []() mutable noexcept {} };
	};
}
