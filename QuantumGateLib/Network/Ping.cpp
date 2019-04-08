// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Ping.h"
#include "..\Common\Random.h"
#include "..\Common\ScopeGuard.h"

#include <Iphlpapi.h>
#include <Ipexport.h>
#include <icmpapi.h>
#include <winternl.h> //  for IO_STATUS_BLOCK

namespace QuantumGate::Implementation::Network
{
	bool Ping::Execute(const bool use_os_api) noexcept
	{
		Reset();

		switch (m_DestinationIPAddress.AddressFamily)
		{
			case BinaryIPAddress::Family::IPv4:
			case BinaryIPAddress::Family::IPv6:
				if (use_os_api) return ExecuteOS();
				else return ExecuteRAW();
			default:
				assert(false);
				break;
		}

		LogErr(L"Ping failed due to invalid IP address family");

		m_Status = Status::Failed;

		return false;
	}

	bool Ping::ExecuteOS() noexcept
	{
		String error;

		try
		{
			LogDbg(L"Pinging %s (Buffer: %u bytes, TTL: %llus, Timeout: %llums)",
				   IPAddress(m_DestinationIPAddress).GetString().c_str(), m_BufferSize, m_TTL.count(), m_Timeout.count());

			IPAddr dest_addr4{ 0 };
			sockaddr_in6 src_addr6{ 0 };
			sockaddr_in6 dest_addr6{ 0 };
			Size reply_size{ 0 };
			HANDLE icmp_handle{ INVALID_HANDLE_VALUE };

			if (m_DestinationIPAddress.AddressFamily == BinaryIPAddress::Family::IPv4)
			{
				dest_addr4 = m_DestinationIPAddress.UInt32s[0];
				reply_size = sizeof(ICMP_ECHO_REPLY) + m_BufferSize + 8 + sizeof(IO_STATUS_BLOCK);
				icmp_handle = IcmpCreateFile();
			}
			else
			{
				src_addr6.sin6_family = AF_INET6;
				dest_addr6.sin6_family = AF_INET6;

				static_assert(sizeof(BinaryIPAddress::Bytes) == sizeof(IPV6_ADDRESS_EX::sin6_addr),
							  "Should be same size");

				std::memcpy(&dest_addr6.sin6_addr, &m_DestinationIPAddress.Bytes, sizeof(in6_addr));

				reply_size = sizeof(ICMPV6_ECHO_REPLY) + m_BufferSize + 8 + sizeof(IO_STATUS_BLOCK);
				icmp_handle = Icmp6CreateFile();
			}

			if (icmp_handle != INVALID_HANDLE_VALUE)
			{
				// Cleanup when we leave
				auto sg = MakeScopeGuard([&]() noexcept { IcmpCloseHandle(icmp_handle); });

				IP_OPTION_INFORMATION ip_opts{ 0 };
				ip_opts.Ttl = static_cast<UCHAR>(m_TTL.count());

				auto icmp_data = Random::GetPseudoRandomBytes(m_BufferSize);
				Buffer reply_buffer(reply_size);

				const auto num_replies = std::invoke([&]()
				{
					if (m_DestinationIPAddress.AddressFamily == BinaryIPAddress::Family::IPv4)
					{
						return IcmpSendEcho2(
							icmp_handle, nullptr, nullptr, nullptr, dest_addr4,
							icmp_data.GetBytes(), static_cast<WORD>(icmp_data.GetSize()), &ip_opts,
							reply_buffer.GetBytes(), static_cast<DWORD>(reply_buffer.GetSize()),
							static_cast<DWORD>(m_Timeout.count()));
					}
					else
					{
						return Icmp6SendEcho2(
							icmp_handle, nullptr, nullptr, nullptr, &src_addr6, &dest_addr6,
							icmp_data.GetBytes(), static_cast<WORD>(icmp_data.GetSize()), &ip_opts,
							reply_buffer.GetBytes(), static_cast<DWORD>(reply_buffer.GetSize()),
							static_cast<DWORD>(m_Timeout.count()));
					}
				});

				if (num_replies != 0)
				{
					ULONG status{ 0 };
					BinaryIPAddress rip;
					ULONG rtt{ 0 };
					std::optional<ULONG> ttl;

					if (m_DestinationIPAddress.AddressFamily == BinaryIPAddress::Family::IPv4)
					{
						const auto echo_reply = reinterpret_cast<ICMP_ECHO_REPLY*>(reply_buffer.GetBytes());
						status = echo_reply->Status;
						rtt = echo_reply->RoundTripTime;
						ttl = echo_reply->Options.Ttl;
						rip.AddressFamily = BinaryIPAddress::Family::IPv4;
						rip.UInt32s[0] = echo_reply->Address;
					}
					else
					{
						if (Icmp6ParseReplies(reply_buffer.GetBytes(), static_cast<DWORD>(reply_buffer.GetSize())) == 1)
						{
							const auto echo_reply = reinterpret_cast<ICMPV6_ECHO_REPLY*>(reply_buffer.GetBytes());
							status = echo_reply->Status;
							rtt = echo_reply->RoundTripTime;
							rip.AddressFamily = BinaryIPAddress::Family::IPv6;

							static_assert(sizeof(BinaryIPAddress::Bytes) == sizeof(IPV6_ADDRESS_EX::sin6_addr),
										  "Should be same size");

							std::memcpy(&rip.Bytes, &dest_addr6.sin6_addr, sizeof(IPV6_ADDRESS_EX::sin6_addr));
						}
						else error = L"failed to parse ICMP6 reply";
					}

					switch (status)
					{
						case IP_SUCCESS:
							m_Status = Status::Succeeded;
							break;
						case IP_TTL_EXPIRED_TRANSIT:
							m_Status = Status::TimeToLiveExceeded;
							break;
						case IP_DEST_NET_UNREACHABLE:
						case IP_DEST_HOST_UNREACHABLE:
						case IP_BAD_DESTINATION:
							m_Status = Status::DestinationUnreachable;
							break;
						case IP_REQ_TIMED_OUT:
							m_Status = Status::Timedout;
						default:
							break;
					}

					switch (m_Status)
					{
						case Status::Succeeded:
						case Status::DestinationUnreachable:
						case Status::TimeToLiveExceeded:
							if (ttl.has_value()) m_ResponseTTL = std::chrono::seconds(*ttl);
							m_RespondingIPAddress = rip;
							m_RoundTripTime = std::chrono::milliseconds(rtt);
							return true;
						case Status::Timedout:
							return true;
						default:
							break;
					}
				}
				else
				{
					if (GetLastError() == IP_REQ_TIMED_OUT)
					{
						m_Status = Status::Timedout;
						return true;
					}

					error = Util::FormatString(L"failed to send ICMP echo request (%s)", GetLastSysErrorString().c_str());
				}
			}
			else error = Util::FormatString(L"failed to create ICMP file handle (%s)", GetLastSysErrorString().c_str());
		}
		catch (...)
		{
			error = L"an exception was thrown";
		}

		LogErr(L"Pinging IP address %s failed - %s",
			   IPAddress(m_DestinationIPAddress).GetString().c_str(), error.c_str());

		m_Status = Status::Failed;

		return false;
	}

