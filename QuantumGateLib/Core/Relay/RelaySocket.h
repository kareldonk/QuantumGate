// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\Containers.h"
#include "..\..\Common\RateLimit.h"
#include "..\..\Network\SocketBase.h"
#include "..\Message.h"

namespace QuantumGate::Implementation::Core::Relay
{
	class Socket final : public Network::SocketBase
	{
		friend class Manager;

		using RelayDataQueue = Containers::Queue<Buffer>;
		using SendRateLimit = RateLimit<Size, 0, 5 * Message::MaxMessageDataSize>;

	public:
		Socket() noexcept;
		Socket(const Socket&) = delete;
		Socket(Socket&&) noexcept = default;
		virtual ~Socket();
		Socket& operator=(const Socket&) = delete;
		Socket& operator=(Socket&&) noexcept = default;

		inline void SetRelays(Manager* relays) noexcept { m_RelayManager = relays; }

		[[nodiscard]] bool BeginAccept(const RelayPort rport, const RelayHop hop,
									   const IPEndpoint& lendpoint, const IPEndpoint& pendpoint) noexcept;
		[[nodiscard]] bool CompleteAccept() noexcept;

		[[nodiscard]] bool BeginConnect(const IPEndpoint& endpoint) noexcept override;
		[[nodiscard]] bool CompleteConnect() noexcept override;

		[[nodiscard]] bool Send(Buffer& buffer) noexcept override;
		[[nodiscard]] bool SendTo(const IPEndpoint& endpoint, Buffer& buffer) noexcept override { return false; }
		[[nodiscard]] bool Receive(Buffer& buffer, const Size max_rcv_size = 0) noexcept override;
		[[nodiscard]] bool ReceiveFrom(IPEndpoint& endpoint, Buffer& buffer, const Size max_rcv_size = 0) noexcept override { return false; }

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
		[[nodiscard]] inline String GetLocalName() const noexcept override { return m_LocalEndpoint.GetString(); }
		[[nodiscard]] inline UInt32 GetLocalPort() const noexcept override { return m_LocalEndpoint.GetPort(); }

		[[nodiscard]] inline const IPEndpoint& GetPeerEndpoint() const noexcept override { return m_PeerEndpoint; }
		[[nodiscard]] inline const IPAddress& GetPeerIPAddress() const noexcept override { return m_PeerEndpoint.GetIPAddress(); }
		[[nodiscard]] inline UInt32 GetPeerPort() const noexcept override { return m_PeerEndpoint.GetPort(); }
		[[nodiscard]] inline String GetPeerName() const noexcept override { return m_PeerEndpoint.GetString(); }

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

	private:
		void SetLocalEndpoint(const IPEndpoint& endpoint, const RelayPort rport, const RelayHop hop) noexcept;

		[[nodiscard]] bool AddToReceiveQueue(Buffer&& buffer) noexcept;

		inline void SetException(const Int errorcode) noexcept
		{
			m_IOStatus.SetException(true);
			m_IOStatus.SetErrorCode(errorcode);
		}

		inline void SetWrite() noexcept { m_IOStatus.SetWrite(true); }
		inline void SetRead() noexcept { m_ClosingRead = true; }

		inline void AddToSendRateLimit(const Size num) noexcept { m_SendRateLimit.Add(num); }
		inline void SubtractFromSendRateLimit(const Size num) noexcept { m_SendRateLimit.Subtract(num); }

	private:
		static constexpr Size MaxSendBufferDataSize{ 5 * Message::MaxMessageDataSize };

	private:
		IOStatus m_IOStatus;

		Manager* m_RelayManager{ nullptr };
		bool m_ClosingRead{ false };
		SendRateLimit m_SendRateLimit;

		Size m_BytesReceived{ 0 };
		Size m_BytesSent{ 0 };

		IPEndpoint m_LocalEndpoint;
		IPEndpoint m_PeerEndpoint;

		SteadyTime m_ConnectedSteadyTime;

		RelayDataQueue m_ReceiveQueue;

		ConnectingCallback m_ConnectingCallback{ []() mutable noexcept {} };
		AcceptCallback m_AcceptCallback{ []() mutable noexcept {} };
		ConnectCallback m_ConnectCallback{ []() mutable noexcept -> bool { return true; } };
		CloseCallback m_CloseCallback{ []() mutable noexcept {} };
	};
}