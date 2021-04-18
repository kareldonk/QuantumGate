// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPMessage.h"
#include "..\..\Memory\StackBuffer.h"
#include "..\..\Memory\BufferReader.h"
#include "..\..\Memory\BufferWriter.h"
#include "..\..\Common\Random.h"
#include "..\..\..\QuantumGateCryptoLib\QuantumGateCryptoLib.h"

using namespace QuantumGate::Implementation::Memory;

namespace QuantumGate::Implementation::Core::UDP
{
	SymmetricKeys::SymmetricKeys(const ProtectedBuffer& shared_secret)
	{
		m_KeyData.Allocate(KeyDataLength);

		if (!shared_secret.IsEmpty())
		{
			siphash(reinterpret_cast<const uint8_t*>(shared_secret.GetBytes()), shared_secret.GetSize(),
					reinterpret_cast<const uint8_t*>(DefaultKeyData),
					reinterpret_cast<uint8_t*>(m_KeyData.GetBytes()), m_KeyData.GetSize());
		}
		else
		{
			// Use default keys when Global Shared Secret is not in use;
			// this provides basic obfuscation and HMAC checks but won't
			// fool more sophisticated traffic analyzers
			std::memcpy(m_KeyData.GetBytes(), DefaultKeyData, KeyDataLength);
		}
	}

