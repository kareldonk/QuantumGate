// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "MessageTransport.h"
#include "MessageDetails.h"

namespace QuantumGate::Implementation::Core
{
	enum class MessageFragmentType
	{
		Unknown, Complete, PartialBegin, Partial, PartialEnd
	};

	enum class MessageFlag : UInt8
	{
		PartialBegin =	0b00000001,
		Partial =		0b00000010,
		PartialEnd =	0b00000100,
		Compressed =	0b00001000,
	};

	struct MessageOptions
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
		inline static ExtenderUUID DefaultExtenderUUID{ L"00000000-0000-0900-0600-000000000000" };

	public:
		MessageType MessageType{ MessageType::Unknown };
		ExtenderUUID ExtenderUUID;
		Buffer MessageData;
		bool UseCompression{ true };
		MessageFragmentType Fragment{ MessageFragmentType::Complete };
	};

	class Message
	{
		class Header
		{
		public:
			Header() noexcept {}
			Header(const Header&) = default;
			Header(Header&&) = default;
			virtual ~Header() = default;
			Header& operator=(const Header&) = default;
			Header& operator=(Header&&) = default;

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

			[[nodiscard]] const bool Read(const BufferView& buffer);
			[[nodiscard]] const bool Write(Buffer& buffer) const;

			[[nodiscard]] inline const bool IsCompressed() const noexcept
			{
				return (m_MessageFlags & static_cast<UInt8>(MessageFlag::Compressed));
			}

			inline void SetMessageFlag(const MessageFlag flag, const bool state) noexcept
			{
				if (state) m_MessageFlags |= static_cast<UInt8>(flag);
				else m_MessageFlags &= ~static_cast<UInt8>(flag);
			}

			inline void SetMessageDataSize(const Size size) noexcept { m_MessageDataSize = static_cast<UInt32>(size); }
			inline const Size GetMessageDataSize() const noexcept { return m_MessageDataSize; }

			inline const MessageType GetMessageType() const noexcept { return m_MessageType; }
			
			inline const MessageFragmentType GetMessageFragmentType() const noexcept
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
		Message(const MessageOptions& msgopt) noexcept;
		Message(const Message&) = delete;
		Message(Message&&) = default;
		virtual ~Message() = default;
		Message& operator=(const Message&) = delete;
		Message& operator=(Message&&) = default;

		[[nodiscard]] inline const bool IsValid() const noexcept { return m_Valid; }

		inline const MessageType GetMessageType() const noexcept { return m_Header.GetMessageType(); }
		inline const MessageFragmentType GetMessageFragmentType() const noexcept { return m_Header.GetMessageFragmentType(); }
		const ExtenderUUID& GetExtenderUUID() const noexcept;
		const Buffer& GetMessageData() const noexcept;
		Buffer&& MoveMessageData() noexcept;

		[[nodiscard]] const bool Read(BufferView buffer, const Crypto::SymmetricKeyData& symkey);
		[[nodiscard]] const bool Write(Buffer& buffer, const Crypto::SymmetricKeyData& symkey);

		static const BufferView GetFromBuffer(BufferView& srcbuf);

	public:
		static constexpr Size MinMessageDataSizeForCompression{ 128 }; // Bytes

		static constexpr Size MaxMessageDataSize{
			// Reserve space for MessageHeader (21 bytes)
			MessageTransport::MaxMessageDataSize - 21
		};

	private:
		void Initialize(const MessageOptions& msgopt) noexcept;
		void Validate() noexcept;

	private:
		bool m_Valid{ false };

		Header m_Header;
		Buffer m_MessageData;

		bool m_UseCompression{ true };
	};
}