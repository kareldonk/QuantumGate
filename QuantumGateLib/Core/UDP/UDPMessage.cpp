// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPMessage.h"
#include "..\..\Memory\BufferReader.h"
#include "..\..\Memory\BufferWriter.h"
#include "..\..\Common\Random.h"

using namespace QuantumGate::Implementation::Memory;

namespace QuantumGate::Implementation::Core::UDP
{
	Message::SynHeader::SynHeader(const Direction direction) noexcept : m_Direction(direction)
	{
		if (m_Direction == Direction::Outgoing)
		{
			// These are not used for outgoing Syn so we fill with random data
			m_MessageAckNumber = static_cast<UInt16>(Random::GetPseudoRandomNumber());
			m_Port = static_cast<UInt16>(Random::GetPseudoRandomNumber());
		}
	}

	bool Message::SynHeader::Read(const BufferView& buffer) noexcept
	{
		assert(m_Direction == Direction::Incoming);

		Memory::BufferReader rdr(buffer, true);
		return rdr.Read(m_MessageHMAC,
						m_MessageSequenceNumber,
						m_MessageAckNumber,
						m_ProtocolVersionMajor,
						m_ProtocolVersionMinor,
						m_ConnectionID,
						m_Port);
	}
	
	bool Message::SynHeader::Write(Buffer& buffer) const noexcept
	{
		assert(m_Direction == Direction::Outgoing);

		Memory::BufferWriter wrt(buffer, true);
		return wrt.WriteWithPreallocation(m_MessageHMAC,
										  m_MessageSequenceNumber,
										  m_MessageAckNumber,
										  m_ProtocolVersionMajor,
										  m_ProtocolVersionMinor,
										  m_ConnectionID,
										  m_Port);
	}

