// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPMessage.h"
#include "..\..\Memory\BufferReader.h"
#include "..\..\Memory\BufferWriter.h"
#include "..\..\Common\Random.h"
#include "..\..\..\QuantumGateCryptoLib\QuantumGateCryptoLib.h"

using namespace QuantumGate::Implementation::Memory;

namespace QuantumGate::Implementation::Core::UDP
{
	Message::Header::Header(const Type type, const Direction direction) noexcept :
		m_Direction(direction), m_MessageType(type)
	{
		if (m_Direction == Direction::Outgoing)
		{
			switch (m_MessageType)
			{
				case Message::Type::Syn:
				{
					// These are not used for outgoing Syn so we fill with random data
					m_MessageAckNumber = static_cast<UInt16>(Random::GetPseudoRandomNumber());
					break;
				}
				case Message::Type::EAck:
				{
					// Not used for the above message type so we fill with random data
					m_MessageSequenceNumber = static_cast<Message::SequenceNumber>(Random::GetPseudoRandomNumber());
					break;
				}
				case Message::Type::MTUD:
				case Message::Type::Null:
				case Message::Type::Reset:
				{
					// Not used for the above message types so we fill with random data
					m_MessageSequenceNumber = static_cast<Message::SequenceNumber>(Random::GetPseudoRandomNumber());
					m_MessageAckNumber = static_cast<Message::SequenceNumber>(Random::GetPseudoRandomNumber());
					break;
				}
				default:
				{
					break;
				}
			}
		}
	}

	bool Message::Header::Read(const BufferView& buffer) noexcept
	{
		assert(m_Direction == Direction::Incoming);

		BufferSpan hmac(reinterpret_cast<Byte*>(&m_MessageHMAC), sizeof(m_MessageHMAC));
		UInt8 msgtype_flags{ 0 };

		Memory::BufferReader rdr(buffer, true);
		if (rdr.Read(hmac,
					 m_MessageSequenceNumber,
					 m_MessageAckNumber,
					 msgtype_flags))
		{
			m_MessageType = static_cast<Type>(msgtype_flags & MessageTypeMask);
			m_AckFlag = msgtype_flags & AckFlag;
			m_SeqNumFlag = msgtype_flags & SeqNumFlag;

			return true;
		}

		return false;
	}

	bool Message::Header::Write(Buffer& buffer) const noexcept
	{
		assert(m_Direction == Direction::Outgoing);

		BufferView hmac(reinterpret_cast<const Byte*>(&m_MessageHMAC), sizeof(m_MessageHMAC));

		UInt8 msgtype_flags{ static_cast<UInt8>(m_MessageType) };
		
		if (m_AckFlag)
		{
			msgtype_flags = msgtype_flags | AckFlag;
		}

		if (m_SeqNumFlag)
		{
			msgtype_flags = msgtype_flags | SeqNumFlag;
		}

		Memory::BufferWriter wrt(buffer, true);
		return wrt.WriteWithPreallocation(hmac,
										  m_MessageSequenceNumber,
										  m_MessageAckNumber,
										  msgtype_flags);
	}

	void Message::SetMessageData(Buffer&& buffer) noexcept
	{
		assert(m_Header.GetMessageType() == Type::Data ||
			   m_Header.GetMessageType() == Type::Null ||
			   m_Header.GetMessageType() == Type::MTUD);

		if (!buffer.IsEmpty())
		{
			std::get<Buffer>(m_Data) = std::move(buffer);

			Validate();
		}
	}

	Size Message::GetMaxMessageDataSize() const noexcept
	{
		return (m_MaxMessageSize - Header::GetSize());
	}

	Size Message::GetMaxAckRangesPerMessage() const noexcept
	{
		const auto asize = m_MaxMessageSize - Header::GetSize();
		return (asize / sizeof(Message::AckRange));
	}

	void Message::SetSynData(SynData&& data) noexcept
	{
		assert(m_Header.GetMessageType() == Type::Syn);

		m_Data = std::move(data);
	}

	const Message::SynData& Message::GetSynData() const noexcept
	{
		assert(m_Header.GetMessageType() == Type::Syn);
		assert(IsValid());

		return std::get<SynData>(m_Data);
	}

