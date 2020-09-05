// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPMessage.h"

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

		MTUDiscovery() noexcept = default;
		MTUDiscovery(const Connection&) = delete;
		MTUDiscovery(Connection&& other) noexcept = delete;
		~MTUDiscovery() = default;
		MTUDiscovery& operator=(const MTUDiscovery&) = delete;
		MTUDiscovery& operator=(MTUDiscovery&&) noexcept = delete;

		[[nodiscard]] inline Size GetMaxMessageSize() const noexcept { return m_MaxAckedMessageSize; }

		[[nodiscard]] bool CreateNewMessage(const IPEndpoint& endpoint) noexcept
		{
			try
			{
				Message msg(Message::Type::MTUD, Message::Direction::Outgoing, m_CurrentMessageSize);
				msg.SetMessageSequenceNumber(static_cast<Message::SequenceNumber>(Util::GetPseudoRandomNumber()));
				msg.SetMessageAckNumber(static_cast<Message::SequenceNumber>(Util::GetPseudoRandomNumber()));
				msg.SetMessageData(Util::GetPseudoRandomBytes(msg.GetMaxMessageDataSize()));

				Buffer data;
				if (msg.Write(data))
				{
					m_MTUDMessageData.emplace();
					m_MTUDMessageData->SequenceNumber = msg.GetMessageSequenceNumber();
					m_MTUDMessageData->NumTries = 0;
					m_MTUDMessageData->Data = std::move(data);
					m_MTUDMessageData->Acked = false;

					return true;
				}
			}
			catch (const std::exception& e)
			{
				LogErr(L"UDP connection MTUD: failed to create MTUD message of size %zu bytes for peer %s due to exception: %s",
					   m_CurrentMessageSize, endpoint.GetString().c_str(), Util::ToStringW(e.what()).c_str());
			}
			catch (...)
			{
				LogErr(L"UDP connection MTUD: failed to create MTUD message of size %zu bytes for peer %s due to unknown exception",
					   m_CurrentMessageSize, endpoint.GetString().c_str());
			}

			return false;
		}

		[[nodiscard]] bool TransmitMessage(Network::Socket& socket, const IPEndpoint& endpoint) noexcept
		{
			// Message must have already been created
			assert(m_MTUDMessageData.has_value());

			LogWarn(L"UDP connection MTUD: sending MTUD message of size %zu bytes to peer %s (%u previous tries)",
					m_MTUDMessageData->Data.GetSize(), endpoint.GetString().c_str(), m_MTUDMessageData->NumTries);

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
				LogErr(L"UDP connection MTUD: failed to send MTUD message of size %zu bytes to peer %s (%s)",
					   m_CurrentMessageSize, endpoint.GetString().c_str(), result.GetErrorString().c_str());
			}

			return false;
		}

		[[nodiscard]] Status Process(Network::Socket& socket, const IPEndpoint& endpoint) noexcept
		{
			switch (m_Status)
			{
				case Status::Start:
				{
					if (socket.SetMTUDiscovery(true))
					{
						const auto result = socket.GetMaxDatagramMessageSize();
						if (result.Succeeded())
						{
							LogWarn(L"UDP connection MTUD: starting MTU Discovery; maximum datagram message size is %d",
									*result);
						}

						if (CreateNewMessage(endpoint) && TransmitMessage(socket, endpoint))
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
						const auto result = socket.GetMaxDatagramMessageSize();
						if (result.Succeeded())
						{
							if (m_MaxAckedMessageSize == *result)
							{
								// Reached maximum possible message size
								m_Status = Status::Finished;
							}
							else
							{
								// Create and send bigger message
								m_CurrentMessageSize = std::min(static_cast<Size>(*result), m_CurrentMessageSize * 2);
								if (!CreateNewMessage(endpoint) || !TransmitMessage(socket, endpoint))
								{
									m_Status = Status::Failed;
								}
							}
						}
						else m_Status = Status::Failed;
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
					LogWarn(L"UDP connection MTUD: finished MTU discovery");

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
				m_MaxAckedMessageSize = m_MTUDMessageData->Data.GetSize();
				m_MTUDMessageData->Acked = true;
			}
		}

		static void AckSentMessage(Network::Socket& socket, const IPEndpoint& endpoint,
								   const Message::SequenceNumber seqnum) noexcept
		{
			try
			{
				Message msg(Message::Type::MTUDAck, Message::Direction::Outgoing, 512);
				msg.SetMessageSequenceNumber(static_cast<Message::SequenceNumber>(Util::GetPseudoRandomNumber()));
				msg.SetMessageAckNumber(seqnum);

				Buffer data;
				if (msg.Write(data))
				{
					LogWarn(L"UDP connection MTUD: sending MTUDAck message to peer %s",
							endpoint.GetString().c_str());

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
		// According to RFC 8200 IPv6 requires an MTU of 1280 octets or greater.
		// The minimum IPv6 header size (fixed header) is 40 octets.
		static constexpr Size MinMessageSize{ 512 };

	private:

		static constexpr std::chrono::milliseconds MinRetransmissionTimeout{ 600 };
		static constexpr Size MaxNumRetries{ 8 };

	private:
		Status m_Status{ Status::Start };
		std::optional<MTUDMessageData> m_MTUDMessageData;
		Size m_MaxAckedMessageSize{ MinMessageSize };
		Size m_CurrentMessageSize{ MinMessageSize };
		std::chrono::milliseconds m_RetransmissionTimeout{ MinRetransmissionTimeout };
	};
}
