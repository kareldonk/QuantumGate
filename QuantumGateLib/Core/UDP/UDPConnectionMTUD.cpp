// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnectionMTUD.h"
#include "UDPConnection.h"

using namespace std::chrono_literals;

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	MTUDiscovery::MTUDiscovery(Connection& connection, const std::chrono::milliseconds max_start_delay) noexcept :
		m_Connection(connection)
	{
		m_StartTime = Util::GetCurrentSteadyTime();

		if (max_start_delay > 0ms)
		{
			// Randomize start delay based on maximum delay
			m_StartDelay = std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(
				Random::GetPseudoRandomNumber(0, max_start_delay.count())));
		}
	}

	MTUDiscovery::Status MTUDiscovery::CreateAndTransmitMessage(const Size prev_msg_size, const Size msg_size, const bool final_msg) noexcept
	{
		if (CreateNewMessage(prev_msg_size, msg_size, final_msg))
		{
			return ProcessTransmitResult(TransmitMessage());
		}
		
		return Status::Failed;
	}

	bool MTUDiscovery::CreateNewMessage(const Size prev_msg_size, const Size msg_size, const bool final_msg) noexcept
	{
		try
		{
			Message msg(Message::Type::MTUD, Message::Direction::Outgoing, msg_size);

			const auto snd_size = std::invoke([&]() noexcept
			{
				const auto maxsize = msg.GetMaxMessageDataSize();
				if (maxsize > prev_msg_size)
				{
					return static_cast<Size>(Random::GetPseudoRandomNumber(prev_msg_size, maxsize));
				}
				else return maxsize;
			});

			msg.SetMessageSequenceNumber(static_cast<Message::SequenceNumber>(Random::GetPseudoRandomNumber()));
			msg.SetMessageData(Random::GetPseudoRandomBytes(snd_size));

			Buffer data;
			if (msg.Write(data, m_Connection.GetSymmetricKeys()))
			{
				m_MTUDMessageData.emplace(
					MTUDMessageData{
						.MaximumMessageSize = prev_msg_size,
						.Final = final_msg,
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

		const auto result = m_Connection.Send(now, m_MTUDMessageData->Data, nullptr, std::nullopt);
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

	MTUDiscovery::Status MTUDiscovery::ProcessTransmitResult(const TransmitResult result) noexcept
	{
		switch (result)
		{
			case TransmitResult::Success:
				return Status::Discovery;
			case TransmitResult::MessageTooLarge:
				if (!m_MTUDMessageData->Final)
				{
					return CreateAndTransmitMessage(UDPMessageSizes::All[m_CurrentMessageSizeIndex - 1],
													UDPMessageSizes::All[m_CurrentMessageSizeIndex - 1], true);
				}
				else return Status::Finished;
			case TransmitResult::Failed:
				break;
			default:
				assert(false);
				break;
		}

		return Status::Failed;
	}

	MTUDiscovery::Status MTUDiscovery::Process() noexcept
	{
		const auto now = Util::GetCurrentSteadyTime();

		if (m_Status == Status::Start)
		{
			// MTU discovery is delayed in order to
			// make traffic analysis more difficult
			if (now < m_StartTime + m_StartDelay)
			{
				return Status::Start;
			}
		}

		switch (m_Status)
		{
			case Status::Start:
			{
				// Begin with first/smallest message size
				m_MaximumMessageSize = UDPMessageSizes::Min;
				assert(UDPMessageSizes::All.size() >= 2);
				m_CurrentMessageSizeIndex = 1;

				// Set MTU discovery option on socket which disables fragmentation
				// so that packets that are larger than the path MTU will get dropped
				if (m_Connection.SetMTUDiscovery(true))
				{
#ifdef UDPMTUD_DEBUG
					SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: starting MTU discovery on connection " <<
							 m_Connection.GetID() << SLogFmt(Default));
#endif

					m_Status = CreateAndTransmitMessage(UDPMessageSizes::Min, UDPMessageSizes::All[m_CurrentMessageSizeIndex]);
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
					(now - m_MTUDMessageData->TimeSent >= m_RetransmissionTimeout))
				{
					if (m_MTUDMessageData->NumTries >= MaxNumRetries)
					{
						if (!m_MTUDMessageData->Final)
						{
							m_Status = CreateAndTransmitMessage(UDPMessageSizes::All[m_CurrentMessageSizeIndex - 1],
																UDPMessageSizes::All[m_CurrentMessageSizeIndex - 1], true);
						}
						else
						{
							// Stop retrying
							m_Status = Status::Finished;
						}
					}
					else
					{
						// Retry transmission and see if we get an ack
						m_Status = ProcessTransmitResult(TransmitMessage());
					}
				}
				else if (m_MTUDMessageData->Acked)
				{
					if (m_CurrentMessageSizeIndex == (UDPMessageSizes::All.size() - 1))
					{
						if (!m_MTUDMessageData->Final)
						{
							m_Status = CreateAndTransmitMessage(UDPMessageSizes::All[m_CurrentMessageSizeIndex],
																UDPMessageSizes::All[m_CurrentMessageSizeIndex], true);
						}
						else
						{
							// Reached maximum possible message size
							m_Status = Status::Finished;
						}
					}
					else
					{
						if (!m_MTUDMessageData->Final)
						{
							// Create and send bigger message
							++m_CurrentMessageSizeIndex;
							m_Status = CreateAndTransmitMessage(UDPMessageSizes::All[m_CurrentMessageSizeIndex - 1],
																UDPMessageSizes::All[m_CurrentMessageSizeIndex]);
						}
						else
						{
							// Reached maximum possible message size
							m_Status = Status::Finished;
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
			m_MaximumMessageSize = m_MTUDMessageData->MaximumMessageSize;
		}
	}

	void MTUDiscovery::AckReceivedMessage(Connection& connection, const Message::SequenceNumber seqnum) noexcept
	{
		try
		{
			Message msg(Message::Type::MTUD, Message::Direction::Outgoing, UDPMessageSizes::Min);
			msg.SetMessageAckNumber(seqnum);

			Buffer data;
			if (msg.Write(data, connection.GetSymmetricKeys()))
			{
#ifdef UDPMTUD_DEBUG
				SLogInfo(SLogFmt(FGBrightBlue) << L"UDP connection MTUD: sending MTUDAck message on connection " <<
						 connection.GetID() << SLogFmt(Default));
#endif
				const auto result = connection.Send(Util::GetCurrentSteadyTime(), data, nullptr, std::nullopt);
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