// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Concurrency\ThreadSafe.h"
#include "..\..\QuantumGateCryptoLib\QuantumGateCryptoLib.h"

#include <atomic>

namespace QuantumGate::Implementation
{
	class Hash final
	{
	private:
		Hash() = default;

		static void InitNonPersistentKey() noexcept;

	public:
		inline static UInt64 GetNonPersistentHash(const UInt64 val) noexcept
		{
			return GetNonPersistentHash(BufferView(reinterpret_cast<const Byte*>(&val), sizeof(val)));
		}

		inline static UInt64 GetNonPersistentHash(const String& txt) noexcept
		{
			return GetNonPersistentHash(BufferView(reinterpret_cast<const Byte*>(txt.data()),
												   txt.size() * sizeof(String::value_type)));
		}

		inline static UInt64 GetNonPersistentHash(const BufferView& buffer) noexcept
		{
			if (!m_NonPersistentKeyInit) InitNonPersistentKey();

			UInt64 hash{ 0 };

			m_NonPersistentKey.WithSharedLock([&](auto& key)
			{
				hash = GetHash(buffer, BufferView(key));
			});

			return hash;
		}

		inline static UInt64 GetPersistentHash(const UInt64 val) noexcept
		{
			return GetPersistentHash(BufferView(reinterpret_cast<const Byte*>(&val), sizeof(val)));
		}

		inline static UInt64 GetPersistentHash(const String& txt) noexcept
		{
			return GetPersistentHash(BufferView(reinterpret_cast<const Byte*>(txt.data()),
												txt.size() * sizeof(String::value_type)));
		}

		inline static UInt64 GetPersistentHash(const BufferView& buffer) noexcept
		{
			return GetHash(buffer, BufferView(reinterpret_cast<const Byte*>(&m_PersistentKey), sizeof(m_PersistentKey)));
		}

		inline static UInt64 GetHash(const BufferView& buffer, const BufferView& key) noexcept
		{
			assert(key.GetSize() == m_KeySize);

			UInt64 hash{ 0 };

			siphash(reinterpret_cast<const uint8_t*>(buffer.GetBytes()), buffer.GetSize(),
					reinterpret_cast<const uint8_t*>(key.GetBytes()),
					reinterpret_cast<uint8_t*>(&hash), 8);

			return hash;
		}

	private:
		static constexpr UInt m_KeySize{ 16 };

		static constexpr UInt8 m_PersistentKey[m_KeySize]{
			33, 66, 99, 33, 66, 99, 33, 66, 99,
			33, 66, 99, 33, 66, 99, 33
		};

		static std::atomic_bool m_NonPersistentKeyInit;
		static Concurrency::ThreadSafe<Memory::FreeBuffer, std::shared_mutex> m_NonPersistentKey;
	};
}