	bool Ping::ExecuteRAW() noexcept
	{
		// Currently only supports IPv4. On Windows this does not fully work for
		// because the OS does not notify the socket of Time Exceeded ICMP
		// messages.
		assert(m_DestinationIPAddress.AddressFamily == BinaryIPAddress::Family::IPv4);

		String error;

		try
		{
			LogDbg(L"Pinging %s (Buffer: %u bytes, TTL: %llus, Timeout: %llums)",
				   IPAddress(m_DestinationIPAddress).GetString().c_str(), m_BufferSize, m_TTL.count(), m_Timeout.count());

			ICMP::EchoMessage icmp_hdr;
			icmp_hdr.Header.Type = static_cast<UInt8>(ICMP::MessageType::Echo);
			icmp_hdr.Header.Code = 0;
			icmp_hdr.Header.Checksum = 0;

			const UInt64 num = Random::GetPseudoRandomNumber();
			icmp_hdr.Identifier = static_cast<UInt16>(num);
			icmp_hdr.SequenceNumber = static_cast<UInt16>(num >> 32);

			const auto icmp_data = Random::GetPseudoRandomBytes(m_BufferSize);

			Buffer icmp_msg(reinterpret_cast<Byte*>(&icmp_hdr), sizeof(icmp_hdr));
			icmp_msg += icmp_data;

			reinterpret_cast<ICMP::EchoMessage*>(icmp_msg.GetBytes())->Header.Checksum = ICMP::CalculateChecksum(icmp_msg);

			Socket socket(IP::AddressFamily::IPv4, Socket::Type::RAW, IP::Protocol::ICMP);

			if (!socket.SetIPTimeToLive(m_TTL)) return false;

			if (socket.SendTo(IPEndpoint(m_DestinationIPAddress, 0), icmp_msg) && icmp_msg.IsEmpty())
			{
				const auto snd_steady_time = Util::GetCurrentSteadyTime();

				if (socket.UpdateIOStatus(m_Timeout, Socket::IOStatus::Update::Read | Socket::IOStatus::Update::Exception))
				{
					const auto rcv_steady_time = Util::GetCurrentSteadyTime();

					if (socket.GetIOStatus().CanRead())
					{
						IPEndpoint endpoint;
						Buffer data;

						if (socket.ReceiveFrom(endpoint, data))
						{
							if (data.GetSize() >= sizeof(IP::Header))
							{
								const auto ip_hdr = reinterpret_cast<const IP::Header*>(data.GetBytes());

								if (ip_hdr->Protocol == static_cast<UInt8>(IP::Protocol::ICMP))
								{
									const auto ttl = ip_hdr->TTL;

									const auto msg_type = ProcessICMPReply(data, icmp_hdr.Identifier, icmp_hdr.SequenceNumber, icmp_data);
									if (msg_type.has_value())
									{
										switch (*msg_type)
										{
											case ICMP::MessageType::EchoReply:
												m_Status = Status::Succeeded;
												break;
											case ICMP::MessageType::DestinationUnreachable:
												m_Status = Status::DestinationUnreachable;
												break;
											case ICMP::MessageType::TimeExceeded:
												m_Status = Status::TimeToLiveExceeded;
												break;
											default:
												assert(false);
												break;
										}

										if (m_Status != Status::Unknown)
										{
											m_ResponseTTL = std::chrono::seconds(ttl);
											m_RespondingIPAddress = endpoint.GetIPAddress().GetBinary();
											m_RoundTripTime = std::chrono::duration_cast<std::chrono::milliseconds>(rcv_steady_time -
																													snd_steady_time);
											return true;
										}
									}
								}

								error = L"received unrecognized ICMP reply";
							}
							else error = L"not enough data received for IP header";
						}
						else error = L"failed to receive from socket";
					}
					else if (socket.GetIOStatus().HasException())
					{
						error = L"exception on socket (" +
							GetSysErrorString(socket.GetIOStatus().GetErrorCode()) + L")";
					}
					else
					{
						m_Status = Status::Timedout;
						return true;
					}
				}
				else error = L"failed to update socket state";
			}
			else error = L"failed to send ICMP packet";
		}
		catch (...)
		{
			error = L"an exception was thrown";
		}

		LogErr(L"Pinging IP address %s failed - %s",
			   IPAddress(m_DestinationIPAddress).GetString().c_str(), error.c_str());

		m_Status = Status::Failed;

		return false;
	}

