// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "BufferReader.h"
#include "..\Common\Endian.h"

namespace QuantumGate::Implementation::Memory
{
	BufferReader::BufferReader(const BufferView& buffer, const bool network_byteorder) noexcept :
		m_Buffer(buffer)
	{
		m_ConvertFromNetworkByteOrder = (network_byteorder && Endian::GetLocalEndian() != EndianType::BigEndian);
	}

	bool BufferReader::ReadEncodedSize(Size& size, const Size maxsize) noexcept
	{
		auto success = false;

		UInt8 es{ 0 };
		if (Read(es))
		{
			if (es < MaxSize::UInt8 - 2)
			{
				size = es;
				success = true;
			}
			else if (es == MaxSize::UInt8 - 2)
			{
				UInt16 is{ 0 };
				success = Read(is);
				size = is;
			}
			else if (es == MaxSize::UInt8 - 1)
			{
				UInt32 is{ 0 };
				success = Read(is);
				size = is;
			}
#ifdef _WIN64
			else if (es == MaxSize::UInt8)
			{
				UInt64 is{ 0 };
				success = Read(is);
				size = is;
			}
#endif
		}
		
		assert(size <= maxsize);

		if (success && size > maxsize) success = false;

		return success;
	}

	bool BufferReader::ReadBytes(Byte* data, const Size len, const bool endian_convert) noexcept
	{
		if (len == 0) return true;

		assert(data != nullptr);
		assert(m_Pointer + len <= m_Buffer.GetSize());

		if (data != nullptr && m_Pointer + len <= m_Buffer.GetSize())
		{
			if (endian_convert && m_ConvertFromNetworkByteOrder)
			{
				// For endian conversion we read the bytes in reverse order
				const Byte* bp = m_Buffer.GetBytes();

				for (Size i = 0; i < len; ++i)
				{
					data[len - i - 1] = bp[m_Pointer + i];
				}
			}
			else memcpy(data, m_Buffer.GetBytes() + m_Pointer, len);

			m_Pointer += len;

			return true;
		}

		return false;
	}

	template<> bool BufferReader::ReadImpl(String& data)
	{
		return ReadBytes(reinterpret_cast<Byte*>(data.data()), GetDataSize(data));
	}

	template<> bool BufferReader::ReadImpl(Network::SerializedBinaryIPAddress& data)
	{
		return ReadBytes(reinterpret_cast<Byte*>(&data), GetDataSize(data));
	}

	template<> bool BufferReader::ReadImpl(Network::SerializedIPEndpoint& data)
	{
		return ReadBytes(reinterpret_cast<Byte*>(&data), GetDataSize(data));
	}

	template<> bool BufferReader::ReadImpl(SerializedUUID& data)
	{
		return ReadBytes(reinterpret_cast<Byte*>(&data), GetDataSize(data));
	}

	template<> bool BufferReader::ReadImpl(Buffer& data)
	{
		return ReadBytes(data.GetBytes(), GetDataSize(data));
	}

	template<> bool BufferReader::ReadImpl(ProtectedBuffer& data)
	{
		return ReadBytes(data.GetBytes(), GetDataSize(data));
	}

	template<> bool BufferReader::ReadImpl(const SizeWrap<String>& data)
	{
		Size size{ 0 };
		return (ReadEncodedSize(size, data.MaxSize()) &&
				[&]()
				{
					// Size is in bytes so we need to divide it
					data->resize(size / sizeof(String::value_type));
					return true;
				}() &&
				ReadImpl(*data));
	}

	template<> bool BufferReader::ReadImpl(const SizeWrap<Buffer>& data)
	{
		Size size{ 0 };
		return (ReadEncodedSize(size, data.MaxSize()) &&
				[&]() { data->Resize(size); return true; }() &&
				ReadImpl(*data));
	}

	template<> bool BufferReader::ReadImpl(const SizeWrap<ProtectedBuffer>& data)
	{
		Size size{ 0 };
		return (ReadEncodedSize(size, data.MaxSize()) &&
				[&]() { data->Resize(size); return true; }() &&
				ReadImpl(*data));
	}
}
