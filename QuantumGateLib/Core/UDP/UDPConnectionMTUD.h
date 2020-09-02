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
		enum class Status { Initialized, Discovery, Finished };

		MTUDiscovery(Network::Socket& socket, IPEndpoint endpoint) noexcept :
			m_Socket(socket), m_Endpoint(endpoint)
		{}

		MTUDiscovery(const Connection&) = delete;
		MTUDiscovery(Connection&& other) noexcept = delete;
		~MTUDiscovery() = default;
		MTUDiscovery& operator=(const MTUDiscovery&) = delete;
		MTUDiscovery& operator=(MTUDiscovery&&) noexcept = delete;

		[[nodiscard]] inline Status GetStatus() const noexcept { return m_Status; }
		[[nodiscard]] inline Size GetMaxMessageSize() const noexcept { return m_MaxAckedMessageSize; }

		void SendNewMessage() noexcept
		{
			try
			{
				Message msg(Message::Type::MTUD, Message::Direction::Outgoing, m_CurrentMessageSize);
				msg.SetMessageSequenceNumber(static_cast<Message::SequenceNumber>(Util::GetPseudoRandomNumber()));
				msg.SetMessageAckNumber(0);
				msg.SetMessageData(Util::GetPseudoRandomBytes(msg.GetMaxMessageDataSize()));

				Buffer data;
				if (msg.Write(data))
				{
					m_MTUDMessageData.emplace();
					m_MTUDMessageData->SequenceNumber = msg.GetMessageSequenceNumber();
					m_MTUDMessageData->NumTries = 0;
					m_MTUDMessageData->TimeSent = Util::GetCurrentSteadyTime();
					m_MTUDMessageData->Data = std::move(data);
					m_MTUDMessageData->Acked = false;

					LogWarn(L"Sending MTUD message of size %zu bytes", m_CurrentMessageSize);

					const auto result = m_Socket.SendTo(m_Endpoint, m_MTUDMessageData->Data);
					if (result.Succeeded()) m_MTUDMessageData->NumTries = 1;
				}
			}
			catch (...) {}
		}

		void Process() noexcept
		{
			switch (m_Status)
			{
				case Status::Initialized:
				{
					SendNewMessage();
					m_Status = Status::Discovery;
					break;
				}
				case Status::Discovery:
				{
					if (!m_MTUDMessageData->Acked &&
						(Util::GetCurrentSteadyTime() - m_MTUDMessageData->TimeSent >= m_RetransmissionTimeout))
					{
						if (m_MTUDMessageData->NumTries >= MaxNumRetries)
						{
							m_Status = Status::Finished;
						}
						else
						{
							const auto result = m_Socket.SendTo(m_Endpoint, m_MTUDMessageData->Data);
							if (result.Succeeded())
							{
								// If data was actually sent, otherwise buffer may
								// temporarily be full/unavailable
								if (*result == m_MTUDMessageData->Data.GetSize())
								{
									// We'll wait for ack or else continue sending
									m_MTUDMessageData->TimeSent = Util::GetCurrentSteadyTime();
									++m_MTUDMessageData->NumTries;
								}
								else return;
							}
							else
							{
								LogErr(L"UDP connection MTUD: send failed for peer %s (%s)",
									   m_Endpoint.GetString().c_str(), result.GetErrorString().c_str());
							}
						}
					}
					else if (m_MTUDMessageData->Acked)
					{
						SendNewMessage();
					}
					break;
				}
				case Status::Finished:
				{
					break;
				}
				default:
				{
					assert(false);
					break;
				}
			}
		}

		void AckSentMessage(const Message::SequenceNumber seqnum) noexcept
		{
			try
			{
				Message msg(Message::Type::MTUDAck, Message::Direction::Outgoing, 512);
				msg.SetMessageSequenceNumber(static_cast<Message::SequenceNumber>(Util::GetPseudoRandomNumber()));
				msg.SetMessageAckNumber(seqnum);

				Buffer data;
				if (msg.Write(data))
				{
					LogWarn(L"Sending MTUDAck message");

					const auto result = m_Socket.SendTo(m_Endpoint, data);
					if (result.Failed())
					{
						LogErr(L"Failed to send MTUDAck message to %s", m_Endpoint.GetString().c_str());
					}
				}
			}
			catch (...) {}
		}

		void ProcessReceivedAck(const Message::SequenceNumber seqnum) noexcept
		{
			if (m_MTUDMessageData->SequenceNumber == seqnum)
			{
				m_RetransmissionTimeout = std::max(MinRetransmissionTimeout,
												   std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() - m_MTUDMessageData->TimeSent));

				m_MaxAckedMessageSize = m_MTUDMessageData->Data.GetSize();

				if (m_MaxAckedMessageSize == m_Socket.GetMaxDatagramMessageSize())
				{
					m_Status = Status::Finished;
				}
				else
				{
					m_CurrentMessageSize = std::min(static_cast<Size>(m_Socket.GetMaxDatagramMessageSize()), m_CurrentMessageSize * 2);
				}

				m_MTUDMessageData->Acked = true;
			}
		}

	private:
		static constexpr std::chrono::milliseconds MinRetransmissionTimeout{ 600 };
		static constexpr Size MaxNumRetries{ 8 };

	private:
		Status m_Status{ Status::Initialized };
		Network::Socket& m_Socket;
		IPEndpoint m_Endpoint;
		std::optional<MTUDMessageData> m_MTUDMessageData;
		Size m_MaxAckedMessageSize{ 512 };
		Size m_CurrentMessageSize{ 512 };
		std::chrono::milliseconds m_RetransmissionTimeout{ MinRetransmissionTimeout };
	};
}
