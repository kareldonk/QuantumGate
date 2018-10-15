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

		Ping(const BinaryIPAddress& ip, const std::chrono::milliseconds timeout = 5000ms,
			 const UInt16 buf_size = 32, const std::chrono::seconds ip_ttl = 64s) noexcept :
			m_DestinationIPAddress(ip), m_Timeout(timeout), m_BufferSize(buf_size), m_TTL(ip_ttl)
		{}

		Ping(const Ping&) = delete;
		Ping(Ping&&) = default;
		~Ping() = default;
		Ping& operator=(const Ping&) = delete;
		Ping& operator=(Ping&&) = default;

		[[nodiscard]] const bool Execute(const bool use_os_api = true) noexcept;

		inline const BinaryIPAddress& GetDestinationIPAddress() const noexcept { return m_DestinationIPAddress; }
		inline std::chrono::milliseconds GetTimeout() const noexcept { return m_Timeout; }
		inline UInt16 GetBufferSize() const noexcept { return m_BufferSize; }
		inline std::chrono::seconds GetTTL() const noexcept { return m_TTL; }

		[[nodiscard]] inline Status GetStatus() const noexcept { return m_Status; }
		inline std::chrono::milliseconds GetRoundTripTime() const noexcept { return m_RoundTripTime; }
		inline std::optional<std::chrono::seconds> GetResponseTTL() const noexcept { return m_ResponseTTL; }
		inline const BinaryIPAddress& GetRespondingIPAddress() const noexcept { return m_RespondingIPAddress; }

		String GetString() const noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const Ping& ping);
		friend Export std::wostream& operator<<(std::wostream& stream, const Ping& ping);

	private:
		[[nodiscard]] const bool ExecuteOS() noexcept;
		[[nodiscard]] const bool ExecuteRAW() noexcept;

		inline void Reset() noexcept
		{
			m_Status = Status::Unknown;
			m_ResponseTTL.reset();
			m_RoundTripTime = 0ms;
			m_RespondingIPAddress.Clear();
		}

		[[nodiscard]] const bool VerifyICMPMessageChecksum(BufferView buffer) const noexcept;
		[[nodiscard]] const bool VerifyICMPEchoMessage(BufferView buffer, const UInt16 expected_id,
													   const UInt16 expected_sequence_number,
													   const BufferView expected_data) const noexcept;

		[[nodiscard]] std::optional<ICMP::MessageType> ProcessICMPReply(BufferView buffer, const UInt16 expected_id,
																		const UInt16 expected_sequence_number,
																		const BufferView expected_data) const noexcept;

	private:
		BinaryIPAddress m_DestinationIPAddress;
		std::chrono::milliseconds m_Timeout{ 0 };
		UInt16 m_BufferSize{ 0 };
		std::chrono::seconds m_TTL{ 0 };

		Status m_Status{ Status::Unknown };
		std::optional<std::chrono::seconds> m_ResponseTTL;
		BinaryIPAddress m_RespondingIPAddress;
		std::chrono::milliseconds m_RoundTripTime{ 0 };
	};
}