// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "MessageTransport.h"
#include "..\Common\Random.h"
#include "..\Common\Endian.h"
#include "..\Memory\BufferReader.h"
#include "..\Memory\BufferWriter.h"

namespace QuantumGate::Implementation::Core
{
	void MessageTransport::OHeader::Initialize() noexcept
	{
		// Gets a random 64-bit number (8 random bytes)
		const auto rndbytes = Random::GetPseudoRandomNumber();

		// Use the first 4 bytes
		m_MessageNonceSeed = static_cast<UInt32>(rndbytes);

		// Use the last 4 bytes
		m_MessageRandomBits = static_cast<UInt32>(rndbytes >> 32);

		Dbg(L"MsgTOHdr Random bytes: %llu : 0b%s", rndbytes, Util::ToBinaryString(rndbytes).c_str());
		Dbg(L"MsgTOHdr Random bits: %u : 0b%s", m_MessageRandomBits, Util::ToBinaryString(m_MessageRandomBits).c_str());
		Dbg(L"MsgTOHdr Nonce seed: %u : 0b%s", m_MessageNonceSeed, Util::ToBinaryString(m_MessageNonceSeed).c_str());
	}

	const bool MessageTransport::OHeader::Read(const BufferView& buffer)
	{
		assert(buffer.GetSize() >= OHeader::GetSize());

		UInt32 size{ 0 };
		m_MessageHMAC.Allocate(MessageHMACSize);

		Memory::BufferReader rdr(buffer, true);
		if (rdr.Read(size, m_MessageNonceSeed, m_MessageHMAC))
		{
			m_MessageDataSize = DeObfuscateMessageDataSize(m_MessageDataSizeSettings, size);

			return true;
		}

		return false;
	}

	const bool MessageTransport::OHeader::Write(Buffer& buffer) const
	{
		const auto size = ObfuscateMessageDataSize(m_MessageDataSizeSettings, m_MessageRandomBits, m_MessageDataSize);

		Memory::BufferWriter wrt(buffer, true);
		return wrt.WriteWithPreallocation(size, m_MessageNonceSeed, m_MessageHMAC);
	}

	UInt32 MessageTransport::OHeader::ObfuscateMessageDataSize(const DataSizeSettings mds_settings,
															   const UInt32 rnd_bits,
															   UInt32 size) noexcept
	{
		static_assert(CHAR_BIT == 8, L"Code below assumes a byte has 8 bits");

		// First 4 bytes are a combination of random bits
		// and data size stored in little endian format, example:
		// 0bRRRRDDDD'DDDDDDDD'DDDDDDDD'RRRRRRRR
		// R = Random bits
		// D = MessageTransport data size bits

		Dbg(L"MsgTDSOffset: %u bits", mds_settings.Offset);
		Dbg(L"MsgTDSXOR bytes: 0b%s", Util::ToBinaryString(mds_settings.XOR).c_str());

		size = size << mds_settings.Offset;
		const auto mask = 0x000FFFFF << mds_settings.Offset;
		size |= (rnd_bits & ~mask);

		Dbg(L"MsgTOHdr first 4 bytes:\t0b%s", Util::ToBinaryString(size).c_str());

		size ^= mds_settings.XOR;

		Dbg(L"MsgTOHdr first 4 bytes (XORed):\t0b%s", Util::ToBinaryString(size).c_str());

		return size;
	}

	UInt32 MessageTransport::OHeader::DeObfuscateMessageDataSize(const DataSizeSettings mds_settings,
																 UInt32 size) noexcept
	{
		static_assert(CHAR_BIT == 8, L"Code below assumes a byte has 8 bits");

		// First 4 bytes are a combination of random bits
		// and data size stored in little endian format, example:
		// 0bRRRRDDDD'DDDDDDDD'DDDDDDDD'RRRRRRRR
		// R = Random bits
		// D = MessageTransport data size bits

		size ^= mds_settings.XOR;

		const auto mask = 0x000FFFFF << mds_settings.Offset;
		size = (size & mask) >> mds_settings.Offset;
		// rnd_bits = (size & ~mask);

		return size;
	}

	void MessageTransport::IHeader::Initialize() noexcept
	{
		m_MessageTime = Util::ToTimeT(Util::GetCurrentSystemTime());
	}

