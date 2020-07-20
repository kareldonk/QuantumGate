// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BufferIO.h"

namespace QuantumGate::Implementation::Memory
{
	class Export BufferReader final : public BufferIO
	{
	public:
		BufferReader() = delete;
		BufferReader(const BufferView& buffer, const bool network_byteorder = false) noexcept;
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

		template<typename T>
		[[nodiscard]] std::enable_if_t<std::is_enum_v<T>, bool> ReadImpl(T& data)
		{
			std::underlying_type_t<T> tdata{ 0 };
			if (ReadImpl(tdata))
			{
				data = static_cast<T>(tdata);
				return true;
			}

			return false;
		}

		template<typename T>
		[[nodiscard]] std::enable_if_t<std::is_integral_v<T> ||
			std::is_same_v<T, Byte> || std::is_enum_v<T> ||
			std::is_same_v<T, SerializedUUID>, bool> ReadImpl(Vector<T>& data)
		{
			for (std::size_t x = 0; x < data.size(); ++x)
			{
				if (!ReadImpl(data[x])) return false;
			}

			return true;
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

		[[nodiscard]] bool ReadEncodedSize(Size& size, const Size maxsize) noexcept;
		[[nodiscard]] bool ReadBytes(Byte* data, const Size len, const bool endian_convert = false) noexcept;

	private:
		bool m_ConvertFromNetworkByteOrder{ false };

		Size m_Pointer{ 0 };
		const BufferView m_Buffer;
	};

	// Specializations
	template<> [[nodiscard]] Export bool BufferReader::ReadImpl(String& data);
	template<> [[nodiscard]] Export bool BufferReader::ReadImpl(Network::SerializedBinaryIPAddress& data);
	template<> [[nodiscard]] Export bool BufferReader::ReadImpl(Network::SerializedIPEndpoint& data);
	template<> [[nodiscard]] Export bool BufferReader::ReadImpl(SerializedUUID& data);
	template<> [[nodiscard]] Export bool BufferReader::ReadImpl(Buffer& data);
	template<> [[nodiscard]] Export bool BufferReader::ReadImpl(ProtectedBuffer& data);
	template<> [[nodiscard]] Export bool BufferReader::ReadImpl(const SizeWrap<String>& data);
	template<> [[nodiscard]] Export bool BufferReader::ReadImpl(const SizeWrap<Buffer>& data);
	template<> [[nodiscard]] Export bool BufferReader::ReadImpl(const SizeWrap<ProtectedBuffer>& data);
}
