// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "MessageTypes.h"
#include "MessageTransport.h"
#include "..\Memory\BufferIO.h"

namespace QuantumGate::Implementation::Core
{
	enum class MessageFragmentType
	{
		Unknown, Complete, PartialBegin, Partial, PartialEnd
	};

	enum class MessageFlag : UInt8
	{
		PartialBegin = 0b00000001,
		Partial = 0b00000010,
		PartialEnd = 0b00000100,
		Compressed = 0b00001000,
	};

	struct MessageOptions final
	{
	public:
		MessageOptions(const MessageType type, Buffer&& msgdata, const bool compress = true,
					   const MessageFragmentType fragment = MessageFragmentType::Complete) noexcept :
			MessageOptions(type, DefaultExtenderUUID, std::move(msgdata), compress, fragment)
		{}

		MessageOptions(const MessageType type, const ExtenderUUID& extuuid, Buffer&& msgdata,
					   const bool compress = true, const MessageFragmentType fragment = MessageFragmentType::Complete) noexcept :
			MessageType(type), ExtenderUUID(extuuid), MessageData(std::move(msgdata)), UseCompression(compress),
			Fragment(fragment)
		{}

	private:
		inline static constexpr ExtenderUUID DefaultExtenderUUID{ 0x0, 0x0, 0x0900, 0x0600000000000000 }; // "00000000-0000-0900-0600-000000000000"

	public:
		MessageType MessageType{ MessageType::Unknown };
		ExtenderUUID ExtenderUUID;
		Buffer MessageData;
		bool UseCompression{ true };
		MessageFragmentType Fragment{ MessageFragmentType::Complete };
	};

	class Message final
	{
		class Header final
		{
		public:
			Header() noexcept {}
			Header(const Header&) noexcept = default;
			Header(Header&&) noexcept = default;
			~Header() = default;
			Header& operator=(const Header&) noexcept = default;
			Header& operator=(Header&&) noexcept = default;

			void Initialize(const MessageOptions& msgopt) noexcept;

			static constexpr Size GetMinSize() noexcept
			{
				return 4 + // 4 bytes for m_MessageDataSize and m_MessageType combined
					sizeof(m_MessageFlags);
			}

			static constexpr Size GetMaxSize() noexcept
			{
				return GetMinSize() +
					sizeof(SerializedUUID);
			}

			Size GetSize() noexcept;

			[[nodiscard]] bool Read(const BufferView& buffer) noexcept;
			[[nodiscard]] bool Write(Buffer& buffer) const noexcept;

			[[nodiscard]] inline bool IsCompressed() const noexcept
			{
				return (m_MessageFlags & static_cast<UInt8>(MessageFlag::Compressed));
			}

			inline void SetMessageFlag(const MessageFlag flag, const bool state) noexcept
			{
				if (state) m_MessageFlags |= static_cast<UInt8>(flag);
				else m_MessageFlags &= ~static_cast<UInt8>(flag);
			}

			inline void SetMessageDataSize(const Size size) noexcept { m_MessageDataSize = static_cast<UInt32>(size); }
			inline Size GetMessageDataSize() const noexcept { return m_MessageDataSize; }

			inline MessageType GetMessageType() const noexcept { return m_MessageType; }

			inline MessageFragmentType GetMessageFragmentType() const noexcept
			{
				if (!(m_MessageFlags & 0x07)) return MessageFragmentType::Complete;
				else if (m_MessageFlags & static_cast<UInt8>(MessageFlag::PartialBegin)) return MessageFragmentType::PartialBegin;
				else if (m_MessageFlags & static_cast<UInt8>(MessageFlag::Partial)) return MessageFragmentType::Partial;
				else if (m_MessageFlags & static_cast<UInt8>(MessageFlag::PartialEnd)) return MessageFragmentType::PartialEnd;

				// Shouldn't get here
				assert(false);

				return MessageFragmentType::Unknown;
			}

			inline const ExtenderUUID& GetExtenderUUID() const noexcept { return m_ExtenderUUID; }

		private:
			static constexpr UInt32 MessageDataSizeMask{ 0b00000000'00011111'11111111'11111111 };
			static constexpr UInt32 MessageTypeMask{ 0b00000111'11111111 };

		private:
			UInt32 m_MessageDataSize{ 0 };
			MessageType m_MessageType{ MessageType::Unknown };
			UInt8 m_MessageFlags{ 0 };
			ExtenderUUID m_ExtenderUUID;
		};

	public:
		Message() noexcept;
		Message(MessageOptions&& msgopt) noexcept;
		Message(const Message&) = delete;
		Message(Message&&) noexcept = default;
		~Message() = default;
		Message& operator=(const Message&) = delete;
		Message& operator=(Message&&) noexcept = default;

		[[nodiscard]] inline bool IsValid() const noexcept { return m_Valid; }

		inline MessageType GetMessageType() const noexcept { return m_Header.GetMessageType(); }
		inline MessageFragmentType GetMessageFragmentType() const noexcept { return m_Header.GetMessageFragmentType(); }
		const ExtenderUUID& GetExtenderUUID() const noexcept;
		const Buffer& GetMessageData() const noexcept;
		Buffer&& MoveMessageData() noexcept;

		[[nodiscard]] bool Read(BufferView buffer, const Crypto::SymmetricKeyData& symkey);
		[[nodiscard]] bool Write(Buffer& buffer, const Crypto::SymmetricKeyData& symkey);

		static BufferView GetFromBuffer(BufferView& srcbuf) noexcept;

	public:
		static constexpr Size MinMessageDataSizeForCompression{ 128 }; // Bytes

		static constexpr Size MaxMessageDataSize{
			// Reserve space for MessageHeader (21 bytes)
			MessageTransport::MaxMessageDataSize - 21
		};

	private:
		void Initialize(MessageOptions&& msgopt) noexcept;
		void Validate() noexcept;

	private:
		bool m_Valid{ false };

		Header m_Header;
		Buffer m_MessageData;

		bool m_UseCompression{ true };
	};

	using RelayMessageID = UInt16;

	struct RelayDataMessage final
	{
		RelayPort Port{ 0 };
		RelayMessageID ID{ 0 };
		Buffer& Data;

		static constexpr Size HeaderSize{
			sizeof(Port) +
			sizeof(ID) +
			// Size of message data in buffer
			Memory::BufferIO::GetSizeOfEncodedSize(Memory::MaxSize::_2MB)
		};

		static constexpr Size MaxMessageDataSize{
			// Reserve space for relay data header
			Message::MaxMessageDataSize - HeaderSize
		};

		[[nodiscard]] Size GetSize() const noexcept
		{
			return Data.GetSize() + HeaderSize;
		}
	};

	struct RelayDataAckMessage final
	{
		RelayPort Port{ 0 };
		RelayMessageID ID{ 0 };
	};
}