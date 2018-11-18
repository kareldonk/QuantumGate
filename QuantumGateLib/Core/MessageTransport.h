// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Crypto\Crypto.h"

namespace QuantumGate::Implementation::Core
{
	enum class MessageTransportCheck
	{
		Unknown, NotEnoughData, TooMuchData, CompleteMessage
	};

	class MessageTransport final
	{
	public :
		struct DataSizeSettings final
		{
			UInt8 Offset{ 9 };
			UInt32 XOR{ 0 };
		};

	private:
		class OHeader final
		{
		public:
			OHeader(const DataSizeSettings mds_settings) noexcept :
				m_MessageDataSizeSettings(mds_settings)
			{}

			OHeader(const OHeader&) = default;
			OHeader(OHeader&&) = default;
			~OHeader() = default;
			OHeader& operator=(const OHeader&) = default;
			OHeader& operator=(OHeader&&) = default;

			void Initialize() noexcept;

			[[nodiscard]] const bool Read(const BufferView& buffer);
			[[nodiscard]] const bool Write(Buffer& buffer) const noexcept;

			static constexpr Size GetSize() noexcept
			{
				return 4 + // 4 bytes for random bits and m_MessageDataSize combined
					sizeof(m_MessageNonceSeed) +
					OHeader::MessageHMACSize;
			}

			inline Buffer& GetHMACBuffer() noexcept { return m_MessageHMAC; }
			inline void SetMessageDataSize(const Size size) noexcept { m_MessageDataSize = static_cast<UInt32>(size); }
			inline const Size GetMessageDataSize() const noexcept { return m_MessageDataSize; }
			inline const void SetMessageNonceSeed(UInt32 seed) noexcept { m_MessageNonceSeed = seed; }
			inline const UInt32 GetMessageNonceSeed() const noexcept { return m_MessageNonceSeed; }


			static UInt32 ObfuscateMessageDataSize(const DataSizeSettings mds_settings, const UInt32 rnd_bits,
												   UInt32 size) noexcept;
			static UInt32 DeObfuscateMessageDataSize(const DataSizeSettings mds_settings, UInt32 size) noexcept;

		public:
			static constexpr Size MessageHMACSize{ 32 };

		private:
			DataSizeSettings m_MessageDataSizeSettings;
			UInt32 m_MessageRandomBits{ 0 };
			UInt32 m_MessageDataSize{ 0 };
			UInt32 m_MessageNonceSeed{ 0 };
			Buffer m_MessageHMAC;
		};

		class IHeader final
		{
		public:
			IHeader() noexcept {}
			IHeader(const IHeader&) = default;
			IHeader(IHeader&&) = default;
			~IHeader() = default;
			IHeader& operator=(const IHeader&) = default;
			IHeader& operator=(IHeader&&) = default;

			void Initialize() noexcept;

			[[nodiscard]] const bool Read(const BufferView& buffer);
			[[nodiscard]] const bool Write(Buffer& buffer) const;

			static constexpr Size GetSize() noexcept
			{
				return sizeof(m_MessageCounter) +
					sizeof(m_MessageTime) +
					sizeof(m_NextRandomDataPrefixLength) +
					sizeof(m_RandomDataSize);
			}

			inline void SetMessageCounter(const UInt8 counter) noexcept { m_MessageCounter = counter; }
			inline const UInt8 GetMessageCounter() const noexcept { return m_MessageCounter; }

			void SetRandomDataSize(const Size minrndsize, const Size maxrndsize) noexcept;
			inline const UInt16 GetRandomDataSize() const noexcept { return m_RandomDataSize; }

			void SetRandomDataPrefixLength(const UInt16 len) noexcept { m_NextRandomDataPrefixLength = len; }
			UInt16 GetRandomDataPrefixLength() const noexcept { return m_NextRandomDataPrefixLength; }

			const SystemTime GetMessageTime() const noexcept;

		private:
			UInt8 m_MessageCounter{ 0 };
			UInt64 m_MessageTime{ 0 };
			UInt16 m_NextRandomDataPrefixLength{ 0 };
			UInt16 m_RandomDataSize{ 0 };
		};

	public:
		MessageTransport(const DataSizeSettings mds_settings, const Settings& settings) noexcept;
		MessageTransport(const MessageTransport&) = delete;
		MessageTransport(MessageTransport&&) = default;
		~MessageTransport() = default;
		MessageTransport& operator=(const MessageTransport&) = delete;
		MessageTransport& operator=(MessageTransport&&) = default;

		[[nodiscard]] inline const bool IsValid() const noexcept { return m_Valid; }

		inline void SetMessageCounter(const UInt8 counter) noexcept { m_IHeader.SetMessageCounter(counter); }
		inline const UInt8 GetMessageCounter() const noexcept { return m_IHeader.GetMessageCounter(); }
		inline const void SetMessageNonceSeed(UInt32 seed) noexcept { m_OHeader.SetMessageNonceSeed(seed); }
		inline const UInt32 GetMessageNonceSeed() const noexcept { return m_OHeader.GetMessageNonceSeed(); }

		void SetMessageData(Buffer&& buffer) noexcept;
		const Buffer& GetMessageData() const noexcept;

		inline void SetCurrentRandomDataPrefixLength(const UInt16 len) noexcept { m_RandomDataPrefixLength = len; }
		inline void SetNextRandomDataPrefixLength(const UInt16 len) noexcept { m_IHeader.SetRandomDataPrefixLength(len); }
		inline UInt16 GetNextRandomDataPrefixLength() const noexcept { return m_IHeader.GetRandomDataPrefixLength(); }

		const SystemTime GetMessageTime() const noexcept;

		[[nodiscard]] const std::pair<bool, bool> Read(BufferView buffer, Crypto::SymmetricKeyData& symkey,
													   const BufferView& nonce);

		[[nodiscard]] const bool Write(Buffer& buffer, Crypto::SymmetricKeyData& symkey, const BufferView& nonce);

		static const MessageTransportCheck Peek(const UInt16 rndp_len, const DataSizeSettings mds_settings,
												const Buffer& srcbuf) noexcept;

		static const MessageTransportCheck GetFromBuffer(const UInt16 rndp_len, const DataSizeSettings mds_settings,
														 Buffer& srcbuf, Buffer& destbuf);

		static std::optional<UInt32> GetNonceSeedFromBuffer(const BufferView& srcbuf) noexcept;

	public:
		static constexpr Size MaxMessageDataSizeOffset{ 12 };

		static constexpr Size MaxMessageSize{ 1'048'576 };				// 2^20 Bytes
		static constexpr Size MaxMessageDataSize{ 1'048'021 };			// Bytes
		static constexpr Size MaxMessageAndRandomDataSize{ 1'048'085 };	// Bytes (64 extra bytes for random data in case message data is maxed out)

		/*
		static constexpr Size MaxMessageSize{ 2'097'152 };				// 2^21 Bytes
		static constexpr Size MaxMessageDataSize{ 2'097'000 };			// Bytes
		static constexpr Size MaxMessageAndRandomDataSize{ 2'097'064 };	// Bytes (64 extra bytes for random data in case message data is maxed out)
		*/

	private:
		void Validate() noexcept;

	private:
		bool m_Valid{ false };

		const Settings& m_Settings;

		OHeader m_OHeader;
		IHeader m_IHeader;
		Buffer m_MessageData;
		UInt16 m_RandomDataPrefixLength{ 0 };
	};
}
