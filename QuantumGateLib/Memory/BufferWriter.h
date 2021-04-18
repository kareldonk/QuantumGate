// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BufferIO.h"

namespace QuantumGate::Implementation::Memory
{
	template<typename B>
	class BufferWriterImpl final : public BufferIO
	{
	public:
		BufferWriterImpl(const bool network_byteorder = false) noexcept : m_Buffer(m_LocalBuffer)
		{
			m_ConvertToNetworkByteOrder = (network_byteorder && Endian::GetNative() != Endian::Type::Big);
		}

		BufferWriterImpl(B& buffer, const bool network_byteorder = false) noexcept : m_Buffer(buffer)
		{
			m_ConvertToNetworkByteOrder = (network_byteorder && Endian::GetNative() != Endian::Type::Big);
			m_Buffer.Clear();
		}

		BufferWriterImpl(const BufferWriterImpl&) = delete;
		BufferWriterImpl(BufferWriterImpl&&) = delete;
		virtual ~BufferWriterImpl() = default;
		BufferWriterImpl& operator=(const BufferWriterImpl&) = delete;
		BufferWriterImpl& operator=(BufferWriterImpl&&) = delete;

		void Preallocate(const Size size) 
		{ 
			m_Buffer.Preallocate(m_Buffer.GetSize() + size);
			m_PreAllocSize += size;
		}

		template<typename... Args>
		[[nodiscard]] bool Write(const Args&... data) noexcept
		{
			try
			{
				return (WriteImpl(data) && ...);
			}
			catch (...) {}

			return false;
		}

		template<typename... Args>
		[[nodiscard]] bool WriteWithPreallocation(const Args&... data) noexcept
		{
			try
			{
				// One preallocation for speed
				const auto extra_size = GetDataSizes(data...);
				Preallocate(extra_size);

				return (WriteImpl(data) && ...);
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] B&& MoveWrittenBytes() noexcept
		{
			// If we had a prealloc size the final buffer length should match;
			// this is to make sure we preallocate exactly what we need which
			// speeds things up because of one allocation only
			assert(m_PreAllocSize > 0 ? m_Buffer.GetSize() == m_PreAllocSize : true);

			return std::move(m_Buffer);
		}

	private:
		template<typename T>
		[[nodiscard]] std::enable_if_t<!std::is_enum_v<T>, bool> WriteImpl(const T& data)
		{
			static_assert(std::is_integral_v<T> || std::is_same_v<T, Byte>, "Unsupported type.");

			return WriteBytes(reinterpret_cast<const Byte*>(&data), GetDataSize(data), m_ConvertToNetworkByteOrder);
		}

		template<typename T> requires (std::is_enum_v<T>)
		[[nodiscard]] bool WriteImpl(const T& data)
		{
			return WriteImpl(static_cast<const std::underlying_type_t<T>>(data));
		}

		template<typename T> requires (std::is_integral_v<T> || std::is_same_v<T, Byte> ||
									   std::is_enum_v<T> || std::is_same_v<T, SerializedUUID>)
		[[nodiscard]] bool WriteImpl(const Vector<T>& data)
		{
			for (const auto& val : data)
			{
				if (!WriteImpl(val)) return false;
			}

			return true;
		}

		template<typename T> requires (std::is_same_v<T, BufferSpan> || std::is_same_v<T, BufferView> ||
									   std::is_same_v<T, Buffer> || std::is_same_v<T, ProtectedBuffer>)
		[[nodiscard]] bool WriteImpl(const T& data)
		{
			return WriteBytes(data.GetBytes(), GetDataSize(data));
		}

		[[nodiscard]] bool WriteImpl(const String& data)
		{
			return WriteBytes(reinterpret_cast<const Byte*>(data.data()), GetDataSize(data));
		}

		template<typename T> requires (std::is_same_v<T, Network::SerializedBinaryIPAddress> ||
									   std::is_same_v<T, Network::SerializedIPEndpoint> ||
									   std::is_same_v<T, SerializedUUID>)
		[[nodiscard]] bool WriteImpl(const T& data)
		{
			return WriteBytes(reinterpret_cast<const Byte*>(&data), GetDataSize(data));
		}

		template<typename T>
		[[nodiscard]] bool WriteImpl(const SizeWrap<T>& data)
		{
			return (WriteEncodedSize(GetDataSize(*data), data.MaxSize()) && WriteImpl(*data));
		}

		template<Size MaxSize>
		[[nodiscard]] bool WriteImpl(const StackBuffer<MaxSize>& data)
		{
			return WriteBytes(data.GetBytes(), GetDataSize(data));
		}

		[[nodiscard]] bool WriteEncodedSize(const Size size, const Size maxsize) noexcept
		{
			assert(size <= maxsize);

			if (size > maxsize) return false;
			else if (size < MaxSize::_UINT8 - 2)
			{
				return Write(static_cast<UInt8>(size));
			}
			else if (size <= MaxSize::_UINT16)
			{
				const UInt8 es = MaxSize::_UINT8 - 2;
				return (Write(es) && Write(static_cast<UInt16>(size)));
			}
			else if (size <= MaxSize::_UINT32)
			{
				const UInt8 es = MaxSize::_UINT8 - 1;
				return (Write(es) && Write(static_cast<UInt32>(size)));
			}
#ifdef _WIN64
			else
			{
				const UInt8 es = MaxSize::_UINT8;
				return (Write(es) && Write(static_cast<UInt64>(size)));
			}
#endif

			return false;
		}

		[[nodiscard]] bool WriteBytes(const Byte* data, const Size len, const bool endian_convert = false)
		{
			if (len == 0) return true;

			assert(data != nullptr);

			if (data != nullptr)
			{
				m_Buffer.Resize(m_Buffer.GetSize() + len);

				// Get beginning address for newly inserted bytes
				Byte* bp = m_Buffer.GetBytes() + (m_Buffer.GetSize() - len);

				if (endian_convert && m_ConvertToNetworkByteOrder)
				{
					// For endian conversion we insert the bytes in reverse order
					for (Size i = 0; i < len; ++i)
					{
						bp[i] = data[len - 1 - i];
					}
				}
				else std::memcpy(bp, data, len);

				return true;
			}

			return false;
		}

	private:
		bool m_ConvertToNetworkByteOrder{ false };

		B m_LocalBuffer;
		B& m_Buffer;
		Size m_PreAllocSize{ 0 };
	};

	using BufferWriter = BufferWriterImpl<Buffer>;

	template<Size MaxSize>
	using StackBufferWriter = BufferWriterImpl<StackBuffer<MaxSize>>;
}