	std::optional<ICMP::MessageType> Ping::ProcessICMPReply(BufferView buffer, const UInt16 expected_id,
															const UInt16 expected_sequence_number,
															const BufferView expected_data) const noexcept
	{
		if (buffer.GetSize() >= sizeof(IP::Header) + sizeof(ICMP::Header))
		{
			// Remove IP header
			buffer.RemoveFirst(sizeof(IP::Header));

			auto icmp_hdr = *reinterpret_cast<const ICMP::Header*>(buffer.GetBytes());

			switch (static_cast<ICMP::MessageType>(icmp_hdr.Type))
			{
				case ICMP::MessageType::DestinationUnreachable:
					if (buffer.GetSize() >=
						(sizeof(ICMP::DestinationUnreachableMessage) + sizeof(IP::Header) + sizeof(ICMP::EchoMessage)))
					{
						if (VerifyICMPMessageChecksum(buffer))
						{
							auto du_msg = *reinterpret_cast<const ICMP::DestinationUnreachableMessage*>(buffer.GetBytes());

							if (du_msg.Unused == 0)
							{
								buffer.RemoveFirst(sizeof(ICMP::DestinationUnreachableMessage));
								buffer.RemoveFirst(sizeof(IP::Header));

								if (VerifyICMPEchoMessage(buffer, expected_id, expected_sequence_number, expected_data))
								{
									return ICMP::MessageType::DestinationUnreachable;
								}
								else LogErr(L"Received ICMP destination unreachable message with invalid original echo message data");
							}
						}
					}
					else LogErr(L"Received ICMP destination unreachable message with unexpected size of %llu bytes", buffer.GetSize());
					break;
				case ICMP::MessageType::TimeExceeded:
					if (buffer.GetSize() >=
						(sizeof(ICMP::TimeExceededMessage) + sizeof(IP::Header) + sizeof(ICMP::EchoMessage)))
					{
						if (VerifyICMPMessageChecksum(buffer))
						{
							auto te_msg = *reinterpret_cast<const ICMP::TimeExceededMessage*>(buffer.GetBytes());

							if (te_msg.Unused == 0)
							{
								buffer.RemoveFirst(sizeof(ICMP::TimeExceededMessage));
								buffer.RemoveFirst(sizeof(IP::Header));

								if (VerifyICMPEchoMessage(buffer, expected_id, expected_sequence_number, expected_data))
								{
									return ICMP::MessageType::TimeExceeded;
								}
								else LogErr(L"Received ICMP time exceeded message with invalid original echo message data");
							}
						}
					}
					else LogErr(L"Received ICMP time exceeded message with unexpected size of %llu bytes", buffer.GetSize());
					break;
				case ICMP::MessageType::EchoReply:
					if (buffer.GetSize() >= sizeof(ICMP::EchoMessage))
					{
						if (VerifyICMPMessageChecksum(buffer))
						{
							auto echo_msg = *reinterpret_cast<const ICMP::EchoMessage*>(buffer.GetBytes());

							if (echo_msg.Header.Code == 0 &&
								echo_msg.Identifier == expected_id &&
								echo_msg.SequenceNumber == expected_sequence_number)
							{
								buffer.RemoveFirst(sizeof(ICMP::EchoMessage));

								if (buffer == expected_data)
								{
									return ICMP::MessageType::EchoReply;
								}
							}
							else LogErr(L"Received ICMP echo reply message with unexpected code, ID or sequence number");
						}
					}
					else LogErr(L"Received ICMP echo reply message with unexpected size of %llu bytes", buffer.GetSize());
					break;
				default:
					LogErr(L"Received unrecognized ICMP message type %u", icmp_hdr.Type);
					break;
			}
		}

		return std::nullopt;
	}

