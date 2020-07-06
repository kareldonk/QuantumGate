// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\Containers.h"
#include "..\..\Common\RateLimit.h"
#include "..\..\Network\SocketBase.h"
#include "..\..\Concurrency\Event.h"
#include "..\Message.h"

namespace QuantumGate::Implementation::Core::Relay
{
	class Socket final : public Network::SocketBase
	{
		friend class Manager;

	public:
		class IOBuffer final
		{
		public:
			IOBuffer() noexcept = delete;
			
			IOBuffer(Buffer& buffer, Concurrency::Event& event) noexcept :
				m_Buffer(buffer), m_Event(event)
			{}

			IOBuffer(const IOBuffer&) = delete;
			IOBuffer(IOBuffer&&) noexcept = default;
			
			~IOBuffer()
			{
				if (!m_Buffer.IsEmpty()) m_Event.Set();
				else m_Event.Reset();
			}
			
			IOBuffer& operator=(const IOBuffer&) = delete;
			IOBuffer& operator=(IOBuffer&&) noexcept = default;

			inline Buffer* operator->() noexcept { return &m_Buffer; }

			inline Buffer& operator*() noexcept { return m_Buffer; }

		private:
			Buffer& m_Buffer;
			Concurrency::Event& m_Event;
		};

		Socket() noexcept;
		Socket(const Socket&) = delete;
		Socket(Socket&&) noexcept = default;
		virtual ~Socket();
		Socket& operator=(const Socket&) = delete;
		Socket& operator=(Socket&&) noexcept = default;

		[[nodiscard]] inline Concurrency::Event& GetReceiveEvent() noexcept { return m_ReceiveEvent; }
		[[nodiscard]] inline const Concurrency::Event& GetReceiveEvent() const noexcept { return m_ReceiveEvent; }
		[[nodiscard]] inline Concurrency::Event& GetSendEvent() noexcept { return m_SendEvent; }
		[[nodiscard]] inline const Concurrency::Event& GetSendEvent() const noexcept { return m_SendEvent; }

		[[nodiscard]] bool BeginAccept(const RelayPort rport, const RelayHop hop,
									   const IPEndpoint& lendpoint, const IPEndpoint& pendpoint) noexcept;
		[[nodiscard]] bool CompleteAccept() noexcept;

		[[nodiscard]] bool BeginConnect(const IPEndpoint& endpoint) noexcept override;
		[[nodiscard]] bool CompleteConnect() noexcept override;

		[[nodiscard]] bool Send(Buffer& buffer, const Size max_snd_size = 0) noexcept override;
		[[nodiscard]] bool SendTo(const IPEndpoint& endpoint, Buffer& buffer, const Size max_snd_size = 0) noexcept override { return false; }
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
		inline void SetRelayManager(Manager* manager) noexcept { m_RelayManager = manager; }
		void SetLocalEndpoint(const IPEndpoint& endpoint, const RelayPort rport, const RelayHop hop) noexcept;

		[[nodiscard]] inline IOBuffer GetSendBuffer() noexcept { return { m_SendBuffer, m_SendEvent }; }
		[[nodiscard]] inline IOBuffer GetReceiveBuffer() noexcept { return { m_ReceiveBuffer, m_ReceiveEvent }; }

		inline void SetException(const Int errorcode) noexcept
		{
			m_IOStatus.SetException(true);
			m_IOStatus.SetErrorCode(errorcode);
		}

		inline void SetWrite() noexcept { m_IOStatus.SetWrite(true); }
		inline void SetRead() noexcept { m_ClosingRead = true; m_ReceiveEvent.Set(); }

		inline void SetMaxSendBufferSize(const Size size) noexcept
		{
			auto msbs = (std::max)(size, MinSendBufferSize);
			msbs = (std::min)(msbs, RelayDataMessage::MaxMessageDataSize);

			if (m_MaxSendBufferSize != msbs)
			{
				m_MaxSendBufferSize = msbs;

				LogDbg(L"New relay socket send buffer size is %zu", m_MaxSendBufferSize);
			}
		}

	private:
		static constexpr Size MinSendBufferSize{ 1u << 16 }; // 65KB

	private:
		IOStatus m_IOStatus;

		Manager* m_RelayManager{ nullptr };
		bool m_ClosingRead{ false };

		Size m_BytesReceived{ 0 };
		Size m_BytesSent{ 0 };

		IPEndpoint m_LocalEndpoint;
		IPEndpoint m_PeerEndpoint;

		SteadyTime m_ConnectedSteadyTime;
		
		Size m_MaxSendBufferSize{ MinSendBufferSize };
		Buffer m_SendBuffer;
		Concurrency::Event m_SendEvent;
		Buffer m_ReceiveBuffer;
		Concurrency::Event m_ReceiveEvent;

		ConnectingCallback m_ConnectingCallback{ []() mutable noexcept {} };
		AcceptCallback m_AcceptCallback{ []() mutable noexcept {} };
		ConnectCallback m_ConnectCallback{ []() mutable noexcept -> bool { return true; } };
		CloseCallback m_CloseCallback{ []() mutable noexcept {} };
	};
}