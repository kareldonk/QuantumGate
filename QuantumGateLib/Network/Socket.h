// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SocketBase.h"

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

		[[nodiscard]] const bool Bind(const IPEndpoint& endpoint, const bool nat_traversal) noexcept;

		[[nodiscard]] const bool Listen(const IPEndpoint& endpoint, const bool cond_accept,
										const bool nat_traversal) noexcept;

		[[nodiscard]] const bool Accept(Socket& s, const bool cond_accept = false,
										const LPCONDITIONPROC cond_func = nullptr, void* cbdata = nullptr) noexcept;

		[[nodiscard]] const bool BeginConnect(const IPEndpoint& endpoint) noexcept override;
		[[nodiscard]] const bool CompleteConnect() noexcept override;

		[[nodiscard]] const bool Send(Buffer& buffer) noexcept override;
		[[nodiscard]] const bool SendTo(const IPEndpoint& endpoint, Buffer& buffer) noexcept override;
		[[nodiscard]] const bool Receive(Buffer& buffer) noexcept override;
		[[nodiscard]] const bool ReceiveFrom(IPEndpoint& endpoint, Buffer& buffer) noexcept override;

		void Close(const bool linger = false) noexcept override;

		inline const IOStatus& GetIOStatus() const noexcept override { return m_IOStatus; }
		[[nodiscard]] const bool UpdateIOStatus(const std::chrono::milliseconds& mseconds,
												const IOStatus::Update ioupdate = IOStatus::Update::All) noexcept override;

		const SystemTime GetConnectedTime() const noexcept override;
		inline const SteadyTime& GetConnectedSteadyTime() const noexcept override { return m_ConnectedSteadyTime; }
		inline const Size GetBytesReceived() const noexcept override { return m_BytesReceived; }
		inline const Size GetBytesSent() const noexcept override { return m_BytesSent; }

		inline const IPEndpoint& GetLocalEndpoint() const noexcept override { return m_LocalEndpoint; }
		inline const IPAddress& GetLocalIPAddress() const noexcept override { return m_LocalEndpoint.GetIPAddress(); }
		inline const UInt32 GetLocalPort() const noexcept override { return m_LocalEndpoint.GetPort(); }
		inline const String GetLocalName() const noexcept override { return m_LocalEndpoint.GetString(); }

		inline const IPEndpoint& GetPeerEndpoint() const noexcept override { return m_PeerEndpoint; }
		inline const IPAddress& GetPeerIPAddress() const noexcept override { return m_PeerEndpoint.GetIPAddress(); }
		inline const UInt32 GetPeerPort() const noexcept override { return m_PeerEndpoint.GetPort(); }
		inline const String GetPeerName() const noexcept override { return m_PeerEndpoint.GetString(); }

		[[nodiscard]] const bool SetBlockingMode(const bool blocking) noexcept;

		[[nodiscard]] const bool SetExclusiveAddressUse(const bool exclusive) noexcept;
		[[nodiscard]] const bool GetExclusiveAddressUse() const noexcept;

		[[nodiscard]] const bool SetSendTimeout(const std::chrono::milliseconds& milliseconds) noexcept;
		[[nodiscard]] const bool SetReceiveTimeout(const std::chrono::milliseconds& milliseconds) noexcept;

		[[nodiscard]] const bool SetIPTimeToLive(const std::chrono::seconds& seconds) noexcept;

		[[nodiscard]] const bool SetReuseAddress(const bool reuse) noexcept;
		[[nodiscard]] const bool SetLinger(const std::chrono::seconds& seconds) noexcept;
		[[nodiscard]] const bool SetNATTraversal(const bool nat_traversal) noexcept;
		[[nodiscard]] const bool SetConditionalAccept(const bool cond_accept) noexcept;

		IP::AddressFamily GetAddressFamily() const noexcept;
		Type GetType() const noexcept;
		IP::Protocol GetProtocol() const noexcept;

		Size GetMaxDatagramMessageSize() const noexcept;
		Size GetSendBufferSize() const noexcept;
		Size GetReceiveBufferSize() const noexcept;

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

		[[nodiscard]] static const bool SockAddrSetEndpoint(sockaddr_storage& addr, const IPEndpoint& endpoint) noexcept;
		[[nodiscard]] static const bool SockAddrGetIPEndpoint(const sockaddr_storage* addr, IPEndpoint& endpoint) noexcept;

		static constexpr std::chrono::seconds DefaultLingerTime{ 10 };
		static constexpr Size ReadWriteBufferSize{ 65'535 }; //64Kb

	protected:
		inline const SOCKET GetSocket() const noexcept { return m_Socket; }

	private:
		[[nodiscard]] const bool SetSocket(const SOCKET s, const bool excl_addr_use = true,
										   const bool blocking = false) noexcept;
		void UpdateSocketInfo() noexcept;

		Buffer& GetReceiveBuffer() const noexcept;

		int GetError() const noexcept;
		int GetSockOptInt(const int optname) const noexcept;

		template<bool read, bool write, bool exception>
		[[nodiscard]] const bool UpdateIOStatusImpl(const std::chrono::milliseconds& mseconds) noexcept;

	private:
		SOCKET m_Socket{ INVALID_SOCKET };
		IOStatus m_IOStatus;

		Size m_BytesReceived{ 0 };
		Size m_BytesSent{ 0 };

		IPEndpoint m_LocalEndpoint;
		IPEndpoint m_PeerEndpoint;

		SteadyTime m_ConnectedSteadyTime;

		ConnectingCallback m_ConnectingCallback{ []() noexcept {} };
		AcceptCallback m_AcceptCallback{ []() noexcept {} };
		ConnectCallback m_ConnectCallback{ []() noexcept -> const bool { return true; } };
		CloseCallback m_CloseCallback{ []() noexcept {} };
	};
}