	Message::Header::Header(const Type type, const Direction direction) noexcept :
		m_Direction(direction), m_MessageType(type)
	{
		if (m_Direction == Direction::Outgoing)
		{
			m_MessageIV = static_cast<Message::IV>(Random::GetPseudoRandomNumber());

			switch (m_MessageType)
			{
				case Message::Type::Syn:
				{
					// These are not used for outgoing Syn so we fill with random data
					m_MessageAckNumber = static_cast<Message::SequenceNumber>(Random::GetPseudoRandomNumber());
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
					// Not (always) used for the above message types so we fill with random data;
					// MTUD messages override these in some cases
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
					 m_MessageIV,
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
										  m_MessageIV,
										  m_MessageSequenceNumber,
										  m_MessageAckNumber,
										  msgtype_flags);
	}

	void Message::SetMessageData(Buffer&& buffer) noexcept
	{
		assert(m_Header.GetMessageType() == Type::Data ||
			   m_Header.GetMessageType() == Type::MTUD);

		if (!buffer.IsEmpty())
		{
			std::get<Buffer>(m_Data) = std::move(buffer);

			Validate();
		}
	}

	Size Message::GetMaxMessageDataSize() const noexcept
	{
		return std::min(MaxSize::_65KB,
						(m_MaxMessageSize - (Header::GetSize() + BufferIO::GetSizeOfEncodedSize(MaxSize::_65KB))));
	}

	Size Message::GetMaxAckRangesPerMessage() const noexcept
	{
		const auto asize = std::min(MaxSize::_65KB,
									m_MaxMessageSize - (Header::GetSize() + BufferIO::GetSizeOfEncodedSize(MaxSize::_65KB)));
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

	bool Message::Read(BufferSpan& buffer, const SymmetricKeys& symkey)
	{
		// Should have enough data for outer message header
		if (buffer.GetSize() < GetHeaderSize()) return false;

		// Deobfuscation and HMAC check
		{
			assert(symkey);

			// Calculate and check HMAC for the message
			{
				BufferView msgview{ buffer };

				HMAC hmac{ 0 };
				std::memcpy(&hmac, msgview.GetBytes(), sizeof(HMAC));
				msgview.RemoveFirst(sizeof(HMAC));

				const auto chmac = CalcHMAC(msgview, symkey);
				if (hmac != chmac)
				{
					LogErr(L"Failed HMAC check for UDP connection message");
					return false;
				}
			}

			// Deobfuscate message
			{
				BufferSpan msgspan{ buffer };
				msgspan.RemoveFirst(sizeof(HMAC));

				IV iv{ 0 };
				std::memcpy(&iv, msgspan.GetBytes(), sizeof(IV));
				msgspan.RemoveFirst(sizeof(IV));

				Deobfuscate(msgspan, symkey, iv);
			}
		}

		// Get message outer header from buffer
		if (!m_Header.Read(buffer)) return false;

		// Remove message header from buffer
		buffer.RemoveFirst(GetHeaderSize());

		switch (m_Header.GetMessageType())
		{
			case Type::Data:
			{
				Buffer data;
				Memory::BufferReader rdr(buffer, true);
				if (!rdr.Read(WithSize(data, MaxSize::_65KB))) return false;

				m_Data = std::move(data);
				break;
			}
			case Type::MTUD:
			{
				// Skip reading unneeded data
				m_Data = Buffer();
				break;
			}
			case Type::EAck:
			{
				StackBuffer<MaxSize::_65KB> data;
				Memory::BufferReader rdr(buffer, true);
				if (!rdr.Read(WithSize(data, MaxSize::_65KB))) return false;

				// Size should be exact multiple of size of AckRange
				// otherwise something is wrong
				assert(data.GetSize() % sizeof(AckRange) == 0);
				if (data.GetSize() % sizeof(AckRange) != 0) return false;

				const auto num_ack_ranges = data.GetSize() / sizeof(AckRange);

				Vector<AckRange> eacks;
				eacks.resize(num_ack_ranges);
				std::memcpy(eacks.data(), data.GetBytes(), data.GetSize());
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
	
	bool Message::Write(Buffer& buffer, const SymmetricKeys& symkey)
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
			{
				// Add message data if any
				const auto& data = std::get<Buffer>(m_Data);
				if (!data.IsEmpty())
				{
					StackBuffer<MaxSize::_65KB> dbuf;
					Memory::StackBufferWriter<MaxSize::_65KB> wrt(dbuf, true);
					if (!wrt.WriteWithPreallocation(WithSize(data, MaxSize::_65KB))) return false;

					msgbuf += dbuf;
				}
				break;
			}
			case Type::EAck:
			{
				const auto& eacks = std::get<Vector<AckRange>>(m_Data);

				StackBuffer<MaxSize::_65KB> ackbuf;
				BufferView ack_view{ reinterpret_cast<const Byte*>(eacks.data()), eacks.size() * sizeof(AckRange) };
				Memory::StackBufferWriter<MaxSize::_65KB> wrt(ackbuf, true);
				if (!wrt.WriteWithPreallocation(WithSize(ack_view, MaxSize::_65KB))) return false;

				msgbuf += ackbuf;
				break;
			}
			case Type::State:
			{
				const auto& state_data = std::get<StateData>(m_Data);

				StackBuffer<sizeof(StateData)> statebuf;
				Memory::StackBufferWriter<sizeof(StateData)> wrt(statebuf, true);
				if (!wrt.WriteWithPreallocation(state_data.MaxWindowSize, state_data.MaxWindowSizeBytes)) return false;

				msgbuf += statebuf;
				break;
			}
			case Type::Syn:
			{
				const auto& syn_data = std::get<SynData>(m_Data);

				StackBuffer<sizeof(SynData)> synbuf;
				Memory::StackBufferWriter<sizeof(SynData)> wrt(synbuf, true);
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
		else
		{
			// Add some random padding data at the end of the message
			const auto free_space = m_MaxMessageSize - msgbuf.GetSize();
			if (free_space > 0 &&
				m_Header.GetMessageType() != Type::MTUD && // Excluded because MTUD data needs to be precise size
				m_Header.GetMessageType() != Type::Data && // Excluded for speed
				m_Header.GetMessageType() != Type::EAck) // Excluded for speed
			{
				msgbuf += Random::GetPseudoRandomBytes(Random::GetPseudoRandomNumber(0, free_space));
			}
		}

		// Obfuscation and HMAC
		{
			assert(symkey);

			// Obfuscate message
			{
				BufferSpan msgspan{ msgbuf };
				msgspan.RemoveFirst(sizeof(HMAC));

				IV iv{ 0 };
				std::memcpy(&iv, msgspan.GetBytes(), sizeof(IV));
				msgspan.RemoveFirst(sizeof(IV));

				Obfuscate(msgspan, symkey, iv);
			}

			// Calculate HMAC for the message
			{
				BufferView msgview{ msgbuf };
				msgview.RemoveFirst(sizeof(HMAC));
				const auto hmac = CalcHMAC(msgview, symkey);
				std::memcpy(msgbuf.GetBytes(), &hmac, sizeof(hmac));
			}
		}

		buffer = std::move(msgbuf);

		return true;
	}

	void Message::Obfuscate(BufferSpan& data, const SymmetricKeys& symkey, const IV iv) noexcept
	{
		StackBuffer32 ivkey{ symkey.GetKey() };

		// Initialize key with IV
		{
			assert(ivkey.GetSize() == sizeof(UInt64));
			assert(sizeof(iv) == sizeof(UInt32));
			auto ivkey32 = reinterpret_cast<UInt32*>(ivkey.GetBytes());
			ivkey32[0] ^= iv;
			ivkey32[1] ^= iv;
		}

		const auto rlen = data.GetSize() % sizeof(UInt64);
		const auto len = (data.GetSize() - rlen) / sizeof(UInt64);

		auto data64 = reinterpret_cast<UInt64*>(data.GetBytes());
		const auto key64 = reinterpret_cast<const UInt64*>(ivkey.GetBytes());

		for (Size i = 0; i < len; ++i)
		{
			data64[i] = data64[i] ^ *key64;
		}

		for (Size i = data.GetSize() - rlen; i < data.GetSize(); ++i)
		{
			data[i] = data[i] ^ ivkey[i % ivkey.GetSize()];
		}
	}

	void Message::Deobfuscate(BufferSpan& data, const SymmetricKeys& symkey, const IV iv) noexcept
	{
		Obfuscate(data, symkey, iv);
	}

	Message::HMAC Message::CalcHMAC(const BufferView& data, const SymmetricKeys& symkey) noexcept
	{
		// Half SipHash requires key size of 8 bytes
		// and we want 4 byte output size
		assert(symkey.GetAuthKey().GetSize() == 8);
		assert(sizeof(HMAC) == 4);

		HMAC hmac{ 0 };

		halfsiphash(reinterpret_cast<const uint8_t*>(data.GetBytes()), data.GetSize(),
					reinterpret_cast<const uint8_t*>(symkey.GetAuthKey().GetBytes()),
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
				type_ok = !HasAck() && !HasSequenceNumber();
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