	void Message::SetStateData(StateData&& data) noexcept
	{
		assert(m_Header.GetMessageType() == Type::State);

		m_Data = std::move(data);
	}

	const Message::StateData& Message::GetStateData() const noexcept
	{
		assert(m_Header.GetMessageType() == Type::State);
		assert(IsValid());

		return std::get<StateData>(m_Data);
	}

	const Buffer& Message::GetMessageData() const noexcept
	{
		assert(m_Header.GetMessageType() == Type::Data);
		assert(IsValid());

		return std::get<Buffer>(m_Data);
	}

	Buffer&& Message::MoveMessageData() noexcept
	{
		assert(m_Header.GetMessageType() == Type::Data);
		assert(IsValid());

		return std::move(std::get<Buffer>(m_Data));
	}

	void Message::SetAckRanges(Vector<Message::AckRange>&& acks) noexcept
	{
		assert(m_Header.GetMessageType() == Type::EAck);

		m_Data = std::move(acks);
	}

	const Vector<Message::AckRange>& Message::GetAckRanges() noexcept
	{
		assert(m_Header.GetMessageType() == Type::EAck);
		assert(IsValid());

		return std::get<Vector<AckRange>>(m_Data);
	}

	Size Message::GetHeaderSize() const noexcept
	{
		return m_Header.GetSize();
	}

	bool Message::Read(BufferView buffer)
	{
		// Should have enough data for outer message header
		if (buffer.GetSize() < GetHeaderSize()) return false;

		// Get message outer header from buffer
		if (!m_Header.Read(buffer)) return false;

		// TEMPORARY
		{
			// Calculate HMAC for the message
			BufferView authkeybuf{ reinterpret_cast<const Byte*>(&DefaultAuthKey), sizeof(DefaultAuthKey) };
			BufferView msgview{ buffer };
			msgview.RemoveFirst(sizeof(HMAC));

			const auto hmac = CalcHMAC(msgview, authkeybuf);
			if (m_Header.GetHMAC() != hmac)
			{
				LogErr(L"Failed HMAC check for UDP connection message");
				return false;
			}
		}

		// Remove message header from buffer
		buffer.RemoveFirst(GetHeaderSize());

		switch (m_Header.GetMessageType())
		{
			case Type::Data:
			{
				m_Data = Buffer(buffer);
				break;
			}
			case Type::MTUD:
			case Type::Null:
			{
				// Skip reading unneeded data
				m_Data = Buffer();
				break;
			}
			case Type::EAck:
			{
				// Size should be exact multiple of size of AckRange
				// otherwise something is wrong
				assert(buffer.GetSize() % sizeof(AckRange) == 0);
				if (buffer.GetSize() % sizeof(AckRange) != 0) return false;

				const auto num_ack_ranges = buffer.GetSize() / sizeof(AckRange);

				Vector<AckRange> eacks;
				eacks.resize(num_ack_ranges);
				std::memcpy(eacks.data(), buffer.GetBytes(), buffer.GetSize());
				m_Data = std::move(eacks);
				break;
			}
			case Type::State:
			{
				StateData state_data;
				Memory::BufferReader rdr(buffer, true);
				if (!rdr.Read(state_data.MaxWindowSize, state_data.MaxWindowSizeBytes)) return false;
				m_Data = std::move(state_data);
				break;
			}
			case Type::Syn:
			{
				SynData syn_data;
				Memory::BufferReader rdr(buffer, true);
				if (!rdr.Read(syn_data.ProtocolVersionMajor, syn_data.ProtocolVersionMinor,
							  syn_data.ConnectionID, syn_data.Port)) return false;
				m_Data = std::move(syn_data);
				break;
			}
			default:
			{
				break;
			}
		}

		Validate();

		return true;
	}
	
