// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BufferIO.h"

namespace QuantumGate::Implementation::Memory
{
	class Export BufferWriter final : public BufferIO
	{
	public:
		BufferWriter(const bool network_byteorder = false) noexcept;
		BufferWriter(Buffer& buffer, const bool network_byteorder = false) noexcept;
		BufferWriter(const BufferWriter&) = delete;
		BufferWriter(BufferWriter&&) = default;
		virtual ~BufferWriter() = default;
		BufferWriter& operator=(const BufferWriter&) = delete;
		BufferWriter& operator=(BufferWriter&&) = default;

		void Preallocate(const Size size) 
		{ 
			m_Buffer.reserve(m_Buffer.size() + size);
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

		[[nodiscard]] Buffer::VectorType&& MoveWrittenBytes() noexcept;

	private:
		template<typename T>
		[[nodiscard]] std::enable_if_t<!std::is_enum_v<T>, bool> WriteImpl(const T& data)
		{
			static_assert(std::is_integral_v<T> || std::is_same_v<T, Byte>, "Unsupported type.");

			return WriteBytes(reinterpret_cast<const Byte*>(&data), GetDataSize(data), m_ConvertToNetworkByteOrder);
		}

		template<typename T>
		[[nodiscard]] std::enable_if_t<std::is_enum_v<T>, bool> WriteImpl(const T& data)
		{
			return WriteImpl(static_cast<const std::underlying_type_t<T>>(data));
		}

		template<typename T>
		[[nodiscard]] std::enable_if_t<std::is_integral_v<T> ||
			std::is_same_v<T, Byte> || std::is_enum_v<T> ||
			std::is_same_v<T, SerializedUUID>, bool> WriteImpl(const Vector<T>& data)
		{
			for (const auto& val : data)
			{
				if (!WriteImpl(val)) return false;
			}

			return true;
		}

		template<typename T>
		[[nodiscard]] bool WriteImpl(const SizeWrap<T>& data)
		{
			return (WriteEncodedSize(GetDataSize(*data), data.MaxSize()) && WriteImpl(*data));
		}

		[[nodiscard]] bool WriteEncodedSize(const Size size, const Size maxsize) noexcept;
		[[nodiscard]] bool WriteBytes(const Byte* data, const Size len, const bool endian_convert = false);

	private:
		bool m_ConvertToNetworkByteOrder{ false };

		Buffer::VectorType m_LocalBuffer;
		Buffer::VectorType& m_Buffer;
		Size m_PreAllocSize{ 0 };
	};

	// Specializations
	template<> [[nodiscard]] Export bool BufferWriter::WriteImpl(const String& data);
	template<> [[nodiscard]] Export bool BufferWriter::WriteImpl(const Network::SerializedBinaryIPAddress& data);
	template<> [[nodiscard]] Export bool BufferWriter::WriteImpl(const Network::SerializedIPEndpoint& data);
	template<> [[nodiscard]] Export bool BufferWriter::WriteImpl(const SerializedUUID& data);
	template<> [[nodiscard]] Export bool BufferWriter::WriteImpl(const Buffer& data);
	template<> [[nodiscard]] Export bool BufferWriter::WriteImpl(const BufferView& data);
	template<> [[nodiscard]] Export bool BufferWriter::WriteImpl(const ProtectedBuffer& data);
}
