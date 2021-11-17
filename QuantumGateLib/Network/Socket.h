// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SocketBase.h"
#include "..\Memory\StackBuffer.h"

#define USE_SOCKET_EVENT

#ifdef USE_SOCKET_EVENT
#include "..\Concurrency\Event.h"
#endif

namespace QuantumGate::Implementation::Network
{
	class Export Socket : public SocketBase
	{
		using ReceiveBuffer = Memory::StackBuffer65K;

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
		Socket(const AddressFamily af, const Type type, const Protocol protocol);
		Socket(const IP::AddressFamily af, const Type type, const IP::Protocol protocol);
		Socket(const BTH::AddressFamily af, const Type type, const BTH::Protocol protocol);
		Socket(const Socket&) = delete;
		Socket(Socket&& other) noexcept;
		virtual ~Socket();
		Socket& operator=(const Socket&) = delete;
		Socket& operator=(Socket&& other) noexcept;

		[[nodiscard]] inline SOCKET GetHandle() const noexcept { assert(m_Socket != INVALID_SOCKET); return m_Socket; }

#ifdef USE_SOCKET_EVENT
		[[nodiscard]] inline Concurrency::Event& GetEvent() noexcept { return m_Event; }
		[[nodiscard]] inline const Concurrency::Event& GetEvent() const noexcept { return m_Event; }
#endif

		[[nodiscard]] AddressFamily GetAddressFamily() const noexcept;
		[[nodiscard]] Type GetType() const noexcept;
		[[nodiscard]] Protocol GetProtocol() const noexcept;

		[[nodiscard]] bool Bind(const Endpoint& endpoint, const bool nat_traversal) noexcept;

		[[nodiscard]] bool Listen(const Endpoint& endpoint, const bool cond_accept,
								  const bool nat_traversal) noexcept;

		[[nodiscard]] bool Accept(Socket& s, const bool cond_accept = false,
								  const LPCONDITIONPROC cond_func = nullptr, void* cbdata = nullptr) noexcept;

		[[nodiscard]] bool BeginConnect(const Endpoint& endpoint) noexcept override;
		[[nodiscard]] bool CompleteConnect() noexcept override;

		[[nodiscard]] Result<Size> Send(const BufferView& buffer, const Size max_snd_size = 0) noexcept override;
		[[nodiscard]] Result<Size> SendTo(const Endpoint& endpoint, const BufferView& buffer, const Size max_snd_size = 0) noexcept override;
		[[nodiscard]] Result<Size> Receive(Buffer& buffer, const Size max_rcv_size = 0) noexcept override;
		[[nodiscard]] Result<Size> Receive(BufferSpan& buffer) noexcept;
		[[nodiscard]] Result<Size> ReceiveFrom(Endpoint& endpoint, Buffer& buffer, const Size max_rcv_size = 0) noexcept override;
		[[nodiscard]] Result<Size> ReceiveFrom(Endpoint& endpoint, BufferSpan& buffer) noexcept;

		void Close(const bool linger = false) noexcept override;

		[[nodiscard]] inline const IOStatus& GetIOStatus() const noexcept override { return m_IOStatus; }
		[[nodiscard]] bool UpdateIOStatus(const std::chrono::milliseconds& mseconds) noexcept override;

		[[nodiscard]] bool CanSuspend() const noexcept override { return false; }
		[[nodiscard]] std::optional<SteadyTime> GetLastSuspendedSteadyTime() const noexcept override { return std::nullopt; }
		[[nodiscard]] std::optional<SteadyTime> GetLastResumedSteadyTime() const noexcept override { return std::nullopt; }

		[[nodiscard]] SystemTime GetConnectedTime() const noexcept override;
		[[nodiscard]] inline const SteadyTime& GetConnectedSteadyTime() const noexcept override { return m_ConnectedSteadyTime; }

		[[nodiscard]] inline Size GetBytesReceived() const noexcept override { return m_BytesReceived; }
		[[nodiscard]] inline Size GetBytesSent() const noexcept override { return m_BytesSent; }

		[[nodiscard]] inline const Endpoint& GetLocalEndpoint() const noexcept override { return m_LocalEndpoint; }
		[[nodiscard]] inline const IPAddress& GetLocalIPAddress() const noexcept override { return m_LocalEndpoint.GetIPEndpoint().GetIPAddress(); }
		[[nodiscard]] inline UInt32 GetLocalPort() const noexcept override { return m_LocalEndpoint.GetIPEndpoint().GetPort(); }
		[[nodiscard]] inline String GetLocalName() const noexcept override { return m_LocalEndpoint.GetString(); }