	bool Message::Write(Buffer& buffer)
	{
		DbgInvoke([&]()
		{
			Validate();
			assert(IsValid());
		});

		Buffer msgbuf;

		// Add message header
		if (!m_Header.Write(msgbuf)) return false;

		switch (m_Header.GetMessageType())
		{
			case Type::Data:
			case Type::MTUD:
			case Type::Null:
			{
				// Add message data if any
				const auto& buffer = std::get<Buffer>(m_Data);
				if (!buffer.IsEmpty())
				{
					msgbuf += buffer;
				}
				break;
			}
			case Type::EAck:
			{
				const auto& eacks = std::get<Vector<AckRange>>(m_Data);

				Buffer ackbuf;
				BufferView ack_view{ reinterpret_cast<const Byte*>(eacks.data()), eacks.size() * sizeof(AckRange) };
				Memory::BufferWriter wrt(ackbuf, true);
				if (!wrt.WriteWithPreallocation(ack_view)) return false;

				msgbuf += ackbuf;
				break;
			}
			case Type::State:
			{
				const auto& state_data = std::get<StateData>(m_Data);

				Buffer statebuf;
				Memory::BufferWriter wrt(statebuf, true);
				if (!wrt.WriteWithPreallocation(state_data.MaxWindowSize, state_data.MaxWindowSizeBytes)) return false;

				msgbuf += statebuf;
				break;
			}
			case Type::Syn:
			{
				const auto& syn_data = std::get<SynData>(m_Data);

				Buffer synbuf;
				Memory::BufferWriter wrt(synbuf, true);
				if (!wrt.WriteWithPreallocation(syn_data.ProtocolVersionMajor, syn_data.ProtocolVersionMinor,
												syn_data.ConnectionID, syn_data.Port)) return false;

				msgbuf += synbuf;
				break;
			}
			default:
			{
				break;
			}
		}

		if (msgbuf.GetSize() > m_MaxMessageSize)
		{
			LogErr(L"Size of UDP message (type %u) combined with header is too large: %zu bytes (Max. is %zu bytes)",
				   GetType(), msgbuf.GetSize(), m_MaxMessageSize);

			return false;
		}

		// TEMPORARY
		{
			// Calculate HMAC for the message
			BufferView authkeybuf{ reinterpret_cast<const Byte*>(&DefaultAuthKey), sizeof(DefaultAuthKey) };
			BufferView msgview{ msgbuf };
			msgview.RemoveFirst(sizeof(HMAC));

			const auto hmac = CalcHMAC(msgview, authkeybuf);
			std::memcpy(msgbuf.GetBytes(), &hmac, sizeof(hmac));
		}

		buffer = std::move(msgbuf);

		return true;
	}

	Message::HMAC Message::CalcHMAC(const BufferView& data, const BufferView& key) noexcept
	{
		// Half SipHash requires key size of 8 bytes
		// and we want 4 byte output size
		assert(key.GetSize() == 8);
		assert(sizeof(HMAC) == 4);

		HMAC hmac{ 0 };

		halfsiphash(reinterpret_cast<const uint8_t*>(data.GetBytes()), data.GetSize(),
					reinterpret_cast<const uint8_t*>(key.GetBytes()),
					reinterpret_cast<uint8_t*>(&hmac), sizeof(hmac));

		return hmac;
	}

	void Message::Validate() noexcept
	{
		m_Valid = false;

		auto type_ok{ false };

		// Check if we have a valid message type
		switch (GetType())
		{
			case Type::Data:
				type_ok = HasAck() && HasSequenceNumber() && std::holds_alternative<Buffer>(m_Data);
				break;
			case Type::State:
				type_ok = HasAck() && HasSequenceNumber() && std::holds_alternative<StateData>(m_Data);
				break;
			case Type::EAck:
				type_ok = HasAck() && std::holds_alternative<Vector<AckRange>>(m_Data);
				break;
			case Type::Syn:
				type_ok = HasSequenceNumber() && std::holds_alternative<SynData>(m_Data);
				break;
			case Type::MTUD:
				type_ok = ((HasSequenceNumber() && !HasAck()) || (!HasSequenceNumber() && HasAck())) && std::holds_alternative<Buffer>(m_Data);
				break;
			case Type::Null:
				type_ok = !HasAck() && !HasSequenceNumber() && std::holds_alternative<Buffer>(m_Data);
				break;
			case Type::Reset:
				type_ok = !HasAck() && !HasSequenceNumber();
				break;
			default:
				LogErr(L"UDP connection: could not validate message: unknown message type %u", GetType());
				break;
		}

		m_Valid = type_ok;
	}
}