// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Message.h"
#include "..\Compression\Compression.h"
#include "..\Memory\BufferReader.h"
#include "..\Memory\BufferWriter.h"

namespace QuantumGate::Implementation::Core
{
	void Message::Header::Initialize(const MessageOptions& msgopt) noexcept
	{
		m_MessageType = msgopt.MessageType;
		m_ExtenderUUID = msgopt.ExtenderUUID;
		m_MessageDataSize = static_cast<UInt32>(msgopt.MessageData.GetSize());

		switch (msgopt.Fragment)
		{
			case MessageFragmentType::Complete:
				// This is the default
				break;
			case MessageFragmentType::PartialBegin:
				SetMessageFlag(MessageFlag::PartialBegin, true);
				break;
			case MessageFragmentType::Partial:
				SetMessageFlag(MessageFlag::Partial, true);
				break;
			case MessageFragmentType::PartialEnd:
				SetMessageFlag(MessageFlag::PartialEnd, true);
				break;
			default:
				// Shouldn't get here
				assert(false);
				break;
		}
	}

	Size Message::Header::GetSize() noexcept
	{
		switch (m_MessageType)
		{
			case MessageType::ExtenderCommunication:
				return GetMaxSize();
			case MessageType::Unknown:
				assert(false);
				break;
			default:
				break;
		}

		return GetMinSize();
	}

	const bool Message::Header::Read(const BufferView& buffer)
	{
		assert(buffer.GetSize() >= Header::GetMinSize());

		UInt32 data{ 0 };
		SerializedUUID suuid;

		Memory::BufferReader rdr(buffer, true);
		if (rdr.Read(data, m_MessageFlags))
		{
			// First 4 bytes are a combination of message size and type
			// stored in little endian format:
			// 0bDDDDDDDD'DDDSSSSS'SSSSSSSS'SSSSSSSS
			// D = Message type bits
			// S = Message size bits

			static_assert(CHAR_BIT == 8, "Code below assumes a byte has 8 bits");

			m_MessageDataSize = (data & MessageDataSizeMask);
			m_MessageType = static_cast<MessageType>((data >> 21) & MessageTypeMask);

			if (m_MessageType == MessageType::ExtenderCommunication)
			{
				if (rdr.Read(suuid))
				{
					m_ExtenderUUID = suuid;
					if (m_ExtenderUUID.GetType() == UUID::Type::Extender)
					{
						return true;
					}
					else LogErr(L"Invalid extender UUID in message payload header");
				}
				else LogErr(L"Could not read message payload header");
			}
			else return true;
		}
		else LogErr(L"Could not read message payload header");

		return false;
	}

	const bool Message::Header::Write(Buffer& buffer) const
	{
		// First 4 bytes are a combination of message size and type
		// stored in little endian format:
		// 0bDDDDDDDD'DDDSSSSS'SSSSSSSS'SSSSSSSS
		// D = Message type bits
		// S = Message size bits

		static_assert(CHAR_BIT == 8, "Code below assumes a byte has 8 bits");

		const UInt32 data = ((static_cast<UInt16>(m_MessageType) & MessageTypeMask) << 21) |
			(m_MessageDataSize & MessageDataSizeMask);

		Dbg(L"MsgHdr first 4 bytes: 0b%s", Util::ToBinaryString(data).c_str());

		Memory::BufferWriter wrt(buffer, true);

		if (m_MessageType == MessageType::ExtenderCommunication)
		{
			return wrt.WriteWithPreallocation(data, m_MessageFlags, SerializedUUID{ m_ExtenderUUID });
		}
		else return wrt.WriteWithPreallocation(data, m_MessageFlags);
	}

	Message::Message() noexcept
	{
		static_assert((Message::Header::GetMaxSize() +
					   Message::MaxMessageDataSize) == MessageTransport::MaxMessageDataSize,
					  "Message header and data size do not match maximum allowed size");

		Dbg(L"Message payload sizes: Hdr: %u, MaxData: %u, MaxMsg: %u",
			Message::Header::GetMaxSize(), Message::MaxMessageDataSize, MessageTransport::MaxMessageDataSize);
	}

	Message::Message(const MessageOptions& msgopt) noexcept : Message()
	{
		Initialize(msgopt);
	}

	void Message::Initialize(const MessageOptions& msgopt) noexcept
	{
		m_UseCompression = msgopt.UseCompression;

		m_Header.Initialize(msgopt);
		m_MessageData = std::move(msgopt.MessageData);

		Validate();
	}

	const ExtenderUUID& Message::GetExtenderUUID() const noexcept
	{
		assert(IsValid());

		return m_Header.GetExtenderUUID();
	}

	const Buffer& Message::GetMessageData() const noexcept
	{
		assert(IsValid());

		return m_MessageData;
	}

	Buffer&& Message::MoveMessageData() noexcept
	{
		assert(IsValid());

		return std::move(m_MessageData);
	}

