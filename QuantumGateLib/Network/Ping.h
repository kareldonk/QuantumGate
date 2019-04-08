// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IP.h"
#include "Socket.h"

namespace QuantumGate::Implementation::Network
{
	using namespace std::literals;

	class Export Ping
	{
	public:
		enum class Status
		{
			Unknown, Succeeded, Timedout, TimeToLiveExceeded, DestinationUnreachable, Failed
		};

		Ping() = delete;

		Ping(const BinaryIPAddress& ip, const UInt16 buf_size = 32,
			 const std::chrono::milliseconds timeout = 5000ms, const std::chrono::seconds ip_ttl = 64s) noexcept :
			m_DestinationIPAddress(ip),m_BufferSize(buf_size), m_Timeout(timeout), m_TTL(ip_ttl)
		{}

		Ping(const Ping&) = delete;
		Ping(Ping&&) = default;
		~Ping() = default;
		Ping& operator=(const Ping&) = delete;
		Ping& operator=(Ping&&) = default;

		[[nodiscard]] bool Execute(const bool use_os_api = true) noexcept;

		[[nodiscard]] inline const BinaryIPAddress& GetDestinationIPAddress() const noexcept { return m_DestinationIPAddress; }
		[[nodiscard]] inline std::chrono::milliseconds GetTimeout() const noexcept { return m_Timeout; }
		[[nodiscard]] inline UInt16 GetBufferSize() const noexcept { return m_BufferSize; }
		[[nodiscard]] inline std::chrono::seconds GetTTL() const noexcept { return m_TTL; }

		[[nodiscard]] inline Status GetStatus() const noexcept { return m_Status; }
		[[nodiscard]] inline const std::optional<std::chrono::seconds>& GetResponseTTL() const noexcept { return m_ResponseTTL; }
		[[nodiscard]] inline const std::optional<BinaryIPAddress>& GetRespondingIPAddress() const noexcept { return m_RespondingIPAddress; }
		[[nodiscard]] inline const std::optional<std::chrono::milliseconds>& GetRoundTripTime() const noexcept { return m_RoundTripTime; }

		[[nodiscard]] String GetString() const noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const Ping& ping);
		friend Export std::wostream& operator<<(std::wostream& stream, const Ping& ping);

	private:
		[[nodiscard]] bool ExecuteOS() noexcept;
		[[nodiscard]] bool ExecuteRAW() noexcept;

		inline void Reset() noexcept
		{
			m_Status = Status::Unknown;
			m_ResponseTTL.reset();
			m_RespondingIPAddress.reset();
			m_RoundTripTime.reset();
		}

		[[nodiscard]] bool VerifyICMPMessageChecksum(BufferView buffer) const noexcept;
		[[nodiscard]] bool VerifyICMPEchoMessage(BufferView buffer, const UInt16 expected_id,
												 const UInt16 expected_sequence_number,
												 const BufferView expected_data) const noexcept;

		[[nodiscard]] std::optional<ICMP::MessageType> ProcessICMPReply(BufferView buffer, const UInt16 expected_id,
																		const UInt16 expected_sequence_number,
																		const BufferView expected_data) const noexcept;

	private:
		BinaryIPAddress m_DestinationIPAddress;
		UInt16 m_BufferSize{ 0 };
		std::chrono::milliseconds m_Timeout{ 0 };
		std::chrono::seconds m_TTL{ 0 };

		Status m_Status{ Status::Unknown };
		std::optional<std::chrono::seconds> m_ResponseTTL;
		std::optional<BinaryIPAddress> m_RespondingIPAddress;
		std::optional<std::chrono::milliseconds> m_RoundTripTime;
	};
}