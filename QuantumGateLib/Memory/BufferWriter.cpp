// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BufferWriter.h"
#include "..\Common\Endian.h"

namespace QuantumGate::Implementation::Memory
{
	BufferWriter::BufferWriter(const bool network_byteorder) noexcept : m_Buffer(m_LocalBuffer)
	{
		m_ConvertToNetworkByteOrder = (network_byteorder && Endian::GetNative() != Endian::Type::Big);
	}

	BufferWriter::BufferWriter(Buffer& buffer, const bool network_byteorder) noexcept : m_Buffer(buffer.GetVector())
	{
		m_ConvertToNetworkByteOrder = (network_byteorder && Endian::GetNative() != Endian::Type::Big);
		m_Buffer.clear();
	}

	Buffer::VectorType&& BufferWriter::MoveWrittenBytes() noexcept
	{
		// If we had a prealloc size the final buffer length should match;
		// this is to make sure we preallocate exactly what we need which
		// speeds things up because of one allocation only
		assert(m_PreAllocSize > 0 ? m_Buffer.size() == m_PreAllocSize : true);

		return std::move(m_Buffer);
	}

	bool BufferWriter::WriteEncodedSize(const Size size, const Size maxsize) noexcept
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

	bool BufferWriter::WriteBytes(const Byte* data, const Size len, const bool endian_convert)
	{
		if (len == 0) return true;

		assert(data != nullptr);

		if (data != nullptr)
		{
			m_Buffer.insert(m_Buffer.end(), len, Byte{ 0 });

			// Get beginning address for newly inserted bytes
			Byte* bp = m_Buffer.data() + (m_Buffer.size() - len);

			if (endian_convert && m_ConvertToNetworkByteOrder)
			{
				// For endian conversion we insert the bytes in reverse order
				for (Size i = 0; i < len; ++i)
				{
					bp[i] = data[len - 1 - i];
				}
			}
			else memcpy(bp, data, len);

			return true;
		}

		return false;
	}

	template<> bool BufferWriter::WriteImpl(const BufferSpan& data)
	{
		return WriteBytes(data.GetBytes(), GetDataSize(data));
	}

	template<> bool BufferWriter::WriteImpl(const BufferView& data)
	{
		return WriteBytes(data.GetBytes(), GetDataSize(data));
	}

	template<> bool BufferWriter::WriteImpl(const String& data)
	{
		return WriteBytes(reinterpret_cast<const Byte*>(data.data()), GetDataSize(data));
	}

	template<> bool BufferWriter::WriteImpl(const Network::SerializedBinaryIPAddress& data)
	{
		return WriteBytes(reinterpret_cast<const Byte*>(&data), GetDataSize(data));
	}

	template<> bool BufferWriter::WriteImpl(const Network::SerializedIPEndpoint& data)
	{
		return WriteBytes(reinterpret_cast<const Byte*>(&data), GetDataSize(data));
	}

	template<> bool BufferWriter::WriteImpl(const SerializedUUID& data)
	{
		return WriteBytes(reinterpret_cast<const Byte*>(&data), GetDataSize(data));
	}

	template<> bool BufferWriter::WriteImpl(const Buffer& data)
	{
		return WriteBytes(data.GetBytes(), GetDataSize(data));
	}

	template<> bool BufferWriter::WriteImpl(const ProtectedBuffer& data)
	{
		return WriteBytes(data.GetBytes(), GetDataSize(data));
	}
}