		[[nodiscard]] inline const Endpoint& GetPeerEndpoint() const noexcept override { return m_PeerEndpoint; }
		[[nodiscard]] inline const IPAddress& GetPeerIPAddress() const noexcept override { return m_PeerEndpoint.GetIPEndpoint().GetIPAddress(); }
		[[nodiscard]] inline UInt32 GetPeerPort() const noexcept override { return m_PeerEndpoint.GetIPEndpoint().GetPort(); }
		[[nodiscard]] inline String GetPeerName() const noexcept override { return m_PeerEndpoint.GetString(); }

		[[nodiscard]] bool SetBlockingMode(const bool blocking) noexcept;

		[[nodiscard]] bool SetReuseAddress(const bool reuse) noexcept;
		[[nodiscard]] bool SetExclusiveAddressUse(const bool exclusive) noexcept;
		[[nodiscard]] Result<bool> GetExclusiveAddressUse() const noexcept;

		[[nodiscard]] bool SetSendTimeout(const std::chrono::milliseconds& milliseconds) noexcept;
		[[nodiscard]] bool SetReceiveTimeout(const std::chrono::milliseconds& milliseconds) noexcept;

		[[nodiscard]] bool SetIPTimeToLive(const std::chrono::seconds& seconds) noexcept;

		[[nodiscard]] bool SetLinger(const std::chrono::seconds& seconds) noexcept;
		
		[[nodiscard]] bool SetNATTraversal(const bool nat_traversal) noexcept;
		Result<bool> GetNATTraversal() noexcept;

		[[nodiscard]] bool SetConditionalAccept(const bool cond_accept) noexcept;
		[[nodiscard]] bool SetNoDelay(const bool no_delay) noexcept;

		[[nodiscard]] bool SetMTUDiscovery(const bool enabled) noexcept;
		[[nodiscard]] Result<bool> IsMTUDiscoveryEnabled() noexcept;

		[[nodiscard]] bool SetSendBufferSize(const int len) noexcept;
		[[nodiscard]] bool SetReceiveBufferSize(const int len) noexcept;

		[[nodiscard]] Result<int> GetMaxDatagramMessageSize() const noexcept;
		[[nodiscard]] Result<int> GetSendBufferSize() const noexcept;
		[[nodiscard]] Result<int> GetReceiveBufferSize() const noexcept;

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

		[[nodiscard]] static bool SockAddrSetEndpoint(sockaddr_storage& addr, const Endpoint& endpoint) noexcept;
		[[nodiscard]] static bool SockAddrGetEndpoint(const Protocol protocol, const sockaddr_storage* addr, Endpoint& endpoint) noexcept;

		static constexpr std::chrono::seconds DefaultLingerTime{ 10 };

	private:
		[[nodiscard]] bool SetSocket(const SOCKET s, const bool excl_addr_use = true,
									 const bool blocking = false) noexcept;
		void UpdateSocketInfo() noexcept;

		void Release() noexcept;

#ifdef USE_SOCKET_EVENT
		[[nodiscard]] bool AttachEvent() noexcept;
		void DetachEvent() noexcept;
#endif

		[[nodiscard]] ReceiveBuffer& GetReceiveBuffer() const noexcept;

		[[nodiscard]] int GetError() const noexcept;
		[[nodiscard]] int GetSockOptInt(const int optname) const noexcept;
		[[nodiscard]] int GetOptInt(const int level, const int optname) const noexcept;

		[[nodiscard]] bool UpdateIOStatusFDSet(const std::chrono::milliseconds& mseconds) noexcept;

#ifdef USE_SOCKET_EVENT
		[[nodiscard]] bool UpdateIOStatusEvent(const std::chrono::milliseconds& mseconds) noexcept;
#endif

	private:
		SOCKET m_Socket{ INVALID_SOCKET };
#ifdef USE_SOCKET_EVENT
		Concurrency::Event m_Event;
#endif
		IOStatus m_IOStatus;

		Size m_BytesReceived{ 0 };
		Size m_BytesSent{ 0 };

		Endpoint m_LocalEndpoint;
		Endpoint m_PeerEndpoint;

		SteadyTime m_ConnectedSteadyTime;

		ConnectingCallback m_ConnectingCallback{ []() mutable noexcept {} };
		AcceptCallback m_AcceptCallback{ []() mutable noexcept {} };
		ConnectCallback m_ConnectCallback{ []() mutable noexcept -> bool { return true; } };
		CloseCallback m_CloseCallback{ []() mutable noexcept {} };
	};
}