	const bool Message::Read(BufferView buffer, const Crypto::SymmetricKeyData& symkey)
	{
		assert(buffer.GetSize() >= Header::GetMinSize());

		// Should have enough data for message header
		if (buffer.GetSize() < Header::GetMinSize()) return false;

		// Get message header from buffer
		if (!m_Header.Read(buffer)) return false;

		auto success = true;

		// If we have message data get it
		if (m_Header.GetMessageDataSize() > 0)
		{
			// Remove outer message header from buffer
			buffer.RemoveFirst(m_Header.GetSize());

			// Remaining buffer size should match data size otherwise something is wrong
			if (m_Header.GetMessageDataSize() == buffer.GetSize())
			{
				if (m_Header.IsCompressed())
				{
					Buffer decombuf;

					// Decompress data while providing a maximum allowable size
					// to protect against decompression bomb attack or bad data
					if (Compression::Decompress(buffer, decombuf, symkey.CompressionAlgorithm,
												Message::MaxMessageDataSize))
					{
						m_Header.SetMessageFlag(MessageFlag::Compressed, false);
						m_Header.SetMessageDataSize(decombuf.GetSize());

						m_MessageData = std::move(decombuf);
					}
					else
					{
						LogErr(L"Could not decompress message data");
						success = false;
					}
				}
				else m_MessageData = buffer;
			}
			else
			{
				LogDbg(L"Message data length mismatch");
				success = false;
			}
		}

		if (success) Validate();

		return success;
	}

	const bool Message::Write(Buffer& buffer, const Crypto::SymmetricKeyData& symkey)
	{
		const bool hasmsgdata = !m_MessageData.IsEmpty();
		Buffer tmpdata;

		auto msghdr = m_Header;

		if (hasmsgdata && m_UseCompression &&
			m_MessageData.GetSize() >= Message::MinMessageDataSizeForCompression)
		{
			// These types should not get compressed
			assert(GetMessageType() != MessageType::Noise &&
				   GetMessageType() != MessageType::RelayData);

			// Compress message data
			if (Compression::Compress(m_MessageData, tmpdata, symkey.CompressionAlgorithm))
			{
				// If the compressed message data is indeed smaller, use it, 
				// otherwise send message uncompressed
				if (tmpdata.GetSize() < m_MessageData.GetSize())
				{
					Dbg(L"Message data compressed to %u bytes (was %u bytes)",
						tmpdata.GetSize(), m_MessageData.GetSize());

					msghdr.SetMessageDataSize(tmpdata.GetSize());
					msghdr.SetMessageFlag(MessageFlag::Compressed, true);
				}
				else
				{
					Dbg(L"Message data compressed to %u bytes (was %u bytes); will send uncompressed",
						tmpdata.GetSize(), m_MessageData.GetSize());

					msghdr.SetMessageDataSize(m_MessageData.GetSize());
					msghdr.SetMessageFlag(MessageFlag::Compressed, false);
				}
			}
			else
			{
				LogErr(L"Could not compress message data");
				return false;
			}
		}

		// Add message header
		if (!msghdr.Write(buffer)) return false;

		// Add message data if any
		if (hasmsgdata)
		{
			if (msghdr.IsCompressed())
			{
				buffer += tmpdata;
			}
			else buffer += m_MessageData;
		}

		if (buffer.GetSize() > MessageTransport::MaxMessageDataSize)
		{
			LogErr(L"Size of message data is too large: %u bytes (Max. is %u bytes)",
				   buffer.GetSize(), MessageTransport::MaxMessageDataSize);

			return false;
		}

		return true;
	}

	void Message::Validate() noexcept
	{
		m_Valid = false;

		// Check if we have a valid message type
		switch (m_Header.GetMessageType())
		{
			case MessageType::ExtenderCommunication:
			case MessageType::Noise:
			case MessageType::BeginPrimaryKeyUpdateExchange:
			case MessageType::EndPrimaryKeyUpdateExchange:
			case MessageType::BeginSecondaryKeyUpdateExchange:
			case MessageType::EndSecondaryKeyUpdateExchange:
			case MessageType::KeyUpdateReady:
			case MessageType::ExtenderUpdate:
			case MessageType::RelayCreate:
			case MessageType::RelayStatus:
			case MessageType::RelayData:
			case MessageType::BeginMetaExchange:
			case MessageType::EndMetaExchange:
			case MessageType::BeginPrimaryKeyExchange:
			case MessageType::EndPrimaryKeyExchange:
			case MessageType::BeginSecondaryKeyExchange:
			case MessageType::EndSecondaryKeyExchange:
			case MessageType::BeginAuthentication:
			case MessageType::EndAuthentication:
			case MessageType::BeginSessionInit:
			case MessageType::EndSessionInit:
				break;
			default:
				LogErr(L"Could not validate message: unknown message type %u", m_Header.GetMessageType());
				return;
		}

		if (m_Header.GetMessageDataSize() != m_MessageData.GetSize())
		{
			LogErr(L"Could not validate message: message data size in header doesn't match actual data size");
			return;
		}

		// If there's message data its size should not exceed maximum allowed
		if (m_MessageData.GetSize() > Message::MaxMessageDataSize)
		{
			LogErr(L"Could not validate message: message data (%u bytes) too large (Max. is %u bytes)",
				   m_MessageData.GetSize(), Message::MaxMessageDataSize);
			return;
		}

		// If we get here all checks were successful
		m_Valid = true;
	}

	const BufferView Message::GetFromBuffer(BufferView& srcbuf)
	{
		// Check if buffer has enough data for message header
		if (srcbuf.GetSize() < Header::GetMinSize()) return nullptr;

		Header hdr;
		if (hdr.Read(srcbuf))
		{
			const auto msglen = hdr.GetSize() + hdr.GetMessageDataSize();

			if (srcbuf.GetSize() >= msglen)
			{
				auto destbuf = BufferView(srcbuf.GetBytes(), msglen);
				srcbuf.RemoveFirst(msglen);

				return destbuf;
			}
		}

		return nullptr;
	}
}