	Message::MsgHeader::MsgHeader(const Type type, const Direction direction) noexcept :
		m_Direction(direction), m_MessageType(type)
	{
		if (m_Direction == Direction::Outgoing)
		{
			switch (m_MessageType)
			{
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

	bool Message::MsgHeader::Read(const BufferView& buffer) noexcept
	{
		assert(m_Direction == Direction::Incoming);

		UInt8 msgtype_flags{ 0 };

		Memory::BufferReader rdr(buffer, true);
		if (rdr.Read(m_MessageHMAC,
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

	bool Message::MsgHeader::Write(Buffer& buffer) const noexcept
	{
		assert(m_Direction == Direction::Outgoing);

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
		return wrt.WriteWithPreallocation(m_MessageHMAC,
										  m_MessageSequenceNumber,
										  m_MessageAckNumber,
										  msgtype_flags);
	}

	Message::Direction Message::GetDirection() const noexcept
	{
		return std::visit(Util::Overloaded{
			[](const SynHeader& hdr) noexcept
			{
				return hdr.GetDirection();
			},
			[](const MsgHeader& hdr) noexcept
			{
				return hdr.GetDirection();
			}
		}, m_Header);
	}

	bool Message::HasSequenceNumber() const noexcept
	{
		return std::visit(Util::Overloaded{
			[](const SynHeader& hdr) noexcept
			{
				return hdr.HasSequenceNumber();
			},
			[](const MsgHeader& hdr) noexcept
			{
				return hdr.HasSequenceNumber();
			}
		}, m_Header);
	}

	void Message::SetMessageSequenceNumber(const Message::SequenceNumber seqnum) noexcept
	{
		std::visit(Util::Overloaded{
			[&](SynHeader& hdr) noexcept
			{
				hdr.SetMessageSequenceNumber(seqnum);
			},
			[&](MsgHeader& hdr) noexcept
			{
				hdr.SetMessageSequenceNumber(seqnum);
			}
		}, m_Header);
	}

	Message::SequenceNumber Message::GetMessageSequenceNumber() const noexcept
	{
		return std::visit(Util::Overloaded{
			[&](const SynHeader& hdr) noexcept
			{
				return hdr.GetMessageSequenceNumber();
			},
			[&](const MsgHeader& hdr) noexcept
			{
				return hdr.GetMessageSequenceNumber();
			}
		}, m_Header);
	}

	bool Message::HasAck() const noexcept
	{
		return std::visit(Util::Overloaded{
			[](const SynHeader& hdr) noexcept
			{
				return hdr.HasAck();
			},
			[](const MsgHeader& hdr) noexcept
			{
				return hdr.HasAck();
			}
		}, m_Header);
	}

	void Message::SetMessageAckNumber(const Message::SequenceNumber acknum) noexcept
	{
		std::visit(Util::Overloaded{
			[&](SynHeader& hdr) noexcept
			{
				hdr.SetMessageAckNumber(acknum);
			},
			[&](MsgHeader& hdr) noexcept
			{
				hdr.SetMessageAckNumber(acknum);
			}
		}, m_Header);
	}
	
	Message::SequenceNumber Message::GetMessageAckNumber() const noexcept
	{
		return std::visit(Util::Overloaded{
			[](const SynHeader& hdr) noexcept
			{
				return hdr.GetMessageAckNumber();
			},
			[](const MsgHeader& hdr) noexcept
			{
				return hdr.GetMessageAckNumber();
			}
		}, m_Header);
	}

	void Message::SetProtocolVersion(const UInt8 major, const UInt8 minor) noexcept
	{
		assert(std::holds_alternative<SynHeader>(m_Header));
		std::get<SynHeader>(m_Header).SetProtocolVersion(major, minor);
	}

	std::pair<UInt8, UInt8> Message::GetProtocolVersion() const noexcept
	{
		assert(std::holds_alternative<SynHeader>(m_Header));
		return std::get<SynHeader>(m_Header).GetProtocolVersion();
	}

	void Message::SetConnectionID(const ConnectionID id) noexcept
	{
		assert(std::holds_alternative<SynHeader>(m_Header));
		std::get<SynHeader>(m_Header).SetConnectionID(id);
	}

	ConnectionID Message::GetConnectionID() const noexcept
	{
		assert(std::holds_alternative<SynHeader>(m_Header));
		return std::get<SynHeader>(m_Header).GetConnectionID();
	}

	void Message::SetPort(const UInt16 port) noexcept
	{
		assert(std::holds_alternative<SynHeader>(m_Header));
		std::get<SynHeader>(m_Header).SetPort(port);
	}

	UInt16 Message::GetPort() const noexcept
	{
		assert(std::holds_alternative<SynHeader>(m_Header));
		return std::get<SynHeader>(m_Header).GetPort();
	}
	
	void Message::SetMessageData(Buffer&& buffer) noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::Data ||
			   std::get<MsgHeader>(m_Header).GetMessageType() == Type::Null ||
			   std::get<MsgHeader>(m_Header).GetMessageType() == Type::MTUD);

		if (!buffer.IsEmpty())
		{
			m_Data = std::move(buffer);

			Validate();
		}
	}

	Size Message::GetMaxMessageDataSize() const noexcept
	{
		return (m_MaxMessageSize - MsgHeader::GetSize());
	}

	Size Message::GetMaxAckSequenceNumbersPerMessage() const noexcept
	{
		const auto asize = m_MaxMessageSize - (MsgHeader::GetSize() + Memory::BufferIO::GetSizeOfEncodedSize(m_MaxMessageSize));
		return (std::min(asize, static_cast<Size>(512u)) / sizeof(Message::SequenceNumber));
	}

	void Message::SetStateData(StateData&& data) noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::State);

		m_StateData = std::move(data);
	}

	const Message::StateData& Message::GetStateData() const noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::State);
		assert(IsValid());

		return m_StateData;
	}