	const bool MessageTransport::IHeader::Read(const BufferView& buffer)
	{
		assert(buffer.GetSize() >= IHeader::GetSize());

		Memory::BufferReader rdr(buffer, true);
		if (rdr.Read(m_MessageCounter, m_MessageTime, m_NextRandomDataPrefixLength, m_RandomDataSize))
		{
			return true;
		}

		LogErr(L"Could not read message iheader");

		return false;
	}

	const bool MessageTransport::IHeader::Write(Buffer& buffer) const
	{
		Buffer rnddata;
		if (m_RandomDataSize > 0)
		{
			rnddata = Random::GetPseudoRandomBytes(m_RandomDataSize);

			Dbg(L"MsgTIHdr Random data: %d bytes - %s", rnddata.GetSize(), Util::GetBase64(rnddata)->c_str());
		}

		Memory::BufferWriter wrt(buffer, true);
		return wrt.WriteWithPreallocation(m_MessageCounter, m_MessageTime, m_NextRandomDataPrefixLength,
										  m_RandomDataSize, rnddata);
	}

	const SystemTime MessageTransport::IHeader::GetMessageTime() const noexcept
	{
		return Util::ToTime(m_MessageTime);
	}

	void MessageTransport::IHeader::SetRandomDataSize(const Size minrndsize, const Size maxrndsize) noexcept
	{
		// Only supports random data size up to UInt16 (2^16)
		assert(minrndsize <= std::numeric_limits<UInt16>::max() && maxrndsize <= std::numeric_limits<UInt16>::max());

		m_RandomDataSize = static_cast<UInt16>(abs(Random::GetPseudoRandomNumber(minrndsize, maxrndsize)));

		Dbg(L"MsgTIHdr Random data size: %u", m_RandomDataSize);
	}

	MessageTransport::MessageTransport(const DataSizeSettings mds_settings, const Settings& settings) noexcept :
		m_Settings(settings), m_OHeader(mds_settings)
	{
		static_assert((MessageTransport::OHeader::GetSize() + MessageTransport::IHeader::GetSize() +
					   MessageTransport::MaxMessageAndRandomDataSize) <= MessageTransport::MaxMessageSize,
					  "MessageTransport header and data sizes violate maximum allowed");

		Dbg(L"MessageTransport sizes: OHdr: %u, IHdr: %u, MaxRndData: %u, MaxMsg: %u",
			MessageTransport::OHeader::GetSize(), MessageTransport::IHeader::GetSize(),
			MessageTransport::MaxMessageAndRandomDataSize, MessageTransport::MaxMessageSize);

		assert(mds_settings.Offset <= MessageTransport::MaxMessageDataSizeOffset);

		m_OHeader.Initialize();

		m_IHeader.Initialize();

		// If we should add random data
		if (settings.Message.MaxInternalRandomDataSize > 0)
		{
			m_IHeader.SetRandomDataSize(settings.Message.MinInternalRandomDataSize, settings.Message.MaxInternalRandomDataSize);
		}

		Validate();
	}

	void MessageTransport::SetMessageData(Buffer&& buffer) noexcept
	{
		if (!buffer.IsEmpty())
		{
			const Size mds = buffer.GetSize();

			assert(mds <= MessageTransport::MaxMessageDataSize);

			m_OHeader.SetMessageDataSize(mds);
			m_MessageData = std::move(buffer);

			// If we should add random data
			if (m_Settings.Message.MaxInternalRandomDataSize > 0)
			{
				Size minrds = m_Settings.Message.MinInternalRandomDataSize;
				Size maxrds = m_Settings.Message.MaxInternalRandomDataSize;

				// Make sure that the random data size plus the message data size
				// will not exceed the maximum allowed message data size; if it does
				// then make the max random data size smaller (there will always be
				// room for at least 0-64 bytes of random data due to difference between 
				// MaxMessageAndRandomDataSize and MaxMessageDataSize)
				if (MessageTransport::MaxMessageAndRandomDataSize - mds < maxrds)
				{
					minrds = 0;
					maxrds = MessageTransport::MaxMessageAndRandomDataSize - mds;
				}

				m_IHeader.SetRandomDataSize(minrds, maxrds);
			}

			Validate();
		}
	}

	const Buffer& MessageTransport::GetMessageData() const noexcept
	{
		return m_MessageData;
	}

