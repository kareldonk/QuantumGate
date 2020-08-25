// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Crypto\Crypto.h"
#include "..\..\Memory\BufferIO.h"

#include <variant>

namespace QuantumGate::Implementation::Core::UDP
{
	using ConnectionID = UInt64;
	using MessageSequenceNumber = UInt16;

	struct ProtocolVersion final
	{
		static constexpr const UInt8 Major{ 0 };
		static constexpr const UInt8 Minor{ 1 };
	};

	class Message final
	{
	public:
		using HMACType = UInt32;

	private:
		class SynHeader final
		{
		public:
			SynHeader() noexcept = default;
			SynHeader(const SynHeader&) = default;
			SynHeader(SynHeader&&) noexcept = default;
			~SynHeader() = default;
			SynHeader& operator=(const SynHeader&) = default;
			SynHeader& operator=(SynHeader&&) noexcept = default;

			void SetMessageSequenceNumber(const MessageSequenceNumber seqnum) noexcept { m_MessageSequenceNumber = seqnum; }
			[[nodiscard]] MessageSequenceNumber GetMessageSequenceNumber() const noexcept { return m_MessageSequenceNumber; }

			void SetMessageAckNumber(const MessageSequenceNumber acknum) noexcept { m_MessageAckNumber = acknum; }
			[[nodiscard]] MessageSequenceNumber GetMessageAckNumber() const noexcept { return m_MessageAckNumber; }

			void SetProtocolVersion(const UInt8 major, const UInt8 minor) noexcept { m_ProtocolVersionMajor = major; m_ProtocolVersionMinor = minor; }
			[[nodiscard]] std::pair<UInt8, UInt8> GetProtocolVersion() const noexcept { return std::make_pair(m_ProtocolVersionMajor, m_ProtocolVersionMinor); }

			void SetConnectionID(const ConnectionID id) noexcept { m_ConnectionID = id; }
			[[nodiscard]] ConnectionID GetConnectionID() const noexcept { return m_ConnectionID; }

			[[nodiscard]] bool Read(const BufferView& buffer) noexcept;
			[[nodiscard]] bool Write(Buffer& buffer) const noexcept;

			static constexpr Size GetSize() noexcept
			{
				return sizeof(m_MessageHMAC) +
					sizeof(m_MessageSequenceNumber) +
					sizeof(m_MessageAckNumber) +
					sizeof(m_ProtocolVersionMajor) +
					sizeof(m_ProtocolVersionMinor) +
					sizeof(m_ConnectionID);
			}

			inline UInt64 GetHMAC() noexcept { return m_MessageHMAC; }

		private:
			HMACType m_MessageHMAC{ 0 };
			MessageSequenceNumber m_MessageSequenceNumber{ 0 };
			MessageSequenceNumber m_MessageAckNumber{ 0 };
			UInt8 m_ProtocolVersionMajor{ 0 };
			UInt8 m_ProtocolVersionMinor{ 0 };
			ConnectionID m_ConnectionID{ 0 };
		};

		class MsgHeader final
		{
			enum class MessageFlags : UInt8
			{
				Syn = 0b00000001,
				Ack = 0b00000010,
				Data = 0b00000100,
				Reset = 0b00001000,
				Null = 0b00010000
			};

		public:
			MsgHeader() noexcept = default;
			MsgHeader(const MsgHeader&) = default;
			MsgHeader(MsgHeader&&) noexcept = default;
			~MsgHeader() = default;
			MsgHeader& operator=(const MsgHeader&) = default;
			MsgHeader& operator=(MsgHeader&&) noexcept = default;

			void SetMessageSequenceNumber(const MessageSequenceNumber seqnum) noexcept { m_MessageSequenceNumber = seqnum; }
			[[nodiscard]] MessageSequenceNumber GetMessageSequenceNumber() const noexcept { return m_MessageSequenceNumber; }

			void SetMessageAckNumber(const MessageSequenceNumber acknum) noexcept { m_MessageAckNumber = acknum; }
			[[nodiscard]] MessageSequenceNumber GetMessageAckNumber() const noexcept { return m_MessageAckNumber; }

