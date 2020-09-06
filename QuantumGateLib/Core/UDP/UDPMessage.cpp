// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPMessage.h"
#include "..\..\Memory\BufferReader.h"
#include "..\..\Memory\BufferWriter.h"

using namespace QuantumGate::Implementation::Memory;

namespace QuantumGate::Implementation::Core::UDP
{
	bool Message::SynHeader::Read(const BufferView& buffer) noexcept
	{
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
		Memory::BufferWriter wrt(buffer, true);
		return wrt.WriteWithPreallocation(m_MessageHMAC,
										  m_MessageSequenceNumber,
										  m_MessageAckNumber,
										  m_ProtocolVersionMajor,
										  m_ProtocolVersionMinor,
										  m_ConnectionID,
										  m_Port);
	}

	bool Message::MsgHeader::Read(const BufferView& buffer) noexcept
	{
		Memory::BufferReader rdr(buffer, true);
		return rdr.Read(m_MessageHMAC,
						m_MessageSequenceNumber,
						m_MessageAckNumber,
						m_MessageType);
	}

	bool Message::MsgHeader::Write(Buffer& buffer) const noexcept
	{
		Memory::BufferWriter wrt(buffer, true);
		return wrt.WriteWithPreallocation(m_MessageHMAC,
										  m_MessageSequenceNumber,
										  m_MessageAckNumber,
										  m_MessageType);
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
			   std::get<MsgHeader>(m_Header).GetMessageType() == Type::MTUD);

		if (!buffer.IsEmpty())
		{
			m_MessageData = std::move(buffer);

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

	const Buffer& Message::GetMessageData() const noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::Data);
		assert(IsValid());

		return m_MessageData;
	}

	Buffer&& Message::MoveMessageData() noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::Data);
		assert(IsValid());

		return std::move(m_MessageData);
	}

	void Message::SetAckSequenceNumbers(Vector<Message::SequenceNumber>&& acks) noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::DataAck);

		if (!acks.empty())
		{
			m_MessageAcks = std::move(acks);
		
			Validate();
		}
	}

	const Vector<Message::SequenceNumber>& Message::GetAckSequenceNumbers() noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).GetMessageType() == Type::DataAck);
		assert(IsValid());

		return m_MessageAcks;
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
		assert(m_Direction == Direction::Incoming);

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
			const auto type = std::get<MsgHeader>(m_Header).GetMessageType();
			if (type == Type::Data)
			{
				m_MessageData = buffer;
			}
			else if (type == Type::DataAck)
			{
				Memory::BufferReader rdr(buffer, true);
				if (!rdr.Read(WithSize(m_MessageAcks, MaxSize::_512B))) return false;
			}
		}

		Validate();

		return true;
	}
	
	bool Message::Write(Buffer& buffer)
	{
		assert(m_Direction == Direction::Outgoing);

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
			const auto type = std::get<MsgHeader>(m_Header).GetMessageType();
			if (type == Type::Data || type == Type::MTUD)
			{
				// Add message data if any
				if (!m_MessageData.IsEmpty())
				{
					msgbuf += m_MessageData;
				}
			}
			else if (type == Type::DataAck)
			{
				Buffer ackbuf;
				Memory::BufferWriter wrt(ackbuf, true);
				if (!wrt.WriteWithPreallocation(WithSize(m_MessageAcks, MaxSize::_512B))) return false;

				msgbuf += ackbuf;
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
		m_Valid = true;
	}
}