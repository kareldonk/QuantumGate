// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPListenerSocket.h"
#include "..\..\Concurrency\Event.h"
#include "..\..\Network\Socket.h"

namespace QuantumGate::Implementation::Core::UDP
{
	class UDPConnectionData final
	{
	public:
		UDPConnectionData(Concurrency::Event* send_event) : m_SendEvent(send_event) {}
		UDPConnectionData(const UDPConnectionData&) = delete;
		UDPConnectionData(UDPConnectionData&&) noexcept = default;
		~UDPConnectionData() = default;
		UDPConnectionData& operator=(const UDPConnectionData&) = delete;
		UDPConnectionData& operator=(UDPConnectionData&&) noexcept = default;

		inline void SignalSendEvent() noexcept { if (m_SendEvent) m_SendEvent->Set(); }
		inline void ChangeSendEvent(Concurrency::Event* send_event) noexcept { m_SendEvent = send_event; }
		inline void RemoveSendEvent() noexcept { m_SendEvent = nullptr; }

		inline void SignalReceiveEvent() noexcept { m_ReceiveEvent.Set(); }
		inline void ResetReceiveEvent() noexcept { m_ReceiveEvent.Reset(); }
		inline Concurrency::Event& GetReceiveEvent() noexcept { return m_ReceiveEvent; }
		inline const Concurrency::Event& GetReceiveEvent() const noexcept { return m_ReceiveEvent; }

		inline void SetLocalEndpoint(const IPEndpoint& endpoint) noexcept { LocalEndpoint = endpoint; }
		inline const IPEndpoint& GetLocalEndpoint() const noexcept { return LocalEndpoint; }

		inline void SetPeerEndpoint(const IPEndpoint& endpoint) noexcept { PeerEndpoint = endpoint; }
		inline const IPEndpoint& GetPeerEndpoint() const noexcept { return PeerEndpoint; }

		inline void SetRead(const bool enabled) noexcept { m_CanRead = enabled; }
		[[nodiscard]] inline bool CanRead() const noexcept { return m_CanRead; }
		inline void SetWrite(const bool enabled) noexcept { m_CanWrite = enabled; }
		[[nodiscard]] inline bool CanWrite() const noexcept { return m_CanWrite; }
		inline void SetSuspended(const bool value) noexcept { m_IsSuspended = value; }
		[[nodiscard]] bool IsSuspended() const noexcept { return m_IsSuspended; }

		inline RingBuffer& GetSendBuffer() noexcept { return m_SendBuffer; }
		inline RingBuffer& GetReceiveBuffer() noexcept { return m_ReceiveBuffer; }

		inline void SetConnectRequest() noexcept
		{
			m_Connect = true;
			SignalSendEvent();
		}

		[[nodiscard]] inline bool HasConnectRequest() const noexcept { return m_Connect; }

		inline void SetCloseRequest() noexcept
		{
			m_Close = true;
			SignalSendEvent();
		}

		[[nodiscard]] inline bool HasCloseRequest() const noexcept { return m_Close; }

		inline void SetException(const int error_code) noexcept
		{
			m_HasException = true;
			m_ErrorCode = error_code;
		}

		[[nodiscard]] inline bool HasException() const noexcept { return m_HasException; }
		[[nodiscard]] inline int GetErrorCode() const noexcept { return m_ErrorCode; }

		void SetListenerSendQueue(const std::shared_ptr<Listener::SendQueue_ThS>& send_queue) noexcept { m_ListenerSendQueue = send_queue; }
		[[nodiscard]] bool HasListenerSendQueue() const noexcept { return m_ListenerSendQueue.operator bool(); }
		[[nodiscard]] const std::shared_ptr<Listener::SendQueue_ThS>& GetListenerSendQueue() const noexcept { return m_ListenerSendQueue; }
		void ReleaseListenerSendQueue() noexcept { m_ListenerSendQueue.reset(); }

	private:
		bool m_CanRead{ false };
		bool m_CanWrite{ false };
		bool m_IsSuspended{ false };
		bool m_HasException{ false };
		int m_ErrorCode{ 0 };

		bool m_Connect{ false };
		bool m_Close{ false };

		IPEndpoint LocalEndpoint;
		IPEndpoint PeerEndpoint;

		RingBuffer m_SendBuffer{ 1u << 20 };	// 1MB
		RingBuffer m_ReceiveBuffer{ 1u << 20 }; // 1MB
		Concurrency::Event m_ReceiveEvent;
		Concurrency::Event* m_SendEvent{ nullptr };

		std::shared_ptr<Listener::SendQueue_ThS> m_ListenerSendQueue;
	};

	using ConnectionData_ThS = Concurrency::ThreadSafe<UDPConnectionData, std::shared_mutex>;
}