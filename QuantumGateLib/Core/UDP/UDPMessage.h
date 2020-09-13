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
			Syn = 1,
			State = 2,
			Data = 3,
			EAck = 4,
			MTUD = 5,
			Reset = 6,
			Null = 7
		};

		enum class Direction : UInt8 { Unknown, Incoming, Outgoing };

		using SequenceNumber = UInt16;
		using HMAC = UInt32;

	private:
		class SynHeader final
		{
		public:
			SynHeader(const Direction direction) noexcept;
			SynHeader(const SynHeader&) = default;
			SynHeader(SynHeader&&) noexcept = default;
			~SynHeader() = default;
			SynHeader& operator=(const SynHeader&) = default;
			SynHeader& operator=(SynHeader&&) noexcept = default;

			[[nodiscard]] bool HasSequenceNumber() const noexcept { return true; }

			void SetMessageSequenceNumber(const SequenceNumber seqnum) noexcept { m_MessageSequenceNumber = seqnum; }
			[[nodiscard]] SequenceNumber GetMessageSequenceNumber() const noexcept { return m_MessageSequenceNumber; }

			[[nodiscard]] bool HasAck() const noexcept { return (m_Direction == Direction::Incoming) ? true : false;; }

			void SetMessageAckNumber(const SequenceNumber acknum) noexcept { m_MessageAckNumber = acknum; }
			[[nodiscard]] SequenceNumber GetMessageAckNumber() const noexcept { return m_MessageAckNumber; }

			void SetProtocolVersion(const UInt8 major, const UInt8 minor) noexcept { m_ProtocolVersionMajor = major; m_ProtocolVersionMinor = minor; }
			[[nodiscard]] std::pair<UInt8, UInt8> GetProtocolVersion() const noexcept { return std::make_pair(m_ProtocolVersionMajor, m_ProtocolVersionMinor); }

			void SetConnectionID(const ConnectionID id) noexcept { m_ConnectionID = id; }
			[[nodiscard]] ConnectionID GetConnectionID() const noexcept { return m_ConnectionID; }

			void SetPort(const UInt16 port) noexcept { m_Port = port; }
			[[nodiscard]] UInt16 GetPort() const noexcept { return m_Port; }

			[[nodiscard]] bool Read(const BufferView& buffer) noexcept;
			[[nodiscard]] bool Write(Buffer& buffer) const noexcept;

			static constexpr Size GetSize() noexcept
			{
				return sizeof(m_MessageHMAC) +
					sizeof(m_MessageSequenceNumber) +
					sizeof(m_MessageAckNumber) +
					sizeof(m_ProtocolVersionMajor) +
					sizeof(m_ProtocolVersionMinor) +
					sizeof(m_ConnectionID) +
					sizeof(m_Port);
			}

			inline UInt64 GetHMAC() noexcept { return m_MessageHMAC; }

		private:
			const Direction m_Direction{ Direction::Unknown };
			HMAC m_MessageHMAC{ 0 };
			SequenceNumber m_MessageSequenceNumber{ 0 };
			SequenceNumber m_MessageAckNumber{ 0 };
			UInt8 m_ProtocolVersionMajor{ 0 };
			UInt8 m_ProtocolVersionMinor{ 0 };
			ConnectionID m_ConnectionID{ 0 };
			UInt16 m_Port{ 0 };
		};

		class MsgHeader final
		{
		public:
			MsgHeader(const Type type, const Direction direction) noexcept;
			MsgHeader(const MsgHeader&) = default;
			MsgHeader(MsgHeader&&) noexcept = default;
			~MsgHeader() = default;
			MsgHeader& operator=(const MsgHeader&) = default;
			MsgHeader& operator=(MsgHeader&&) noexcept = default;

			[[nodiscard]] inline Type GetMessageType() const noexcept { return m_MessageType; }

			[[nodiscard]] bool HasSequenceNumber() const noexcept { return m_SeqNumFlag; }

			void SetMessageSequenceNumber(const SequenceNumber seqnum) noexcept
			{
				assert(m_MessageType != Message::Type::EAck &&
					   m_MessageType != Message::Type::Reset);

				m_MessageSequenceNumber = seqnum;
				m_SeqNumFlag = true;
			}
			
			[[nodiscard]] SequenceNumber GetMessageSequenceNumber() const noexcept
			{
				assert(m_MessageType != Message::Type::EAck &&
					   m_MessageType != Message::Type::Reset);
				assert(m_SeqNumFlag);

				return m_MessageSequenceNumber;
			}

			[[nodiscard]] bool HasAck() const noexcept { return m_AckFlag; }

			void SetMessageAckNumber(const SequenceNumber acknum) noexcept { m_MessageAckNumber = acknum; m_AckFlag = true; }
			[[nodiscard]] SequenceNumber GetMessageAckNumber() const noexcept { assert(m_AckFlag); return m_MessageAckNumber; }

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
			static constexpr UInt8 MessageTypeMask{ 0b00001111 };
			static constexpr UInt8 AckFlag{ 0b10000000 };
			static constexpr UInt8 SeqNumFlag{ 0b01000000 };

		private:
			const Direction m_Direction{ Direction::Unknown };
			HMAC m_MessageHMAC{ 0 };
			SequenceNumber m_MessageSequenceNumber{ 0 };
			SequenceNumber m_MessageAckNumber{ 0 };
			Type m_MessageType{ 0 };
			bool m_AckFlag{ false };
			bool m_SeqNumFlag{ false };
		};

		using HeaderType = std::variant<SynHeader, MsgHeader>;

	public:
		struct StateData final
		{
			UInt32 MaxWindowSize{ 0 };
			UInt32 MaxWindowSizeBytes{ 0 };
		};

		Message(const Type type, const Direction direction, const Size max_size) noexcept :
			m_MaxMessageSize(max_size), m_Header(InitializeHeader(type, direction))
		{
			Validate();
		}

		Message(const Type type, const Direction direction) noexcept :
			m_Header(InitializeHeader(type, direction))
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

		[[nodiscard]] bool HasSequenceNumber() const noexcept;

		void SetMessageSequenceNumber(const SequenceNumber seqnum) noexcept;
		[[nodiscard]] SequenceNumber GetMessageSequenceNumber() const noexcept;

		[[nodiscard]] bool HasAck() const noexcept;

		void SetMessageAckNumber(const SequenceNumber acknum) noexcept;
		[[nodiscard]] SequenceNumber GetMessageAckNumber() const noexcept;

		void SetProtocolVersion(const UInt8 major, const UInt8 minor) noexcept;
		[[nodiscard]] std::pair<UInt8, UInt8> GetProtocolVersion() const noexcept;

		void SetConnectionID(const ConnectionID id) noexcept;
		[[nodiscard]] ConnectionID GetConnectionID() const noexcept;

		void SetPort(const UInt16 port) noexcept;
		[[nodiscard]] UInt16 GetPort() const noexcept;

		void SetStateData(StateData&& data) noexcept;
		const StateData& GetStateData() const noexcept;

		void SetAckSequenceNumbers(Vector<SequenceNumber>&& acks) noexcept;
		const Vector<SequenceNumber>& GetAckSequenceNumbers() noexcept;

		void SetMessageData(Buffer&& buffer) noexcept;
		const Buffer& GetMessageData() const noexcept;
		Buffer&& MoveMessageData() noexcept;

		Size GetMaxMessageDataSize() const noexcept;
		Size GetMaxAckSequenceNumbersPerMessage() const noexcept;

		[[nodiscard]] bool Read(BufferView buffer);
		[[nodiscard]] bool Write(Buffer& buffer);

		static SequenceNumber GetNextSequenceNumber(const SequenceNumber current) noexcept
		{
			if (current == std::numeric_limits<SequenceNumber>::max())
			{
				return 0;
			}
			else return current + 1;
		}

		static SequenceNumber GetPreviousSequenceNumber(const SequenceNumber current) noexcept
		{
			if (current == 0)
			{
				return std::numeric_limits<SequenceNumber>::max();
			}
			else return current - 1;
		}

	private:
		inline HeaderType InitializeHeader(const Type type, const Direction direction) const noexcept
		{
			if (type == Type::Syn) return SynHeader(direction);
			else return MsgHeader(type, direction);
		}

		Size GetHeaderSize() const noexcept;

		void Validate() noexcept;

	private:
		const Size m_MaxMessageSize{ 0 };
		bool m_Valid{ false };
		HeaderType m_Header;
		StateData m_StateData;
		Buffer m_Data;
		Vector<SequenceNumber> m_EAcks;
	};
}
