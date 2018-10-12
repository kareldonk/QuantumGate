// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Network\SocketBase.h"

#include <queue>

namespace QuantumGate::Implementation::Core::Relay
{
	class Socket : public Network::SocketBase
	{
		friend class Manager;

		using RelayDataQueue = std::queue<Buffer>;

	public:
		Socket() noexcept;
		Socket(const Socket&) = delete;
		Socket(Socket&&) = default;
		virtual ~Socket();
		Socket& operator=(const Socket&) = delete;
		Socket& operator=(Socket&&) = default;

		inline void SetRelays(Manager* relays) noexcept { m_RelayManager = relays; }

		[[nodiscard]] const bool BeginAccept(const RelayPort rport, const RelayHop hop,
										const IPEndpoint& lendpoint, const IPEndpoint& pendpoint) noexcept;
		[[nodiscard]] const bool CompleteAccept() noexcept;

		[[nodiscard]] const bool BeginConnect(const IPEndpoint& endpoint) noexcept override;
		[[nodiscard]] const bool CompleteConnect() noexcept override;

		[[nodiscard]] const bool Send(Buffer& buffer) noexcept override;
		[[nodiscard]] const bool Receive(Buffer& buffer) noexcept override;

		void Close(const bool linger = false) noexcept override;

		inline const Network::SocketIOStatus& GetIOStatus() const noexcept override { return m_IOStatus; }
		[[nodiscard]] const bool UpdateIOStatus(const std::chrono::milliseconds& mseconds,
												const UInt8 ioupdate = IOStatusUpdate::All) noexcept override;

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

		inline void SetConnectingCallback(Network::SocketConnectingCallback&& callback) noexcept override
		{
			m_ConnectingCallback = std::move(callback);
		}

		inline void SetAcceptCallback(Network::SocketAcceptCallback&& callback) noexcept override
		{
			m_AcceptCallback = std::move(callback);
		}

		inline void SetConnectCallback(Network::SocketConnectCallback&& callback) noexcept override
		{
			m_ConnectCallback = std::move(callback);
		}

		inline void SetCloseCallback(Network::SocketCloseCallback&& callback) noexcept override
		{
			m_CloseCallback = std::move(callback);
		}

	private:
		void SetLocalEndpoint(const IPEndpoint& endpoint, const RelayPort rport, const RelayHop hop) noexcept;

		const bool AddToReceiveQueue(Buffer&& buffer) noexcept;

		inline void SetException(const Int errorcode) noexcept
		{
			m_IOStatus.SetException(true);
			m_IOStatus.SetErrorCode(errorcode);
		}

		inline void SetWrite() noexcept { m_IOStatus.SetWrite(true); }
		inline void SetRead() noexcept { m_ClosingRead = true; }

	private:
		Network::SocketIOStatus m_IOStatus;

		Manager* m_RelayManager{ nullptr };
		bool m_ClosingRead{ false };

		Size m_BytesReceived{ 0 };
		Size m_BytesSent{ 0 };

		IPEndpoint m_LocalEndpoint;
		IPEndpoint m_PeerEndpoint;

		SteadyTime m_ConnectedSteadyTime;

		RelayDataQueue m_ReceiveQueue;

		Network::SocketConnectingCallback m_ConnectingCallback{ []() noexcept {} };
		Network::SocketAcceptCallback m_AcceptCallback{ []() noexcept {} };
		Network::SocketConnectCallback m_ConnectCallback{ []() noexcept -> const bool { return true; } };
		Network::SocketCloseCallback m_CloseCallback{ []() noexcept {} };
	};
}