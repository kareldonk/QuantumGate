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

#pragma pack(push, 1) // Disable padding bytes
		struct AckRange final
		{
			SequenceNumber Begin{ 0 };
			SequenceNumber End{ 0 };
		};
#pragma pack(pop)

	private:
		class Header final
		{
		public:
			Header(const Type type, const Direction direction) noexcept;
			Header(const Header&) = default;
			Header(Header&&) noexcept = default;
			~Header() = default;
			Header& operator=(const Header&) = default;
			Header& operator=(Header&&) noexcept = default;

			[[nodiscard]] inline Direction GetDirection() const noexcept { return m_Direction; }

			[[nodiscard]] inline Type GetMessageType() const noexcept { return m_MessageType; }

			[[nodiscard]] inline bool HasSequenceNumber() const noexcept { return m_SeqNumFlag; }

			inline void SetMessageSequenceNumber(const SequenceNumber seqnum) noexcept
			{
				assert(m_MessageType == Message::Type::Data ||
					   m_MessageType == Message::Type::State ||
					   m_MessageType == Message::Type::Syn);

				m_MessageSequenceNumber = seqnum;
				m_SeqNumFlag = true;
			}
			
			[[nodiscard]] inline SequenceNumber GetMessageSequenceNumber() const noexcept
			{
				assert(m_MessageType == Message::Type::Data ||
					   m_MessageType == Message::Type::State ||
					   m_MessageType == Message::Type::Syn);
				assert(m_SeqNumFlag);

				return m_MessageSequenceNumber;
			}

			[[nodiscard]] inline bool HasAck() const noexcept { return m_AckFlag; }

			inline void SetMessageAckNumber(const SequenceNumber acknum) noexcept { m_MessageAckNumber = acknum; m_AckFlag = true; }
			[[nodiscard]] inline SequenceNumber GetMessageAckNumber() const noexcept { assert(m_AckFlag); return m_MessageAckNumber; }

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

	public:
		struct StateData final
		{
			UInt32 MaxWindowSize{ 0 };
			UInt32 MaxWindowSizeBytes{ 0 };
		};

		struct SynData final
		{
			UInt8 ProtocolVersionMajor{ 0 };
			UInt8 ProtocolVersionMinor{ 0 };
			ConnectionID ConnectionID{ 0 };
			UInt16 Port{ 0 };
		};

		Message(const Type type, const Direction direction, const Size max_size) noexcept :
			m_MaxMessageSize(max_size), m_Header(type, direction)
		{
			InitData(type, direction);
		}

		Message(const Type type, const Direction direction) noexcept :
			m_Header(type, direction)
		{
			InitData(type, direction);
		}

		Message(const Message&) = delete;
		Message(Message&&) noexcept = default;
		~Message() = default;
		Message& operator=(const Message&) = delete;
		Message& operator=(Message&&) noexcept = default;

		[[nodiscard]] inline Type GetType() const noexcept { return m_Header.GetMessageType(); }

		[[nodiscard]] inline Direction GetDirection() const noexcept { return m_Header.GetDirection(); }

		[[nodiscard]] inline bool IsValid() const noexcept { return m_Valid; }

		[[nodiscard]] inline bool HasSequenceNumber() const noexcept { return m_Header.HasSequenceNumber(); }

		inline void SetMessageSequenceNumber(const SequenceNumber seqnum) noexcept { m_Header.SetMessageSequenceNumber(seqnum); }
		[[nodiscard]] inline SequenceNumber GetMessageSequenceNumber() const noexcept { return m_Header.GetMessageSequenceNumber(); }

		[[nodiscard]] inline bool HasAck() const noexcept { return m_Header.HasAck(); }

		inline void SetMessageAckNumber(const SequenceNumber acknum) noexcept { m_Header.SetMessageAckNumber(acknum); }
		[[nodiscard]] inline SequenceNumber GetMessageAckNumber() const noexcept { return m_Header.GetMessageAckNumber(); }

		void SetSynData(SynData&& data) noexcept;
		[[nodiscard]] const SynData& GetSynData() const noexcept;

		void SetStateData(StateData&& data) noexcept;
		[[nodiscard]] const StateData& GetStateData() const noexcept;

		[[nodiscard]] Size GetMaxAckRangesPerMessage() const noexcept;
		void SetAckRanges(Vector<AckRange>&& acks) noexcept;
		[[nodiscard]] const Vector<AckRange>& GetAckRanges() noexcept;

		[[nodiscard]] Size GetMaxMessageDataSize() const noexcept;
		void SetMessageData(Buffer&& buffer) noexcept;
		[[nodiscard]] const Buffer& GetMessageData() const noexcept;
		[[nodiscard]] Buffer&& MoveMessageData() noexcept;

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
		void InitData(const Type type, const Direction direction) noexcept
		{
			if (direction == Direction::Incoming) return;

			switch (type)
			{
				case Type::Syn:
					m_Data = SynData();
					break;
				case Type::State:
					m_Data = StateData();
					break;
				case Type::Data:
				case Type::MTUD:
				case Type::Null:
					m_Data = Buffer();
					break;
				case Type::EAck:
					m_Data = Vector<AckRange>();
					break;
				default:
					break;
			}
		}

		Size GetHeaderSize() const noexcept;

		void Validate() noexcept;

	private:
		using Data = std::variant<std::monostate, SynData, StateData, Buffer, Vector<AckRange>>;

		const Size m_MaxMessageSize{ 0 };
		bool m_Valid{ false };
		Header m_Header;
		Data m_Data;
	};
}
