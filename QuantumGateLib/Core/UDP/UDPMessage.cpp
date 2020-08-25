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
						m_ConnectionID);
	}
	
	bool Message::SynHeader::Write(Buffer& buffer) const noexcept
	{
		Memory::BufferWriter wrt(buffer, true);
		return wrt.WriteWithPreallocation(m_MessageHMAC,
										  m_MessageSequenceNumber,
										  m_MessageAckNumber,
										  m_ProtocolVersionMajor,
										  m_ProtocolVersionMinor,
										  m_ConnectionID);
	}

	bool Message::MsgHeader::Read(const BufferView& buffer) noexcept
	{
		Memory::BufferReader rdr(buffer, true);
		return rdr.Read(m_MessageHMAC,
						m_MessageSequenceNumber,
						m_MessageAckNumber,
						m_MessageFlags);
	}

	bool Message::MsgHeader::Write(Buffer& buffer) const noexcept
	{
		Memory::BufferWriter wrt(buffer, true);
		return wrt.WriteWithPreallocation(m_MessageHMAC,
										  m_MessageSequenceNumber,
										  m_MessageAckNumber,
										  m_MessageFlags);
	}

	void Message::SetMessageSequenceNumber(const MessageSequenceNumber seqnum) noexcept
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

	MessageSequenceNumber Message::GetMessageSequenceNumber() const noexcept
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

	void Message::SetMessageAckNumber(const MessageSequenceNumber acknum) noexcept
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
	
	MessageSequenceNumber Message::GetMessageAckNumber() const noexcept
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
	
	void Message::SetMessageData(Buffer&& buffer) noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));

		if (!buffer.IsEmpty())
		{
			m_MessageData = std::move(buffer);

			std::get<MsgHeader>(m_Header).SetData();

			Validate();
		}
	}

	bool Message::IsSyn() const noexcept
	{
		assert(IsValid());
		return std::holds_alternative<SynHeader>(m_Header);
	}

	bool Message::IsNormal() const noexcept
	{
		assert(IsValid());
		return std::holds_alternative<MsgHeader>(m_Header);
	}

	bool Message::IsAck() const noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(IsValid());

		return std::get<MsgHeader>(m_Header).IsAck();
	}

	bool Message::IsData() const noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(IsValid());

		return std::get<MsgHeader>(m_Header).IsData();
	}

	const Buffer& Message::GetMessageData() const noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).IsData());
		assert(IsValid());

		return m_MessageData;
	}

	Buffer&& Message::MoveMessageData() noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).IsData());
		assert(IsValid());

		return std::move(m_MessageData);
	}

	void Message::SetAckSequenceNumbers(Vector<MessageSequenceNumber>&& acks) noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));

		if (!acks.empty())
		{
			m_MessageAcks = std::move(acks);

			std::get<MsgHeader>(m_Header).SetAck();
			
			Validate();
		}
	}

	const Vector<MessageSequenceNumber>& Message::GetAckSequenceNumbers() noexcept
	{
		assert(std::holds_alternative<MsgHeader>(m_Header));
		assert(std::get<MsgHeader>(m_Header).IsAck());

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
			if (std::get<MsgHeader>(m_Header).IsData())
			{
				m_MessageData = buffer;
			}
			else if (std::get<MsgHeader>(m_Header).IsAck())
			{
				Memory::BufferReader rdr(buffer, true);
				if (!rdr.Read(WithSize(m_MessageAcks, MaxAckDataSize))) return false;
			}
		}

		Validate();

		return true;
	}
	
	bool Message::Write(Buffer& buffer)
	{
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
			if (std::get<MsgHeader>(m_Header).IsData())
			{
				// Add message data if any
				if (!m_MessageData.IsEmpty())
				{
					msgbuf += m_MessageData;
				}
			}
			else if (std::get<MsgHeader>(m_Header).IsAck())
			{
				Buffer ackbuf;
				Memory::BufferWriter wrt(ackbuf, true);
				if (!wrt.WriteWithPreallocation(WithSize(m_MessageAcks, MaxAckDataSize))) return false;

				msgbuf += ackbuf;
			}
		}

		if (msgbuf.GetSize() > MaxMessageSize)
		{
			LogErr(L"Size of UDP message data combined with header is too large: %zu bytes (Max. is %zu bytes)",
				   msgbuf.GetSize(), Message::MaxMessageSize);

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