	void MessageTransport::Validate() noexcept
	{
		m_Valid = false;

		// If there's message data its size should not exceed maximum allowed
		if (m_MessageData.GetSize() > MessageTransport::MaxMessageDataSize)
		{
			LogErr(L"Could not validate message transport: message data too large (Max. is %u bytes)",
				   MessageTransport::MaxMessageDataSize);

			return;
		}

		m_Valid = true;
	}

	const SystemTime MessageTransport::GetMessageTime() const noexcept
	{
		return m_IHeader.GetMessageTime();
	}

	const std::pair<bool, bool> MessageTransport::Read(BufferView buffer,
													   Crypto::SymmetricKeyData& symkey,
													   const BufferView& nonce)
	{
		assert(buffer.GetSize() >= OHeader::GetSize());
		assert(!nonce.IsEmpty());

		// Should have enough data for outer message header
		if (buffer.GetSize() < OHeader::GetSize()) return std::make_pair(false, false);

		// Get message outer header from buffer
		if (!m_OHeader.Read(buffer)) return std::make_pair(false, false);

		auto success = false;
		auto retry = false;

		// If we have message data get it
		if (m_OHeader.GetMessageDataSize() > 0)
		{
			// Remove outer message header from buffer
			buffer.RemoveFirst(OHeader::GetSize());

			// Remaining buffer size should match data size otherwise something is wrong
			if (m_OHeader.GetMessageDataSize() == buffer.GetSize())
			{
				Buffer hmac;

				// Calculate message HMAC
				if (Crypto::HMAC(buffer, hmac, symkey.AuthKey, Algorithm::Hash::BLAKE2S256))
				{
					assert(hmac.GetSize() == OHeader::MessageHMACSize);

					// Check if message data corresponds to HMAC
					if (Crypto::CompareBuffers(m_OHeader.GetHMACBuffer(), hmac))
					{
						Buffer decrbuf;

						// Decrypt message data
						if (Crypto::Decrypt(buffer, decrbuf, symkey, nonce))
						{
							// Get message inner header from buffer
							if (m_IHeader.Read(decrbuf))
							{
								// Remove inner message header and random padding data (if any) from buffer
								decrbuf.RemoveFirst(IHeader::GetSize() + m_IHeader.GetRandomDataSize());

								// Rest of message is message data
								if (!decrbuf.IsEmpty())
								{
									m_MessageData = std::move(decrbuf);
								}

								success = true;
							}
						}
						else LogErr(L"Could not decrypt message data");
					}
					else
					{
						LogDbg(L"Incorrect message HMAC");

						// If the message HMAC wasn't correct it could mean the message was encrypted
						// using a different key, so we'll try again with another key if we have one
						retry = true;
					}
				}
				else LogErr(L"MessageTransport HMAC could not be computed");
			}
			else LogDbg(L"MessageTransport data length mismatch");
		}
		else LogDbg(L"MessageTransport has no data");

		if (success) Validate();

		return std::make_pair(success, retry);
	}

