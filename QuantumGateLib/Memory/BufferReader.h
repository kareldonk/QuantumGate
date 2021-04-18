// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BufferIO.h"

namespace QuantumGate::Implementation::Memory
{
	class BufferReader final : public BufferIO
	{
	public:
		BufferReader() = delete;
		
		BufferReader(const BufferView& buffer, const bool network_byteorder = false) noexcept : m_Buffer(buffer)
		{
			m_ConvertFromNetworkByteOrder = (network_byteorder && Endian::GetNative() != Endian::Type::Big);
		}
		
		BufferReader(const BufferReader&) = delete;
		BufferReader(BufferReader&&) = delete;
		virtual ~BufferReader() = default;
		BufferReader& operator=(const BufferReader&) = delete;
		BufferReader& operator=(BufferReader&&) = delete;

		template<typename... Args>
		[[nodiscard]] bool Read(Args&... data) noexcept
		{
			try
			{
				return (ReadImpl(data) && ...);
			}
			catch (...) {}

			return false;
		}

	private:
		template<typename T>
		[[nodiscard]] std::enable_if_t<!std::is_enum_v<T>, bool> ReadImpl(T& data)
		{
			static_assert(std::is_integral_v<T> || std::is_same_v<T, Byte>, "Unsupported type.");

			return ReadBytes(reinterpret_cast<Byte*>(&data), GetDataSize(data), m_ConvertFromNetworkByteOrder);
		}

		template<typename T> requires (std::is_enum_v<T>)
		[[nodiscard]] bool ReadImpl(T& data)
		{
			std::underlying_type_t<T> tdata{ 0 };
			if (ReadImpl(tdata))
			{
				data = static_cast<T>(tdata);
				return true;
			}

			return false;
		}

		template<typename T> requires (std::is_integral_v<T> || std::is_same_v<T, Byte> ||
									   std::is_enum_v<T> || std::is_same_v<T, SerializedUUID>)
		[[nodiscard]] bool ReadImpl(Vector<T>& data)
		{
			for (std::size_t x = 0; x < data.size(); ++x)
			{
				if (!ReadImpl(data[x])) return false;
			}

			return true;
		}

		template<typename T> requires (std::is_same_v<T, BufferSpan> ||
									   std::is_same_v<T, Buffer> || std::is_same_v<T, ProtectedBuffer>)
		[[nodiscard]] bool ReadImpl(T& data)
		{
			return ReadBytes(data.GetBytes(), GetDataSize(data));
		}

		[[nodiscard]] bool ReadImpl(String& data)
		{
			return ReadBytes(reinterpret_cast<Byte*>(data.data()), GetDataSize(data));
		}

		[[nodiscard]] bool ReadImpl(Network::SerializedBinaryIPAddress& data)
		{
			return ReadBytes(reinterpret_cast<Byte*>(&data), GetDataSize(data));
		}

		[[nodiscard]] bool ReadImpl(Network::SerializedIPEndpoint& data)
		{
			return ReadBytes(reinterpret_cast<Byte*>(&data), GetDataSize(data));
		}

		[[nodiscard]] bool ReadImpl(SerializedUUID& data)
		{
			return ReadBytes(reinterpret_cast<Byte*>(&data), GetDataSize(data));
		}

		template<Size MaxSize>
		[[nodiscard]] bool ReadImpl(StackBuffer<MaxSize>& data)
		{
			return ReadBytes(data.GetBytes(), GetDataSize(data));
		}

		template<typename T>
		[[nodiscard]] bool ReadImpl(const SizeWrap<T>& data)
		{
			static_assert(false, "Unsupported type.");
			return false;
		}

		template<typename T>
		[[nodiscard]] bool ReadImpl(const SizeWrap<Vector<T>>& data)
		{
			Size size{ 0 };
			if (!ReadEncodedSize(size, data.MaxSize())) return false;

			// Size should be exact multiple of size of T
			// otherwise something is wrong
			assert(size % sizeof(T) == 0);
			if (size % sizeof(T) != 0) return false;

			// Size is in bytes so we need to divide it
			data->resize(size / sizeof(T));
			
			return ReadImpl(*data);
		}

		[[nodiscard]] bool ReadImpl(const SizeWrap<String>& data)
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

		[[nodiscard]] bool ReadImpl(const SizeWrap<Buffer>& data)
		{
			Size size{ 0 };
			return (ReadEncodedSize(size, data.MaxSize()) &&
					[&]() { data->Resize(size); return true; }() &&
					ReadImpl(*data));
		}

		[[nodiscard]] bool ReadImpl(const SizeWrap<ProtectedBuffer>& data)
		{
			Size size{ 0 };
			return (ReadEncodedSize(size, data.MaxSize()) &&
					[&]() { data->Resize(size); return true; }() &&
					ReadImpl(*data));
		}

		template<Size MaxSize>
		[[nodiscard]] bool ReadImpl(const SizeWrap<StackBuffer<MaxSize>>& data)
		{
			Size size{ 0 };
			return (ReadEncodedSize(size, data.MaxSize()) &&
					[&]() { data->Resize(size); return true; }() &&
					ReadImpl(*data));
		}

		[[nodiscard]] bool ReadEncodedSize(Size& size, const Size maxsize) noexcept
		{
			auto success = false;

			UInt8 es{ 0 };
			if (Read(es))
			{
				if (es < MaxSize::_UINT8 - 2)
				{
					size = es;
					success = true;
				}
				else if (es == MaxSize::_UINT8 - 2)
				{
					UInt16 is{ 0 };
					success = Read(is);
					size = is;
				}
				else if (es == MaxSize::_UINT8 - 1)
				{
					UInt32 is{ 0 };
					success = Read(is);
					size = is;
				}
#ifdef _WIN64
				else if (es == MaxSize::_UINT8)
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

		[[nodiscard]] bool ReadBytes(Byte* data, const Size len, const bool endian_convert = false) noexcept
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
				else std::memcpy(data, m_Buffer.GetBytes() + m_Pointer, len);

				m_Pointer += len;

				return true;
			}

			return false;
		}

	private:
		bool m_ConvertFromNetworkByteOrder{ false };

		Size m_Pointer{ 0 };
		const BufferView m_Buffer;
	};
}