			[[nodiscard]] bool IsAck() const noexcept { return (m_MessageFlags & static_cast<UInt8>(MessageFlags::Ack)); }
			void SetAck() noexcept { m_MessageFlags |= static_cast<UInt8>(MessageFlags::Ack); }

			[[nodiscard]] bool IsData() const noexcept { return (m_MessageFlags & static_cast<UInt8>(MessageFlags::Data)); }
			void SetData() noexcept { m_MessageFlags |= static_cast<UInt8>(MessageFlags::Data); }

			[[nodiscard]] bool Read(const BufferView& buffer) noexcept;
			[[nodiscard]] bool Write(Buffer& buffer) const noexcept;

			static constexpr Size GetSize() noexcept
			{
				return sizeof(m_MessageHMAC) +
					sizeof(m_MessageSequenceNumber) +
					sizeof(m_MessageAckNumber) +
					sizeof(m_MessageFlags);
			}

			inline UInt64 GetHMAC() noexcept { return m_MessageHMAC; }

		private:
			HMACType m_MessageHMAC{ 0 };
			MessageSequenceNumber m_MessageSequenceNumber{ 0 };
			MessageSequenceNumber m_MessageAckNumber{ 0 };
			UInt8 m_MessageFlags{ 0 };
		};

		using HeaderType = std::variant<SynHeader, MsgHeader>;

	public:
		enum class Type { Syn, Normal };

		Message(const Type type) noexcept : m_Header(InitHeader(type))
		{
			Validate();
		}

		Message(const Message&) = delete;
		Message(Message&&) noexcept = default;
		~Message() = default;
		Message& operator=(const Message&) = delete;
		Message& operator=(Message&&) noexcept = default;

		[[nodiscard]] inline bool IsValid() const noexcept { return m_Valid; }

		[[nodiscard]] bool IsSyn() const noexcept;
		[[nodiscard]] bool IsNormal() const noexcept;
		[[nodiscard]] bool IsAck() const noexcept;
		[[nodiscard]] bool IsData() const noexcept;

		void SetMessageSequenceNumber(const MessageSequenceNumber seqnum) noexcept;
		[[nodiscard]] MessageSequenceNumber GetMessageSequenceNumber() const noexcept;

		void SetMessageAckNumber(const MessageSequenceNumber acknum) noexcept;
		[[nodiscard]] MessageSequenceNumber GetMessageAckNumber() const noexcept;

		void SetProtocolVersion(const UInt8 major, const UInt8 minor) noexcept;
		[[nodiscard]] std::pair<UInt8, UInt8> GetProtocolVersion() const noexcept;

		void SetConnectionID(const ConnectionID id) noexcept;
		[[nodiscard]] ConnectionID GetConnectionID() const noexcept;

		void SetAckSequenceNumbers(Vector<MessageSequenceNumber>&& acks) noexcept;
		const Vector<MessageSequenceNumber>& GetAckSequenceNumbers() noexcept;

		void SetMessageData(Buffer&& buffer) noexcept;
		const Buffer& GetMessageData() const noexcept;
		Buffer&& MoveMessageData() noexcept;
		inline Size GetMaxMessageDataSize() const noexcept { return (MaxMessageSize - GetHeaderSize()); }

		inline Size GetMaxAckSequenceNumbersPerMessage() const noexcept
		{
			return (MaxMessageSize - (GetHeaderSize() + Memory::BufferIO::GetSizeOfEncodedSize(MaxAckDataSize))) / sizeof(MessageSequenceNumber);
		}

		[[nodiscard]] bool Read(BufferView buffer);
		[[nodiscard]] bool Write(Buffer& buffer);

	public:
		static constexpr Size MaxMessageSize{ 8192 }; // Bytes
		static constexpr Size MaxAckDataSize{ 8192 }; // Bytes

	private:
		inline HeaderType InitHeader(const Type type) const noexcept
		{
			if (type == Type::Syn) return SynHeader();
			return MsgHeader();
		}

		Size GetHeaderSize() const noexcept;

		void Validate() noexcept;

	private:
		bool m_Valid{ false };
		HeaderType m_Header;
		Buffer m_MessageData;
		Vector<MessageSequenceNumber> m_MessageAcks;
	};
}
