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

		constexpr BufferImpl() noexcept {}
		constexpr BufferImpl(const BufferImpl& other) { *this += other; }
		constexpr BufferImpl(const BufferView& other) { *this += other; }
		constexpr BufferImpl(BufferImpl&& other) noexcept { Swap(other.m_Buffer); }
		constexpr BufferImpl(VectorType&& buffer) noexcept { Swap(buffer); }
		constexpr BufferImpl(const Size size) { Allocate(size); }
		constexpr BufferImpl(const Byte* data, const Size data_size) { Add(data, data_size); }
		virtual ~BufferImpl() { Clear(); }

		constexpr explicit operator bool() const noexcept { return !IsEmpty(); }

		constexpr Byte& operator[](Size index) noexcept { return m_Buffer[index]; }
		constexpr const Byte& operator[](Size index) const noexcept { return m_Buffer[index]; }

		constexpr BufferImpl& operator=(const BufferImpl& other)
		{
			// Check for same object
			if (this == &other) return *this;

			Allocate(other.GetSize());
			memcpy(GetBytes(), other.GetBytes(), other.GetSize());

			return *this;
		}

		constexpr BufferImpl& operator=(const BufferView& buffer)
		{
			Allocate(buffer.GetSize());
			memcpy(GetBytes(), buffer.GetBytes(), buffer.GetSize());

			return *this;
		}

		constexpr BufferImpl& operator=(const VectorType& buffer)
		{
			return this->operator=(BufferView(buffer.data(), buffer.size()));
		}

		constexpr BufferImpl& operator=(BufferImpl&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			Clear();
			Swap(other.m_Buffer);
			
			return *this;
		}

		constexpr BufferImpl& operator=(VectorType&& buffer) noexcept
		{
			Clear();
			Swap(buffer);
			
			return *this;
		}

		constexpr const bool operator==(const BufferImpl& other) const noexcept
		{
			if (GetSize() != other.GetSize()) return false;

			return (memcmp(GetBytes(), other.GetBytes(), GetSize()) == 0);
		}

		constexpr const bool operator!=(const BufferImpl& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr operator BufferView() const noexcept { return { GetBytes(), GetSize() }; }

		constexpr BufferImpl& operator+=(const BufferImpl& other) { Add(other.GetBytes(), other.GetSize()); return *this; }
		constexpr BufferImpl& operator+=(const BufferView& buffer) { Add(buffer.GetBytes(), buffer.GetSize()); return *this; }
		constexpr BufferImpl& operator+=(const VectorType& buffer) { Add(buffer.data(), buffer.size()); return *this; }

		constexpr VectorType& GetVector() noexcept { return m_Buffer; }
		constexpr const VectorType& GetVector() const noexcept { return m_Buffer; }

		[[nodiscard]] constexpr Byte* GetBytes() noexcept { return m_Buffer.data(); }
		[[nodiscard]] constexpr const Byte* GetBytes() const noexcept { return m_Buffer.data(); }
		
		[[nodiscard]] constexpr Size GetSize() const noexcept { return m_Buffer.size(); }
		
		[[nodiscard]] constexpr const bool IsEmpty() const noexcept { return m_Buffer.empty(); }
		
		constexpr void Swap(VectorType& other) noexcept { m_Buffer.swap(other); }
		constexpr void Swap(BufferImpl& other) noexcept { m_Buffer.swap(other.m_Buffer); }

		constexpr void Allocate(const Size size) { m_Buffer.resize(size, Byte{ 0 }); }

		constexpr void Preallocate(const Size size) { m_Buffer.reserve(size); }

		constexpr void FreeUnused() { m_Buffer.shrink_to_fit(); }

		constexpr void Clear() noexcept { m_Buffer.clear(); }

		constexpr void RemoveFirst(const Size num) noexcept
		{
			assert(GetSize() >= num);

			m_Buffer.erase(m_Buffer.begin(), (m_Buffer.size() > num) ? m_Buffer.begin() + num : m_Buffer.end());
		}

		constexpr void RemoveLast(const Size num) noexcept
		{
			assert(GetSize() >= num);

			m_Buffer.resize((m_Buffer.size() > num) ? m_Buffer.size() - num : 0);
		}

		constexpr void Resize(const Size new_size)
		{
			if (new_size == GetSize()) return;

			m_Buffer.resize(new_size, Byte{ 0 });
		}

	private:
		constexpr void Add(const Byte* data, const Size size)
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