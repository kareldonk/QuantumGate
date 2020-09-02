// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Crypto\Crypto.h"
#include "..\..\Memory\BufferIO.h"

#include <variant>

namespace QuantumGate::Implementation::Core::UDP
{
	using ConnectionID = UInt64;

	struct ProtocolVersion final
	{
		static constexpr const UInt8 Major{ 0 };
		static constexpr const UInt8 Minor{ 1 };
	};

	class Message final
	{
	public:
		enum class Type : UInt8
		{
			Unknown = 0,
			Syn = 5,
			Data = 10,
			DataAck = 15,
			MTUD = 25,
			MTUDAck = 30,
			Reset = 35,
			Null = 40
		};

		enum class Direction : UInt8 { Unknown, Incoming, Outgoing };

		using SequenceNumber = UInt16;
		using HMAC = UInt32;

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

			void SetMessageSequenceNumber(const SequenceNumber seqnum) noexcept { m_MessageSequenceNumber = seqnum; }
			[[nodiscard]] SequenceNumber GetMessageSequenceNumber() const noexcept { return m_MessageSequenceNumber; }

			void SetMessageAckNumber(const SequenceNumber acknum) noexcept { m_MessageAckNumber = acknum; }
			[[nodiscard]] SequenceNumber GetMessageAckNumber() const noexcept { return m_MessageAckNumber; }

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
			HMAC m_MessageHMAC{ 0 };
			SequenceNumber m_MessageSequenceNumber{ 0 };
			SequenceNumber m_MessageAckNumber{ 0 };
			UInt8 m_ProtocolVersionMajor{ 0 };
			UInt8 m_ProtocolVersionMinor{ 0 };
			ConnectionID m_ConnectionID{ 0 };
		};

		class MsgHeader final
		{
		public:
			MsgHeader(const Type type) noexcept : m_MessageType(type) {}
			MsgHeader(const MsgHeader&) = default;
			MsgHeader(MsgHeader&&) noexcept = default;
			~MsgHeader() = default;
			MsgHeader& operator=(const MsgHeader&) = default;
			MsgHeader& operator=(MsgHeader&&) noexcept = default;

			[[nodiscard]] inline Type GetMessageType() const noexcept { return m_MessageType; }
			void SetMessageSequenceNumber(const SequenceNumber seqnum) noexcept { m_MessageSequenceNumber = seqnum; }
			[[nodiscard]] SequenceNumber GetMessageSequenceNumber() const noexcept { return m_MessageSequenceNumber; }

			void SetMessageAckNumber(const SequenceNumber acknum) noexcept { m_MessageAckNumber = acknum; }
			[[nodiscard]] SequenceNumber GetMessageAckNumber() const noexcept { return m_MessageAckNumber; }

			[[nodiscard]] bool Read(const BufferView& buffer) noexcept;
			[[nodiscard]] bool Write(Buffer& buffer) const noexcept;

			static constexpr Size GetSize() noexcept
			{
				return sizeof(m_MessageHMAC) +
					sizeof(m_MessageSequenceNumber) +
					sizeof(m_MessageAckNumber) +
					sizeof(m_MessageType);
			}

			inline UInt64 GetHMAC() noexcept { return m_MessageHMAC; }

		private:
			HMAC m_MessageHMAC{ 0 };
			SequenceNumber m_MessageSequenceNumber{ 0 };
			SequenceNumber m_MessageAckNumber{ 0 };
			Type m_MessageType{ 0 };
		};

		using HeaderType = std::variant<SynHeader, MsgHeader>;

	public:
		Message(const Type type, const Direction direction, const Size max_size) noexcept :
			m_Direction(direction), m_MaxMessageSize(max_size), m_Header(InitHeader(type))
		{
			Validate();
		}

		Message(const Type type, const Direction direction) noexcept :
			m_Direction(direction), m_Header(InitHeader(type))
		{
			Validate();
		}

		Message(const Message&) = delete;
		Message(Message&&) noexcept = default;
		~Message() = default;
		Message& operator=(const Message&) = delete;
		Message& operator=(Message&&) noexcept = default;

		[[nodiscard]] inline Type GetType() const noexcept
		{
			if (std::holds_alternative<SynHeader>(m_Header)) return Type::Syn;
			else return std::get<MsgHeader>(m_Header).GetMessageType();
		}

		[[nodiscard]] inline bool IsValid() const noexcept { return m_Valid; }

		void SetMessageSequenceNumber(const SequenceNumber seqnum) noexcept;
		[[nodiscard]] SequenceNumber GetMessageSequenceNumber() const noexcept;

		void SetMessageAckNumber(const SequenceNumber acknum) noexcept;
		[[nodiscard]] SequenceNumber GetMessageAckNumber() const noexcept;

		void SetProtocolVersion(const UInt8 major, const UInt8 minor) noexcept;
		[[nodiscard]] std::pair<UInt8, UInt8> GetProtocolVersion() const noexcept;

		void SetConnectionID(const ConnectionID id) noexcept;
		[[nodiscard]] ConnectionID GetConnectionID() const noexcept;

		void SetAckSequenceNumbers(Vector<SequenceNumber>&& acks) noexcept;
		const Vector<SequenceNumber>& GetAckSequenceNumbers() noexcept;

		void SetMessageData(Buffer&& buffer) noexcept;
		const Buffer& GetMessageData() const noexcept;
		Buffer&& MoveMessageData() noexcept;

		Size GetMaxMessageDataSize() const noexcept;
		Size GetMaxAckSequenceNumbersPerMessage() const noexcept;

		[[nodiscard]] bool Read(BufferView buffer);
		[[nodiscard]] bool Write(Buffer& buffer);

	private:
		inline HeaderType InitHeader(const Type type) const noexcept
		{
			if (type == Type::Syn) return SynHeader();
			else return MsgHeader(type);
		}

		Size GetHeaderSize() const noexcept;

		void Validate() noexcept;

	private:
		const Direction m_Direction{ Direction::Unknown };
		const Size m_MaxMessageSize{ 0 };
		bool m_Valid{ false };
		HeaderType m_Header;
		Buffer m_MessageData;
		Vector<SequenceNumber> m_MessageAcks;
	};
}