	bool Ping::VerifyICMPEchoMessage(BufferView buffer, const UInt16 expected_id,
									 const UInt16 expected_sequence_number,
									 const BufferView expected_data) const noexcept
	{
		if (buffer.GetSize() >= sizeof(ICMP::EchoMessage))
		{
			auto echo_msg = *reinterpret_cast<const ICMP::EchoMessage*>(buffer.GetBytes());

			if (echo_msg.Identifier == expected_id &&
				echo_msg.SequenceNumber == expected_sequence_number)
			{
				buffer.RemoveFirst(sizeof(ICMP::EchoMessage));

				auto cmp_size = expected_data.GetSize();

				// We'll compare at most the first 64 bits
				// which are required according to RFC 792
				if (cmp_size > 8) cmp_size = 8;

				if (buffer.GetSize() >= cmp_size)
				{
					return (buffer.GetFirst(cmp_size) == expected_data.GetFirst(cmp_size));
				}
			}
		}

		return false;
	}

	bool Ping::VerifyICMPMessageChecksum(BufferView buffer) const noexcept
	{
		try
		{
			// We should require at least the basic ICMP header
			if (buffer.GetSize() >= sizeof(ICMP::Header))
			{
				Buffer chksum_buf{ buffer };

				// Reset checksum to zeros before calculating
				reinterpret_cast<ICMP::Header*>(chksum_buf.GetBytes())->Checksum = 0;

				if (reinterpret_cast<const ICMP::Header*>(buffer.GetBytes())->Checksum ==
					ICMP::CalculateChecksum(chksum_buf)) return true;

				LogErr(L"ICMP message checksum failed verification");
			}
		}
		catch (...)
		{
			LogErr(L"Failed to verify ICMP message checksum due to exception");
		}

		return false;
	}

	String Ping::GetString() const noexcept
	{
		try
		{
			String str = Util::FormatString(L"Destination [IP: %s, Timeout: %llums, Buffer size: %u bytes, TTL: %llus]",
											IPAddress(m_DestinationIPAddress).GetString().c_str(), m_Timeout.count(),
											m_BufferSize, m_TTL.count());
			String rttl_str;
			if (m_ResponseTTL.has_value())
			{
				rttl_str += Util::FormatString(L", Response TTL: %llus", m_ResponseTTL->count());
			}

			switch (m_Status)
			{
				case Status::Succeeded:
					str += Util::FormatString(L" / Result [Succeeded, Responding IP: %s, Response time: %llums%s]",
											  IPAddress(*m_RespondingIPAddress).GetString().c_str(), m_RoundTripTime->count(),
											  rttl_str.c_str());
					break;
				case Status::TimeToLiveExceeded:
					str += Util::FormatString(L" / Result [TTL Exceeded, Responding IP: %s, Response time: %llums%s]",
											  IPAddress(*m_RespondingIPAddress).GetString().c_str(), m_RoundTripTime->count(),
											  rttl_str.c_str());
					break;
				case Status::DestinationUnreachable:
					str += Util::FormatString(L" / Result [Destination unreachable, Responding IP: %s, Response time: %llums%s]",
											  IPAddress(*m_RespondingIPAddress).GetString().c_str(), m_RoundTripTime->count(),
											  rttl_str.c_str());
					break;
				case Status::Timedout:
					str += L" / Result [Timed out]";
					break;
				case Status::Failed:
					str += L" / Result [Failed]";
					break;
				default:
					str += L" / Result [None]";
					break;
			}

			return str;
		}
		catch (...) {}

		return {};
	}

	std::ostream& operator<<(std::ostream& stream, const Ping& ping)
	{
		stream << Util::ToStringA(ping.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const Ping& ping)
	{
		stream << ping.GetString();
		return stream;
	}
}