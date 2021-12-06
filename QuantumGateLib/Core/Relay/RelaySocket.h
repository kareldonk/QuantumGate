// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Network\SocketBase.h"
#include "..\..\Concurrency\EventComposite.h"
#include "..\Message.h"

namespace QuantumGate::Implementation::Core::Relay
{
	class Socket final : public Network::SocketBase
	{
		friend class Manager;

		using IOEvent = Concurrency::EventComposite<2, Concurrency::EventCompositeOperatorType::AND>;

	public:
		class IOBuffer final
		{
		public:
			IOBuffer() noexcept = delete;
			
			IOBuffer(Buffer& buffer, IOEvent& event) noexcept :
				m_Buffer(buffer), m_Event(event)
			{}

			IOBuffer(const IOBuffer&) = delete;
			IOBuffer(IOBuffer&&) noexcept = default;
			
			~IOBuffer()
			{
				if (!m_Buffer.IsEmpty()) m_Event.GetSubEvent(0).Set();
				else m_Event.GetSubEvent(0).Reset();
			}
			
			IOBuffer& operator=(const IOBuffer&) = delete;
			IOBuffer& operator=(IOBuffer&&) noexcept = default;

			inline Buffer* operator->() noexcept { return &m_Buffer; }

			inline Buffer& operator*() noexcept { return m_Buffer; }

		private:
			Buffer& m_Buffer;
			IOEvent& m_Event;
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
									   const Endpoint& lendpoint, const Endpoint& pendpoint) noexcept;
		[[nodiscard]] bool CompleteAccept() noexcept;

		[[nodiscard]] bool BeginConnect(const Endpoint& endpoint) noexcept override;
		[[nodiscard]] bool CompleteConnect() noexcept override;

		[[nodiscard]] Result<Size> Send(const BufferView& buffer, const Size max_snd_size = 0) noexcept override;
		[[nodiscard]] Result<Size> SendTo(const Endpoint& endpoint, const BufferView& buffer, const Size max_snd_size = 0) noexcept override { return ResultCode::Failed; }
		[[nodiscard]] Result<Size> Receive(Buffer& buffer, const Size max_rcv_size = 0) noexcept override;
		[[nodiscard]] Result<Size> ReceiveFrom(Endpoint& endpoint, Buffer& buffer, const Size max_rcv_size = 0) noexcept override { return ResultCode::Failed; }

		void Close(const bool linger = false) noexcept override;

		[[nodiscard]] inline const IOStatus& GetIOStatus() const noexcept override { return m_IOStatus; }
		[[nodiscard]] bool UpdateIOStatus(const std::chrono::milliseconds& mseconds) noexcept override;

		[[nodiscard]] bool CanSuspend() const noexcept override { return true; }
		[[nodiscard]] std::optional<SteadyTime> GetLastSuspendedSteadyTime() const noexcept override { return m_LastSuspendedSteadyTime; }
		[[nodiscard]] std::optional<SteadyTime> GetLastResumedSteadyTime() const noexcept override { return m_LastResumedSteadyTime; }

		[[nodiscard]] SystemTime GetConnectedTime() const noexcept override;
		[[nodiscard]] inline const SteadyTime& GetConnectedSteadyTime() const noexcept override { return m_ConnectedSteadyTime; }
		[[nodiscard]] inline Size GetBytesReceived() const noexcept override { return m_BytesReceived; }
		[[nodiscard]] inline Size GetBytesSent() const noexcept override { return m_BytesSent; }

		[[nodiscard]] inline const Endpoint& GetLocalEndpoint() const noexcept override { return m_LocalEndpoint; }
		[[nodiscard]] inline String GetLocalName() const noexcept override { return m_LocalEndpoint.GetString(); }

		[[nodiscard]] inline const Endpoint& GetPeerEndpoint() const noexcept override { return m_PeerEndpoint; }
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
		void SetLocalEndpoint(const Endpoint& endpoint, const RelayPort rport, const RelayHop hop) noexcept;

		[[nodiscard]] inline IOBuffer GetSendBuffer() noexcept { return { m_SendBuffer, m_SendEvent }; }
		[[nodiscard]] inline IOBuffer GetReceiveBuffer() noexcept { return { m_ReceiveBuffer, m_ReceiveEvent }; }

		inline void SetException(const Int errorcode) noexcept
		{
			m_IOStatus.SetException(true);
			m_IOStatus.SetErrorCode(errorcode);
		}

		inline void SetSocketWrite() noexcept
		{
			m_ConnectWrite = true;
			m_ReceiveEvent.GetSubEvent(0).Set();
		}

		inline void SetSocketRead() noexcept
		{
			m_ClosingRead = true;
			m_ReceiveEvent.GetSubEvent(0).Set();
		}

		inline void SetSocketSuspended(const bool suspended) noexcept
		{
			m_IOStatus.SetSuspended(suspended);
			if (suspended)
			{
				m_LastSuspendedSteadyTime = Util::GetCurrentSteadyTime();
			}
			else
			{
				m_LastResumedSteadyTime = Util::GetCurrentSteadyTime();
			}
			m_ReceiveEvent.GetSubEvent(0).Set();
		}

		inline void SetRelayWrite(const bool enabled) noexcept
		{
			if (enabled) m_SendEvent.GetSubEvent(1).Set();
			else m_SendEvent.GetSubEvent(1).Reset();
		}

	private:
		static constexpr Size MaxSendBufferSize{ RelayDataMessage::MaxMessageDataSize };

	private:
		IOStatus m_IOStatus;

		bool m_ConnectWrite{ false };
		bool m_ClosingRead{ false };

		Size m_BytesReceived{ 0 };
		Size m_BytesSent{ 0 };

		Endpoint m_LocalEndpoint;
		Endpoint m_PeerEndpoint;

		SteadyTime m_ConnectedSteadyTime;
		std::optional<SteadyTime> m_LastSuspendedSteadyTime;
		std::optional<SteadyTime> m_LastResumedSteadyTime;

		Buffer m_SendBuffer;
		IOEvent m_SendEvent;
		Buffer m_ReceiveBuffer;
		IOEvent m_ReceiveEvent;

		ConnectingCallback m_ConnectingCallback{ []() mutable noexcept {} };
		AcceptCallback m_AcceptCallback{ []() mutable noexcept {} };
		ConnectCallback m_ConnectCallback{ []() mutable noexcept -> bool { return true; } };
		CloseCallback m_CloseCallback{ []() mutable noexcept {} };
	};
}