	const bool MessageTransport::Write(Buffer& buffer, Crypto::SymmetricKeyData& symkey, const BufferView& nonce)
	{
		assert(!nonce.IsEmpty());

		Buffer msgdatabuf;

		// Add inner message header
		if (!m_IHeader.Write(msgdatabuf)) return false;

		// Add message data if any
		if (!m_MessageData.IsEmpty())
		{
			msgdatabuf += m_MessageData;
		}

		if (msgdatabuf.GetSize() > (MessageTransport::IHeader::GetSize() + MessageTransport::MaxMessageAndRandomDataSize))
		{
			LogErr(L"Size of MessageTransport data combined with random data is too large: %u bytes (Max. is %u bytes)",
				   msgdatabuf.GetSize(), MessageTransport::MaxMessageAndRandomDataSize);

			return false;
		}

		auto msgohdr = m_OHeader;
		Buffer encrdata;

		// Encrypt message
		if (Crypto::Encrypt(msgdatabuf, encrdata, symkey, nonce))
		{
			msgohdr.SetMessageDataSize(encrdata.GetSize());

			// Calculate HMAC for the encrypted message
			if (Crypto::HMAC(encrdata, msgohdr.GetHMACBuffer(), symkey.AuthKey, Algorithm::Hash::BLAKE2S256))
			{
				assert(msgohdr.GetHMACBuffer().GetSize() == OHeader::MessageHMACSize);

				Dbg(L"MessageTransport hash: %s", Util::GetBase64(msgohdr.GetHMACBuffer())->c_str());

				auto& msgbuffer = msgdatabuf;
				msgbuffer.Clear();

				// First get the outer message header into the output buffer, then
				// add inner message header and message data to the output buffer
				if (msgohdr.Write(msgbuffer))
				{
					msgbuffer += encrdata;

					Dbg(L"Send buffer: %d bytes - %s", msgbuffer.GetSize(), Util::GetBase64(msgbuffer)->c_str());

					if (msgbuffer.GetSize() <= MessageTransport::MaxMessageSize)
					{
						if (m_RandomDataPrefixLength > 0)
						{
							buffer.Preallocate(m_RandomDataPrefixLength + msgbuffer.GetSize());
							buffer = Random::GetPseudoRandomBytes(m_RandomDataPrefixLength);
							buffer += msgbuffer;
						}
						else buffer = std::move(msgbuffer);

						Dbg(L"Send buffer plus random data prefix: %d bytes - %s",
							buffer.GetSize(), Util::GetBase64(buffer)->c_str());

						return true;
					}
					else LogErr(L"MessageTransport size too large: %u bytes (Max. is %u bytes)",
								msgbuffer.GetSize(), MessageTransport::MaxMessageSize);
				}
			}
			else LogErr(L"Could not compute MessageTransport HMAC");
		}
		else LogErr(L"Could not encrypt MessageTransport data");

		return false;
	}

	const MessageTransportCheck MessageTransport::Peek(const UInt16 rndp_len, const DataSizeSettings mds_settings,
													   const Buffer& srcbuf) noexcept
	{
		// Check if buffer has enough data for outer MessageTransport header
		if (srcbuf.GetSize() < rndp_len + OHeader::GetSize()) return MessageTransportCheck::NotEnoughData;

		const UInt32 mdsize = OHeader::DeObfuscateMessageDataSize(mds_settings,
																  Endian::FromNetworkByteOrder(
																	  *reinterpret_cast<const UInt32*>(
																		  srcbuf.GetBytes() + rndp_len)));
		const auto msglen = OHeader::GetSize() + mdsize;

		// Check if message size is too large (might be bad data)
		if (msglen > MessageTransport::MaxMessageSize)
		{
			return MessageTransportCheck::TooMuchData;
		}

		// Check if buffer has enough data for a complete message
		if (srcbuf.GetSize() >= msglen + rndp_len)
		{
			return MessageTransportCheck::CompleteMessage;
		}

		return MessageTransportCheck::NotEnoughData;
	}

	const MessageTransportCheck MessageTransport::GetFromBuffer(const UInt16 rndp_len, const DataSizeSettings mds_settings,
																Buffer& srcbuf, Buffer& destbuf)
	{
		// Check if buffer has enough data for outer MessageTransport header
		if (srcbuf.GetSize() < rndp_len + OHeader::GetSize()) return MessageTransportCheck::NotEnoughData;

		BufferView srcbufview(srcbuf);
		srcbufview.RemoveFirst(rndp_len);

		OHeader hdr(mds_settings);
		if (hdr.Read(srcbufview))
		{
			const auto msglen = hdr.GetSize() + hdr.GetMessageDataSize();

			// If buffer has enough data for a complete message read
			// the message out and remove it from the buffer
			if (srcbufview.GetSize() >= msglen)
			{
				destbuf.Allocate(msglen);
				memcpy(destbuf.GetBytes(), srcbufview.GetBytes(), msglen);

				srcbuf.RemoveFirst(rndp_len + msglen);

				return MessageTransportCheck::CompleteMessage;
			}
		}

		return MessageTransportCheck::NotEnoughData;
	}

	std::optional<UInt32> MessageTransport::GetNonceSeedFromBuffer(const BufferView& srcbuf) noexcept
	{
		// Buffer should at least have the MessageTransport header
		if (srcbuf.GetSize() >= OHeader::GetSize())
		{
			// Nonce seed starts at 5th byte and is 4 bytes long stored in network byte order 
			UInt32 nseed = Endian::FromNetworkByteOrder(*reinterpret_cast<const UInt32*>(srcbuf.GetBytes() + 4));
			return { nseed };
		}

		return std::nullopt;
	}
}
