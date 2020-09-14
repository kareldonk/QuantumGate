// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnectionMTUD.h"
#include "UDPConnection.h"

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	MTUDiscovery::MTUDiscovery(Connection& connection) noexcept : m_Connection(connection)
	{}

	bool MTUDiscovery::CreateNewMessage(const Size msg_size) noexcept
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
			LogErr(L"UDP connection MTUD: failed to create MTUD message of size %zu bytes on connection %llu due to exception: %s",
				   msg_size, m_Connection.GetID(), Util::ToStringW(e.what()).c_str());
		}
		catch (...)
		{
			LogErr(L"UDP connection MTUD: failed to create MTUD message of size %zu bytes on connection %llu due to unknown exception",
				   msg_size, m_Connection.GetID());
		}

		return false;
	}

	MTUDiscovery::TransmitResult MTUDiscovery::TransmitMessage() noexcept
	{
		// Message must have already been created
		assert(m_MTUDMessageData.has_value());

#ifdef UDPMTUD_DEBUG
		SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: sending MTUD message of size " <<
				 m_MTUDMessageData->Data.GetSize() << L" bytes on connection " << m_Connection.GetID() <<
				 L" (" << m_MTUDMessageData->NumTries << L" previous tries)" << SLogFmt(Default));
#endif
		const auto now = Util::GetCurrentSteadyTime();

		const auto result = m_Connection.Send(now, m_MTUDMessageData->Data, false);
		if (result.Succeeded())
		{
			// If data was actually sent, otherwise buffer may
			// temporarily be full/unavailable
			if (*result == m_MTUDMessageData->Data.GetSize())
			{
				// We'll wait for ack or else continue trying
				m_MTUDMessageData->TimeSent = now;
				++m_MTUDMessageData->NumTries;
			}

			return TransmitResult::Success;
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
						 m_MTUDMessageData->Data.GetSize() << L" bytes on connection " << m_Connection.GetID() <<
						 L" (" << result.GetErrorString() << L")" << SLogFmt(Default));
#endif
				return TransmitResult::MessageTooLarge;
			}
			else
			{
				LogErr(L"UDP connection MTUD: failed to send MTUD message of size %zu bytes on connection %llu (%s)",
					   m_MTUDMessageData->Data.GetSize(), m_Connection.GetID(), result.GetErrorString().c_str());
			}
		}

		return TransmitResult::Failed;
	}

	void MTUDiscovery::ProcessTransmitResult(const TransmitResult result) noexcept
	{
		switch (result)
		{
			case TransmitResult::Success:
				m_Status = Status::Discovery;
				break;
			case TransmitResult::MessageTooLarge:
				m_Status = Status::Finished;
				break;
			case TransmitResult::Failed:
				m_Status = Status::Failed;
				break;
			default:
				assert(false);
				break;
		}
	}

	MTUDiscovery::Status MTUDiscovery::Process() noexcept
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
				if (m_Connection.SetMTUDiscovery(true))
				{
#ifdef UDPMTUD_DEBUG
					SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: starting MTU discovery on connection " <<
							 m_Connection.GetID() << SLogFmt(Default));
#endif
					if (CreateNewMessage(MessageSizes[m_CurrentMessageSizeIndex]))
					{
						ProcessTransmitResult(TransmitMessage());
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
						ProcessTransmitResult(TransmitMessage());
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
						if (CreateNewMessage(MessageSizes[m_CurrentMessageSizeIndex]))
						{
							ProcessTransmitResult(TransmitMessage());
						}
						else m_Status = Status::Failed;
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
					LogErr(L"UDP connection MTUD: failed MTU discovery; maximum message size is %zu bytes for connection %llu",
						   GetMaxMessageSize(), m_Connection.GetID());
				}
				else
				{
#ifdef UDPMTUD_DEBUG
					SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: finished MTU discovery; maximum message size is " <<
							 GetMaxMessageSize() << L" bytes for connection " <<  m_Connection.GetID() << SLogFmt(Default));
#endif
				}

				// Disable MTU discovery on socket now that we're done
				if (!m_Connection.SetMTUDiscovery(false))
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

	void MTUDiscovery::ProcessReceivedAck(const Message::SequenceNumber seqnum) noexcept
	{
		if (m_Status == Status::Discovery && m_MTUDMessageData->SequenceNumber == seqnum)
		{
			m_RetransmissionTimeout = std::max(MinRetransmissionTimeout,
											   std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() - m_MTUDMessageData->TimeSent));
			m_MTUDMessageData->Acked = true;
			m_MaximumMessageSize = m_MTUDMessageData->Data.GetSize();
		}
	}

	void MTUDiscovery::AckReceivedMessage(Connection& connection, const Message::SequenceNumber seqnum) noexcept
	{
		try
		{
			Message msg(Message::Type::MTUD, Message::Direction::Outgoing, MinMessageSize);
			msg.SetMessageAckNumber(seqnum);

			Buffer data;
			if (msg.Write(data))
			{
#ifdef UDPMTUD_DEBUG
				SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: sending MTUDAck message on connection " <<
						 connection.GetID() << SLogFmt(Default));
#endif
				const auto result = connection.Send(Util::GetCurrentSteadyTime(), data, false);
				if (result.Failed())
				{
					LogErr(L"UDP connection MTUD: failed to send MTUDAck message on connection %llu",
						   connection.GetID());
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"UDP connection MTUD: failed to send MTUDAck message on connection %llu due to exception: %s",
				   connection.GetID(), Util::ToStringW(e.what()).c_str());
		}
		catch (...)
		{
			LogErr(L"UDP connection MTUD: failed to send MTUDAck message on connection %llu due to unknown exception",
				   connection.GetID());
		}
	}
}