// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\TCP\TCPSocket.h"
#include "..\UDP\UDPSocket.h"
#include "..\BTH\BTHSocket.h"
#include "..\Relay\RelaySocket.h"

namespace QuantumGate::Implementation::Core::Peer
{
	using namespace QuantumGate::Implementation::Network;

	enum class GateType
	{
		Unknown, TCPSocket, UDPSocket, BTHSocket, RelaySocket
	};

	class Gate
	{
	public:
		Gate(const GateType type) noexcept;
		Gate(const AddressFamily af, const Protocol protocol);
		Gate(const Gate&) = delete;
		Gate(Gate&&) noexcept = default;
		virtual ~Gate();
		Gate& operator=(const Gate&) = delete;
		Gate& operator=(Gate&&) noexcept = default;

		template<typename T>
		[[nodiscard]] T& GetSocket() noexcept { return *dynamic_cast<T*>(m_Socket); }

		template<typename T>
		[[nodiscard]] const T& GetSocket() const noexcept { return *dynamic_cast<const T*>(m_Socket); }

		[[nodiscard]] GateType GetGateType() const noexcept { return m_Type; }

		[[nodiscard]] bool BeginConnect(const Endpoint& endpoint) noexcept
		{
			assert(m_Socket); return m_Socket->BeginConnect(endpoint);
		}

		[[nodiscard]] bool CompleteConnect() noexcept { assert(m_Socket); return m_Socket->CompleteConnect(); }

		[[nodiscard]] Result<Size> Send(const BufferView& buffer) noexcept { assert(m_Socket); return m_Socket->Send(buffer); }
		[[nodiscard]] Result<Size> Receive(Buffer& buffer) noexcept { assert(m_Socket); return m_Socket->Receive(buffer); }

		void Close(const bool linger = false) noexcept { assert(m_Socket); return m_Socket->Close(linger); }

		[[nodiscard]] const Socket::IOStatus& GetIOStatus() const noexcept { assert(m_Socket); return m_Socket->GetIOStatus(); }

		[[nodiscard]] bool UpdateIOStatus(const std::chrono::milliseconds& mseconds) noexcept
		{
			assert(m_Socket); return m_Socket->UpdateIOStatus(mseconds);
		}

		[[nodiscard]] bool CanSuspend() const noexcept { assert(m_Socket); return m_Socket->CanSuspend(); }
		[[nodiscard]] std::optional<SteadyTime> GetLastSuspendedSteadyTime() const noexcept { assert(m_Socket); return m_Socket->GetLastSuspendedSteadyTime(); }
		[[nodiscard]] std::optional<SteadyTime> GetLastResumedSteadyTime() const noexcept { assert(m_Socket); return m_Socket->GetLastResumedSteadyTime(); }

		[[nodiscard]] SystemTime GetConnectedTime() const noexcept { assert(m_Socket); return m_Socket->GetConnectedTime(); }

		[[nodiscard]] const SteadyTime& GetConnectedSteadyTime() const noexcept
		{
			assert(m_Socket); return m_Socket->GetConnectedSteadyTime();
		}

		[[nodiscard]] Size GetBytesReceived() const noexcept { assert(m_Socket); return m_Socket->GetBytesReceived(); }
		[[nodiscard]] Size GetBytesSent() const noexcept { assert(m_Socket); return m_Socket->GetBytesSent(); }

		[[nodiscard]] const Endpoint& GetLocalEndpoint() const noexcept { assert(m_Socket); return m_Socket->GetLocalEndpoint(); }
		[[nodiscard]] virtual String GetLocalName() const noexcept { assert(m_Socket); return m_Socket->GetLocalName(); }

		[[nodiscard]] const Endpoint& GetPeerEndpoint() const noexcept { assert(m_Socket); return m_Socket->GetPeerEndpoint(); }
		[[nodiscard]] virtual String GetPeerName() const noexcept { assert(m_Socket); return m_Socket->GetPeerName(); }

	protected:
		virtual void OnConnecting() noexcept {}
		virtual void OnAccept() noexcept {}
		virtual bool OnConnect() noexcept { return true; }
		virtual void OnClose() noexcept {}

	private:
		void SetCallbacks() noexcept;

	private:
		static constexpr Size SocketStorageSize = std::max(std::max(sizeof(Socket), sizeof(UDP::Socket)), sizeof(Relay::Socket));

	private:
		SocketBase* m_Socket{ nullptr };
		typename std::aligned_storage<SocketStorageSize>::type m_SocketStorage{ 0 };
		GateType m_Type{ GateType::Unknown };
	};
}