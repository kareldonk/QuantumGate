// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Buffer.h"

namespace QuantumGate::Implementation::Memory
{
	template<typename T>
	concept RingBufferTypeRequirements = requires(T b)
	{
		b.GetBytes();
		b.GetSize();
	};

	template<typename B = FreeBuffer> requires RingBufferTypeRequirements<B>
	class RingBufferImpl
	{
	public:
		using BufferType = B;
		using SizeType = Size;

		RingBufferImpl() noexcept {}

		RingBufferImpl(const Size size) :
			m_Buffer(size),
			m_WriteSpace(size)
		{}

		RingBufferImpl(const Byte* data, const Size data_size) :
			m_Buffer(data, data_size)
		{}

		RingBufferImpl(const BufferView& other) :
			m_Buffer(other)
		{}

		~RingBufferImpl() = default;

		RingBufferImpl(const RingBufferImpl& other) :
			m_Buffer(other.m_Buffer),
			m_ReadOffset(other.m_ReadOffset),
			m_WriteOffset(other.m_WriteOffset),
			m_WriteSpace(other.m_WriteSpace)
		{}

		RingBufferImpl(RingBufferImpl&& other) noexcept :
			m_Buffer(std::move(other.m_Buffer)),
			m_ReadOffset(std::exchange(other.m_ReadOffset, 0)),
			m_WriteOffset(std::exchange(other.m_WriteSpace, 0)),
			m_WriteSpace(std::exchange(other.m_WriteSpace, 0))
		{}

		template<typename T> requires RingBufferTypeRequirements<T>
		[[nodiscard]] inline Size Read(T& in_data) noexcept
		{
			return Read(in_data.GetBytes(), in_data.GetSize());
		}

		[[nodiscard]] Size Read(Byte* out_data, Size out_data_size) noexcept
		{
			const auto read_size = GetReadSize();
			if (read_size == 0 || out_data == nullptr || out_data_size == 0)
			{
				return 0;
			}

			// Can't read more than available
			if (out_data_size > read_size)
			{
				out_data_size = read_size;
			}

			const auto size = GetSize();
			const auto len = size - m_ReadOffset;

			if (out_data_size > len)
			{
				// Wrap around
				std::memcpy(out_data, m_Buffer.GetBytes() + m_ReadOffset, len);
				std::memcpy(out_data + len, m_Buffer.GetBytes(), out_data_size - len);
			}
			else
			{
				std::memcpy(out_data, m_Buffer.GetBytes() + m_ReadOffset, out_data_size);
			}

			m_ReadOffset = (m_ReadOffset + out_data_size) % size;
			m_WriteSpace += out_data_size;

			return out_data_size;
		}

		template<typename T> requires RingBufferTypeRequirements<T>
		[[nodiscard]] inline Size Write(const T& in_data) noexcept
		{
			return Write(in_data.GetBytes(), in_data.GetSize());
		}

		[[nodiscard]] Size Write(const Byte* in_data, Size in_data_size) noexcept
		{
			const auto write_size = GetWriteSize();
			if (write_size == 0 || in_data == 0 || in_data_size <= 0)
			{
				return 0;
			}

			// Can't write more than available
			if (in_data_size > write_size)
			{
				in_data_size = write_size;
			}

			const auto size = GetSize();
			const auto len = size - m_WriteOffset;

			if (in_data_size > len)
			{
				// Wrap around
				std::memcpy(m_Buffer.GetBytes() + m_WriteOffset, in_data, len);
				std::memcpy(m_Buffer.GetBytes(), in_data + len, in_data_size - len);
			}
			else
			{
				std::memcpy(m_Buffer.GetBytes() + m_WriteOffset, in_data, in_data_size);
			}

			m_WriteOffset = (m_WriteOffset + in_data_size) % size;
			m_WriteSpace -= in_data_size;

			return in_data_size;
		}

		[[nodiscard]] inline Size GetSize() const noexcept { return m_Buffer.GetSize(); }
		
		[[nodiscard]] inline Size GetWriteSize() const noexcept
		{
			return m_WriteSpace;
		}

		[[nodiscard]] inline Size GetReadSize() const noexcept { return (GetSize() - m_WriteSpace); }

		inline void Swap(RingBufferImpl& other) noexcept
		{
			m_Buffer.Swap(other.m_Buffer);
			m_ReadOffset = std::exchange(other.m_ReadOffset, m_ReadOffset);
			m_WriteOffset = std::exchange(other.m_WriteOffset, m_WriteOffset);
			m_WriteSpace = std::exchange(other.m_WriteSpace, m_WriteSpace);
		}

		inline void Resize(const Size new_size)
		{
			if (new_size == GetSize()) return;

			BufferType new_buffer(new_size);

			const auto numread = Read(new_buffer);
			m_Buffer.Swap(new_buffer);
			
			m_ReadOffset = 0;
			m_WriteOffset = numread;
			m_WriteSpace = new_size - numread;
		}

		inline void Clear() noexcept
		{
			m_ReadOffset = 0;
			m_WriteOffset = 0;
			m_WriteSpace = GetSize();
		}

	private:
		BufferType m_Buffer;
		Size m_ReadOffset{ 0 };
		Size m_WriteOffset{ 0 };
		Size m_WriteSpace{ 0 };
	};

	using FreeRingBuffer = RingBufferImpl<>;
	using RingBuffer = RingBufferImpl<Buffer>;
	using ProtectedRingBuffer = RingBufferImpl<ProtectedBuffer>;
}