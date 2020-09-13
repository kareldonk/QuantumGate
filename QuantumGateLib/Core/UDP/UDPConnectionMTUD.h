// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPMessage.h"
#include "..\..\Common\Random.h"

// Use to enable/disable MTU discovery debug console output
// #define UDPMTUD_DEBUG

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class MTUDiscovery final
	{
		struct MTUDMessageData final
		{
			Message::SequenceNumber SequenceNumber{ 0 };
			UInt NumTries{ 0 };
			SteadyTime TimeSent;
			Buffer Data;
			bool Acked{ false };
		};

	public:
		enum class Status { Start, Discovery, Finished, Failed };

		MTUDiscovery() noexcept {}
		MTUDiscovery(const MTUDiscovery&) = delete;
		MTUDiscovery(MTUDiscovery&&) noexcept = delete;
		~MTUDiscovery() = default;
		MTUDiscovery& operator=(const MTUDiscovery&) = delete;
		MTUDiscovery& operator=(MTUDiscovery&&) noexcept = delete;

		[[nodiscard]] inline Size GetMaxMessageSize() const noexcept { return m_MaximumMessageSize; }

		[[nodiscard]] bool CreateNewMessage(const Size msg_size, const IPEndpoint& endpoint) noexcept
		{
			try
			{
				Message msg(Message::Type::MTUD, Message::Direction::Outgoing, msg_size);
				msg.SetMessageSequenceNumber(static_cast<Message::SequenceNumber>(Random::GetPseudoRandomNumber()));
				msg.SetMessageData(Random::GetPseudoRandomBytes(msg.GetMaxMessageDataSize()));

				Buffer data;
				if (msg.Write(data))
				{
					m_MTUDMessageData.emplace(
						MTUDMessageData{
							.SequenceNumber = msg.GetMessageSequenceNumber(),
							.NumTries = 0,
							.Data = std::move(data),
							.Acked = false
						});

					return true;
				}
			}
			catch (const std::exception& e)
			{
				LogErr(L"UDP connection MTUD: failed to create MTUD message of size %zu bytes for peer %s due to exception: %s",
					   msg_size, endpoint.GetString().c_str(), Util::ToStringW(e.what()).c_str());
			}
			catch (...)
			{
				LogErr(L"UDP connection MTUD: failed to create MTUD message of size %zu bytes for peer %s due to unknown exception",
					   msg_size, endpoint.GetString().c_str());
			}

			return false;
		}

		[[nodiscard]] bool TransmitMessage(Network::Socket& socket, const IPEndpoint& endpoint) noexcept
		{
			// Message must have already been created
			assert(m_MTUDMessageData.has_value());

#ifdef UDPMTUD_DEBUG
			SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: sending MTUD message of size " <<
					 m_MTUDMessageData->Data.GetSize() << L" bytes to peer " << endpoint.GetString() <<
					 L" (" << m_MTUDMessageData->NumTries << L" previous tries)" << SLogFmt(Default));
#endif
			const auto result = socket.SendTo(endpoint, m_MTUDMessageData->Data);
			if (result.Succeeded())
			{
				// If data was actually sent, otherwise buffer may
				// temporarily be full/unavailable
				if (*result == m_MTUDMessageData->Data.GetSize())
				{
					// We'll wait for ack or else continue trying
					m_MTUDMessageData->TimeSent = Util::GetCurrentSteadyTime();
					++m_MTUDMessageData->NumTries;
				}

				return true;
			}
			else
			{
				if (result.GetErrorCode().category() == std::system_category() &&
					result.GetErrorCode().value() == 10040)
				{
					// 10040 is 'message too large' error;
					// we are expecting that at some point
#ifdef UDPMTUD_DEBUG
					SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD : failed to send MTUD message of size " <<
							 m_MTUDMessageData->Data.GetSize() << L" bytes to peer " << endpoint.GetString() <<
							 L" (" << result.GetErrorString() << L")" << SLogFmt(Default));
#endif
				}
				else
				{
					LogErr(L"UDP connection MTUD: failed to send MTUD message of size %zu bytes to peer %s (%s)",
						   m_MTUDMessageData->Data.GetSize(), endpoint.GetString().c_str(), result.GetErrorString().c_str());
				}
			}

			return false;
		}

		[[nodiscard]] Status Process(Network::Socket& socket, const IPEndpoint& endpoint) noexcept
		{
			switch (m_Status)
			{
				case Status::Start:
				{
					// Begin with first/smallest message size
					m_MaximumMessageSize = MinMessageSize;
					m_CurrentMessageSizeIndex = 0;

					// Set MTU discovery option on socket which disables fragmentation
					// so that packets that are larger than the path MTU will get dropped
					if (socket.SetMTUDiscovery(true))
					{
#ifdef UDPMTUD_DEBUG
						int maxdg_size{ 0 };

						const auto result = socket.GetMaxDatagramMessageSize();
						if (result.Succeeded()) maxdg_size = *result;
						else
						{
							LogErr(L"UDP connection MTUD: failed to get maximum datagram mesage size of socket for peer %s (%s)",
								   endpoint.GetString().c_str(), result.GetErrorString().c_str());
						}

						SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: starting MTU discovery for peer " <<
								 endpoint.GetString() << L"; maximum datagram message size is " << maxdg_size <<
								 L" bytes" << SLogFmt(Default));
#endif
						if (CreateNewMessage(MessageSizes[m_CurrentMessageSizeIndex], endpoint) &&
							TransmitMessage(socket, endpoint))
						{
							m_Status = Status::Discovery;
						}
						else m_Status = Status::Failed;
					}
					else
					{
						LogErr(L"UDP connection MTUD: failed to enable MTU discovery option on socket");
						m_Status = Status::Failed;
					}
					break;
				}
				case Status::Discovery:
				{
					if (!m_MTUDMessageData->Acked &&
						(Util::GetCurrentSteadyTime() - m_MTUDMessageData->TimeSent >= m_RetransmissionTimeout))
					{
						if (m_MTUDMessageData->NumTries >= MaxNumRetries)
						{
							// Stop retrying
							m_Status = Status::Finished;
						}
						else
						{
							// Retry transmission and see if we get an ack
							if (!TransmitMessage(socket, endpoint))
							{
								m_Status = Status::Failed;
							}
						}
					}
					else if (m_MTUDMessageData->Acked)
					{
						if (m_CurrentMessageSizeIndex == (MessageSizes.size() - 1))
						{
							// Reached maximum possible message size
							m_Status = Status::Finished;
						}
						else
						{
							// Create and send bigger message
							++m_CurrentMessageSizeIndex;
							if (!CreateNewMessage(MessageSizes[m_CurrentMessageSizeIndex], endpoint) ||
								!TransmitMessage(socket, endpoint))
							{
								m_Status = Status::Failed;
							}
						}
					}
					break;
				}
				case Status::Finished:
				case Status::Failed:
				{
					break;
				}
				default:
				{
					// Shouldn't get here
					assert(false);
					m_Status = Status::Failed;
					break;
				}
			}

			switch (m_Status)
			{
				case Status::Finished:
				case Status::Failed:
				{
					if (m_Status == Status::Failed)
					{
						LogErr(L"UDP connection MTUD: failed MTU discovery; maximum message size is %zu bytes for peer %s",
							   endpoint.GetString().c_str(), GetMaxMessageSize());
					}
					else
					{
#ifdef UDPMTUD_DEBUG
						SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: finished MTU discovery; maximum message size is " <<
								 GetMaxMessageSize() << L" bytes for peer " << endpoint.GetString() << SLogFmt(Default));
#endif
					}

					// Disable MTU discovery on socket now that we're done
					if (!socket.SetMTUDiscovery(false))
					{
						LogErr(L"UDP connection MTUD: failed to disable MTU discovery option on socket");
					}

					break;
				}
				default:
				{
					break;
				}
			}

			return m_Status;
		}

		void ProcessReceivedAck(const Message::SequenceNumber seqnum) noexcept
		{
			if (m_Status == Status::Discovery && m_MTUDMessageData->SequenceNumber == seqnum)
			{
				m_RetransmissionTimeout = std::max(MinRetransmissionTimeout,
												   std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() - m_MTUDMessageData->TimeSent));
				m_MTUDMessageData->Acked = true;
				m_MaximumMessageSize = m_MTUDMessageData->Data.GetSize();
			}
		}

		static void AckReceivedMessage(Network::Socket& socket, const IPEndpoint& endpoint,
									   const Message::SequenceNumber seqnum) noexcept
		{
			try
			{
				Message msg(Message::Type::MTUD, Message::Direction::Outgoing, MinMessageSize);
				msg.SetMessageAckNumber(seqnum);

				Buffer data;
				if (msg.Write(data))
				{
#ifdef UDPMTUD_DEBUG
					SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: sending MTUDAck message to peer " <<
							 endpoint.GetString() << SLogFmt(Default));
#endif
					const auto result = socket.SendTo(endpoint, data);
					if (result.Failed())
					{
						LogErr(L"UDP connection MTUD: failed to send MTUDAck message to peer %s",
							   endpoint.GetString().c_str());
					}
				}
			}
			catch (const std::exception& e)
			{
				LogErr(L"UDP connection MTUD: failed to send MTUDAck message to peer %s due to exception: %s",
					   endpoint.GetString().c_str(), Util::ToStringW(e.what()).c_str());
			}
			catch (...)
			{
				LogErr(L"UDP connection MTUD: failed to send MTUDAck message to peer %s due to unknown exception",
					   endpoint.GetString().c_str());
			}
		}

	public:
		// According to RFC 791 IPv4 requires an MTU of 576 octets or greater, while
		// the maximum size of the IP header is 60.
		// According to RFC 8200 IPv6 requires an MTU of 1280 octets or greater, while
		// the minimum IPv6 header size (fixed header) is 40 octets. Recommended configuration
		// is for 1500 octets or greater.
		// Maximum message size is 65467 octets (65535 - 8 octet UDP header - 60 octet IP header).
		static constexpr std::array<Size, 9> MessageSizes{ 508, 1232, 1452, 2048, 4096, 8192, 16384, 32768, 65467 };
		static constexpr Size MinMessageSize{ 508 };
		static constexpr Size MaxMessageSize{  65467 };

	private:

		static constexpr std::chrono::milliseconds MinRetransmissionTimeout{ 600 };
		static constexpr Size MaxNumRetries{ 6 };

	private:
		Status m_Status{ Status::Start };
		std::optional<MTUDMessageData> m_MTUDMessageData;
		Size m_MaximumMessageSize{ MinMessageSize };
		Size m_CurrentMessageSizeIndex{ 0 };
		std::chrono::milliseconds m_RetransmissionTimeout{ MinRetransmissionTimeout };
	};
}
