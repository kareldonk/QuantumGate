// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Common\Callback.h"
#include "SocketIOStatus.h"
#include "IPEndpoint.h"

namespace QuantumGate::Implementation::Network
{
	using SocketConnectingCallback = Callback<void(void) noexcept>;
	using SocketAcceptCallback = Callback<void(void) noexcept>;
	using SocketConnectCallback = Callback<const bool(void) noexcept>;
	using SocketCloseCallback = Callback<void(void) noexcept>;

	class Export SocketBase
	{
	public:
		struct IOStatusUpdate
		{
			static constexpr UInt8 Read{ 0b00000001 };
			static constexpr UInt8 Write{ 0b00000010 };
			static constexpr UInt8 Exception{ 0b00000100 };
			static constexpr UInt8 All{ 0b11111111 };
		};

		SocketBase() = default;
		SocketBase(const SocketBase&) = default;
		SocketBase(SocketBase&&) = default;
		virtual ~SocketBase() = default;
		SocketBase& operator=(const SocketBase&) = default;
		SocketBase& operator=(SocketBase&&) = default;

		virtual const bool BeginConnect(const IPEndpoint& endpoint) noexcept = 0;
		virtual const bool CompleteConnect() noexcept = 0;

		virtual const bool Send(Buffer& buffer) noexcept = 0;
		virtual const bool Receive(Buffer& buffer) noexcept = 0;

		virtual void Close(const bool linger = false) noexcept = 0;

		virtual const SocketIOStatus& GetIOStatus() const noexcept = 0;
		virtual const bool UpdateIOStatus(const std::chrono::milliseconds& mseconds,
										  const UInt8 ioupdate = IOStatusUpdate::All) noexcept = 0;

		virtual const SystemTime GetConnectedTime() const noexcept = 0;
		virtual const SteadyTime& GetConnectedSteadyTime() const noexcept = 0;
		virtual const Size GetBytesReceived() const noexcept = 0;
		virtual const Size GetBytesSent() const noexcept = 0;

		virtual const IPEndpoint& GetLocalEndpoint() const noexcept = 0;
		virtual const IPAddress& GetLocalIPAddress() const noexcept = 0;
		virtual const String GetLocalName() const noexcept = 0;
		virtual const UInt32 GetLocalPort() const noexcept = 0;

		virtual const IPEndpoint& GetPeerEndpoint() const noexcept = 0;
		virtual const IPAddress& GetPeerIPAddress() const noexcept = 0;
		virtual const UInt32 GetPeerPort() const noexcept = 0;
		virtual const String GetPeerName() const noexcept = 0;

		virtual void SetConnectingCallback(SocketConnectingCallback&& callback) noexcept = 0;
		virtual void SetAcceptCallback(SocketAcceptCallback&& callback) noexcept = 0;
		virtual void SetConnectCallback(SocketConnectCallback&& callback) noexcept = 0;
		virtual void SetCloseCallback(SocketCloseCallback&& callback) noexcept = 0;
	};
}