	const Buffer& Message::GetMessageData() const noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::Data);
		assert(IsValid());

		return m_Data;
	}

	Buffer&& Message::MoveMessageData() noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::Data);
		assert(IsValid());

		return std::move(m_Data);
	}

	void Message::SetAckSequenceNumbers(Vector<Message::SequenceNumber>&& acks) noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::EAck);

		if (!acks.empty())
		{
			m_EAcks = std::move(acks);
		
			Validate();
		}
	}

	const Vector<Message::SequenceNumber>& Message::GetAckSequenceNumbers() noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::EAck);
		assert(IsValid());

		return m_EAcks;
	}

	Size Message::GetHeaderSize() const noexcept
	{
		return std::visit(Util::Overloaded{
			[](const SynHeader& hdr) noexcept
			{
				return hdr.GetSize();
			},
			[](const MsgHeader& hdr) noexcept
			{
				return hdr.GetSize();
			}
		}, m_Header);
	}

	bool Message::Read(BufferView buffer)
	{
		// Should have enough data for outer message header
		if (buffer.GetSize() < GetHeaderSize()) return false;

		// Get message outer header from buffer
		auto success = std::visit(Util::Overloaded{
			[&](SynHeader& hdr) noexcept -> bool
			{
				return hdr.Read(buffer);
			},
			[&](MsgHeader& hdr) noexcept -> bool
			{
				return hdr.Read(buffer);
			}
		}, m_Header);

		if (!success) return false;

		// Remove message header from buffer
		buffer.RemoveFirst(GetHeaderSize());

		if (std::holds_alternative<MsgHeader>(m_Header))
		{
			switch (std::get<MsgHeader>(m_Header).GetMessageType())
			{
				case Type::Data:
				{
					m_Data = buffer;
					break;
				}
				case Type::MTUD:
				case Type::Null:
				{
					// Skip reading unneeded data message
					break;
				}
				case Type::EAck:
				{
					Memory::BufferReader rdr(buffer, true);
					if (!rdr.Read(WithSize(m_EAcks, MaxSize::_512B))) return false;
					break;
				}
				case Type::State:
				{
					Memory::BufferReader rdr(buffer, true);
					if (!rdr.Read(m_StateData.MaxWindowSize, m_StateData.MaxWindowSizeBytes)) return false;
					break;
				}
				default:
				{
					break;
				}
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
		auto result = std::visit(Util::Overloaded{
			[&](SynHeader& hdr) noexcept -> bool
			{
				return hdr.Write(msgbuf);
			},
			[&](MsgHeader& hdr) noexcept -> bool
			{
				return hdr.Write(msgbuf);
			}
		}, m_Header);

		if (!result) return false;

		if (std::holds_alternative<MsgHeader>(m_Header))
		{
			switch (std::get<MsgHeader>(m_Header).GetMessageType())
			{
				case Type::Data:
				case Type::MTUD:
				case Type::Null:
				{
					// Add message data if any
					if (!m_Data.IsEmpty())
					{
						msgbuf += m_Data;
					}
					break;
				}
				case Type::EAck:
				{
					Buffer ackbuf;
					Memory::BufferWriter wrt(ackbuf, true);
					if (!wrt.WriteWithPreallocation(WithSize(m_EAcks, MaxSize::_512B))) return false;

					msgbuf += ackbuf;
					break;
				}
				case Type::State:
				{
					Buffer statebuf;
					Memory::BufferWriter wrt(statebuf, true);
					if (!wrt.WriteWithPreallocation(m_StateData.MaxWindowSize, m_StateData.MaxWindowSizeBytes)) return false;

					msgbuf += statebuf;
					break;
				}
				default:
				{
					break;
				}
			}
		}

		if (msgbuf.GetSize() > m_MaxMessageSize)
		{
			LogErr(L"Size of UDP message data combined with header is too large: %zu bytes (Max. is %zu bytes)",
				   msgbuf.GetSize(), m_MaxMessageSize);

			return false;
		}

		buffer = std::move(msgbuf);

		return true;
	}

	void Message::Validate() noexcept
	{
		m_Valid = false;

		auto type_ok{ false };

		// Check if we have a valid message type
		switch (GetType())
		{
			case Type::Data:
			case Type::State:
				type_ok = HasAck() && HasSequenceNumber();
				break;
			case Type::EAck:
				type_ok = HasAck();
				break;
			case Type::Syn:
				type_ok = HasSequenceNumber();
				break;
			case Type::MTUD:
				type_ok = (HasSequenceNumber() && !HasAck()) || (!HasSequenceNumber() && HasAck());
				break;
			case Type::Null:
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