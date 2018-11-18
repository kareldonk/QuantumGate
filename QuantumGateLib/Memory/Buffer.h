// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PoolAllocator.h"
#include "ProtectedAllocator.h"
#include "BufferView.h"

#include <cassert> 
#include <vector>

namespace QuantumGate::Implementation::Memory
{
	template<typename A = Allocator<Byte>>
	class BufferImpl
	{
	public:
		using VectorType = std::vector<Byte, A>;
		using SizeType = Size;

		BufferImpl() noexcept {}
		BufferImpl(const BufferImpl& other) { *this += other; }
		BufferImpl(const BufferView& other) { *this += other; }
		BufferImpl(BufferImpl&& other) noexcept { Swap(other.m_Buffer); }
		BufferImpl(VectorType&& buffer) noexcept { Swap(buffer); }
		BufferImpl(const Size size) { Allocate(size); }
		BufferImpl(const Byte* data, const Size data_size) { Add(data, data_size); }
		~BufferImpl() = default;

		inline explicit operator bool() const noexcept { return !IsEmpty(); }

		inline Byte& operator[](Size index) { return m_Buffer[index]; }
		inline const Byte& operator[](Size index) const { return m_Buffer[index]; }

		inline BufferImpl& operator=(const BufferImpl& other)
		{
			// Check for same object
			if (this == &other) return *this;

			Allocate(other.GetSize());
			memcpy(GetBytes(), other.GetBytes(), other.GetSize());

			return *this;
		}

		inline BufferImpl& operator=(const BufferView& buffer)
		{
			Allocate(buffer.GetSize());
			memcpy(GetBytes(), buffer.GetBytes(), buffer.GetSize());

			return *this;
		}

		inline BufferImpl& operator=(const VectorType& buffer)
		{
			return this->operator=(BufferView(buffer.data(), buffer.size()));
		}

		inline BufferImpl& operator=(BufferImpl&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			Clear();
			Swap(other.m_Buffer);
			
			return *this;
		}

		inline BufferImpl& operator=(VectorType&& buffer) noexcept
		{
			// Check for same object
			if (&m_Buffer == &buffer) return *this;

			Clear();
			Swap(buffer);
			
			return *this;
		}

		inline const bool operator==(const BufferImpl& other) const noexcept
		{
			if (GetSize() != other.GetSize()) return false;

			return (memcmp(GetBytes(), other.GetBytes(), GetSize()) == 0);
		}

		inline const bool operator!=(const BufferImpl& other) const noexcept
		{
			return !(*this == other);
		}

		inline operator BufferView() const noexcept { return { GetBytes(), GetSize() }; }

		inline BufferImpl& operator+=(const BufferImpl& other) { Add(other.GetBytes(), other.GetSize()); return *this; }
		inline BufferImpl& operator+=(const BufferView& buffer) { Add(buffer.GetBytes(), buffer.GetSize()); return *this; }
		inline BufferImpl& operator+=(const VectorType& buffer) { Add(buffer.data(), buffer.size()); return *this; }

		inline VectorType& GetVector() noexcept { return m_Buffer; }
		inline const VectorType& GetVector() const noexcept { return m_Buffer; }

		[[nodiscard]] inline Byte* GetBytes() noexcept { return m_Buffer.data(); }
		[[nodiscard]] inline const Byte* GetBytes() const noexcept { return m_Buffer.data(); }
		
		[[nodiscard]] inline Size GetSize() const noexcept { return m_Buffer.size(); }
		
		[[nodiscard]] inline const bool IsEmpty() const noexcept { return m_Buffer.empty(); }
		
		inline void Swap(VectorType& other) noexcept { m_Buffer.swap(other); }
		inline void Swap(BufferImpl& other) noexcept { m_Buffer.swap(other.m_Buffer); }

		inline void Allocate(const Size size) { m_Buffer.resize(size, Byte{ 0 }); }

		inline void Preallocate(const Size size) { m_Buffer.reserve(size); }

		inline void FreeUnused() { m_Buffer.shrink_to_fit(); }

		inline void Clear() noexcept { m_Buffer.clear(); }

		inline void RemoveFirst(const Size num) noexcept
		{
			assert(GetSize() >= num);

			m_Buffer.erase(m_Buffer.begin(), (m_Buffer.size() > num) ? m_Buffer.begin() + num : m_Buffer.end());
		}

		inline void RemoveLast(const Size num) noexcept
		{
			assert(GetSize() >= num);

			m_Buffer.resize((m_Buffer.size() > num) ? m_Buffer.size() - num : 0);
		}

		inline void Resize(const Size new_size)
		{
			if (new_size == GetSize()) return;

			m_Buffer.resize(new_size, Byte{ 0 });
		}

	private:
		inline void Add(const Byte* data, const Size size)
		{
			if (data != nullptr && size > 0)
			{
				m_Buffer.insert(m_Buffer.end(), data, data + size);
			}
		}

	private:
		VectorType m_Buffer;
	};

	using FreeBuffer = BufferImpl<>;
	using Buffer = BufferImpl<PoolAllocator<Byte>>;
	using ProtectedBuffer = BufferImpl<ProtectedAllocator<Byte>>;
}