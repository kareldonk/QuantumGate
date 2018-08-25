// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <bitset>

namespace QuantumGate::Implementation::Network
{
	class SocketIOStatus
	{
		enum class StatusType
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
		inline void SetOpen(const bool state) noexcept { Set(StatusType::Open, state); }
		inline void SetConnecting(const bool state) noexcept { Set(StatusType::Connecting, state); }
		inline void SetConnected(const bool state) noexcept { Set(StatusType::Connected, state); }
		inline void SetListening(const bool state) noexcept { Set(StatusType::Listening, state); }
		inline void SetRead(const bool state) noexcept { Set(StatusType::Read, state); }
		inline void SetWrite(const bool state) noexcept { Set(StatusType::Write, state); }
		inline void SetException(const bool state) noexcept { Set(StatusType::Exception, state); }
		inline void SetErrorCode(const Int errorcode) noexcept { ErrorCode = errorcode; }

		inline const bool IsOpen() const noexcept { return IsSet(StatusType::Open); }
		inline const bool IsConnecting() const noexcept { return IsSet(StatusType::Connecting); }
		inline const bool IsConnected() const noexcept { return IsSet(StatusType::Connected); }
		inline const bool IsListening() const noexcept { return IsSet(StatusType::Listening); }
		inline const bool CanRead() const noexcept { return IsSet(StatusType::Read); }
		inline const bool CanWrite() const noexcept { return IsSet(StatusType::Write); }
		inline const bool HasException() const noexcept { return IsSet(StatusType::Exception); }
		inline const Int GetErrorCode() const noexcept { return ErrorCode; }

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

		ForceInline const bool IsSet(const StatusType status) const noexcept
		{
			return (Status.test(static_cast<Size>(status)));
		}

	private:
		std::bitset<8> Status{ 0 };
		Int ErrorCode{ -1 };
	};
}