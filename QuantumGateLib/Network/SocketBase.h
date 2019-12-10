// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IP.h"
#include "IPEndpoint.h"
#include "..\Common\Callback.h"

#include <bitset>

namespace QuantumGate::Implementation::Network
{
	class SocketException : public std::exception
	{
	public:
		SocketException(const char* message) noexcept : std::exception(message) {}
	};

	class Export SocketBase
	{
	public:
		class IOStatus
		{
			enum class StatusType : UInt8
			{
				Open = 0,
				Connecting,
				Connected,
				Listening,
				Read,
				Write,
				Exception
			};

		public:
			enum class Update : UInt8
			{
				Read = 0b00000001,
				Write = 0b00000010,
				Exception = 0b00000100,
				All = 0b00000111
			};

			inline void SetOpen(const bool state) noexcept { Set(StatusType::Open, state); }
			inline void SetConnecting(const bool state) noexcept { Set(StatusType::Connecting, state); }
			inline void SetConnected(const bool state) noexcept { Set(StatusType::Connected, state); }
			inline void SetListening(const bool state) noexcept { Set(StatusType::Listening, state); }
			inline void SetRead(const bool state) noexcept { Set(StatusType::Read, state); }
			inline void SetWrite(const bool state) noexcept { Set(StatusType::Write, state); }
			inline void SetException(const bool state) noexcept { Set(StatusType::Exception, state); }
			inline void SetErrorCode(const Int errorcode) noexcept { ErrorCode = errorcode; }

			inline bool IsOpen() const noexcept { return IsSet(StatusType::Open); }
			inline bool IsConnecting() const noexcept { return IsSet(StatusType::Connecting); }
			inline bool IsConnected() const noexcept { return IsSet(StatusType::Connected); }
			inline bool IsListening() const noexcept { return IsSet(StatusType::Listening); }
			inline bool CanRead() const noexcept { return IsSet(StatusType::Read); }
			inline bool CanWrite() const noexcept { return IsSet(StatusType::Write); }
			inline bool HasException() const noexcept { return IsSet(StatusType::Exception); }
			inline Int GetErrorCode() const noexcept { return ErrorCode; }

			inline void Reset() noexcept
			{
				Status.reset();
				ErrorCode = -1;
			}

		private:
			ForceInline void Set(const StatusType status, const bool state) noexcept
			{
				Status.set(static_cast<Size>(status), state);
			}

			ForceInline bool IsSet(const StatusType status) const noexcept
			{
				return (Status.test(static_cast<Size>(status)));
			}

		private:
			std::bitset<8> Status{ 0 };
			Int ErrorCode{ -1 };
		};

		using ConnectingCallback = Callback<void(void) noexcept>;
		using AcceptCallback = Callback<void(void) noexcept>;
		using ConnectCallback = Callback<bool(void) noexcept>;
		using CloseCallback = Callback<void(void) noexcept>;

		SocketBase() noexcept = default;
		SocketBase(const SocketBase&) noexcept = default;
		SocketBase(SocketBase&&) noexcept = default;
		virtual ~SocketBase() = default;
		SocketBase& operator=(const SocketBase&) noexcept = default;
		SocketBase& operator=(SocketBase&&) noexcept = default;

		virtual bool BeginConnect(const IPEndpoint& endpoint) noexcept = 0;
		virtual bool CompleteConnect() noexcept = 0;

		virtual bool Send(Buffer& buffer) noexcept = 0;
		virtual bool SendTo(const IPEndpoint& endpoint, Buffer& buffer) noexcept = 0;
		virtual bool Receive(Buffer& buffer) noexcept = 0;
		virtual bool ReceiveFrom(IPEndpoint& endpoint, Buffer& buffer) noexcept = 0;

		virtual void Close(const bool linger = false) noexcept = 0;

		virtual const IOStatus& GetIOStatus() const noexcept = 0;
		virtual bool UpdateIOStatus(const std::chrono::milliseconds& mseconds,
									const IOStatus::Update ioupdate = IOStatus::Update::All) noexcept = 0;

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

		virtual void SetConnectingCallback(ConnectingCallback&& callback) noexcept = 0;
		virtual void SetAcceptCallback(AcceptCallback&& callback) noexcept = 0;
		virtual void SetConnectCallback(ConnectCallback&& callback) noexcept = 0;
		virtual void SetCloseCallback(CloseCallback&& callback) noexcept = 0;
	};

	inline constexpr SocketBase::IOStatus::Update operator|(SocketBase::IOStatus::Update a, SocketBase::IOStatus::Update b)
	{
		return static_cast<SocketBase::IOStatus::Update>(static_cast<int>(a) | static_cast<int>(b));
	}

	inline constexpr bool operator&(SocketBase::IOStatus::Update a, SocketBase::IOStatus::Update b)
	{
		if (static_cast<int>(a) & static_cast<int>(b)) return true;
		return false;
	}
}