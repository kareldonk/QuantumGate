// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

// Undefine conflicting macro from Windows SDK for std::numeric_limits
#pragma push_macro("max")
#undef max

#include "StackBuffer.h"
#include "..\Common\Endian.h"
#include "..\Network\SerializedBinaryIPAddress.h"
#include "..\Network\SerializedBinaryBTHAddress.h"
#include "..\Network\SerializedBTHEndpoint.h"
#include "..\Network\SerializedIPEndpoint.h"

#include <limits>

namespace QuantumGate::Implementation::Memory
{
	struct MemorySize final
	{
		static constexpr Size _1B{ 0x00000001UL };
		static constexpr Size _256B{ 0x00000100UL };
		static constexpr Size _512B{ 0x00000200UL };
		static constexpr Size _1KB{ 0x00000400UL };
		static constexpr Size _65KB{ 0x00010000UL };
		static constexpr Size _1MB{ 0x00100000UL };
		static constexpr Size _2MB{ 0x00200000UL };
		static constexpr Size _4MB{ 0x00400000UL };
		static constexpr Size _8MB{ 0x00800000UL };
		static constexpr Size _16MB{ 0x01000000UL };
	};

	class MaxSize final
	{
	public:
		constexpr MaxSize(Size size) noexcept : m_Size(size) {}

		constexpr Size GetSize() const noexcept { return m_Size; }

		static constexpr Size _UINT8{ std::numeric_limits<UInt8>::max() };
		static constexpr Size _UINT16{ std::numeric_limits<UInt16>::max() };
		static constexpr Size _UINT32{ std::numeric_limits<UInt32>::max() };

		static constexpr Size _256B{ _UINT8 };
		static constexpr Size _512B{ 0x00000200UL };
		static constexpr Size _1KB{ 0x00000400UL };
		static constexpr Size _65KB{ _UINT16 };
		static constexpr Size _1MB{ 0x00100000UL };
		static constexpr Size _2MB{ 0x00200000UL };
		static constexpr Size _4MB{ 0x00400000UL };
		static constexpr Size _8MB{ 0x00800000UL };
		static constexpr Size _16MB{ 0x00ffffffUL };
		static constexpr Size _4GB{ _UINT32 };

#ifdef _WIN64
		static constexpr Size _UINT64{ std::numeric_limits<QuantumGate::UInt64>::max() };
		static constexpr Size _18EB{ _UINT64 };
#endif

	private:
		const Size m_Size{ 0 };
	};

	namespace BufferIO
	{
		template<typename T>
		concept Sizeable = requires(T v)
		{
			{ v.GetDataSize() } -> std::same_as<Size>;
		};

		template<typename T, typename R>
		concept Readable = requires(T v, R& x)
		{
			{ v.Read(x) } -> std::same_as<bool>;
				requires Sizeable<T>;
		};

		template<typename T, typename W>
		concept Writable = requires(T v, W& x)
		{
			{ v.Write(x) } -> std::same_as<bool>;
				requires Sizeable<T>;
		};

		template<typename T>
		class SizeWrap final
		{
		public:
			SizeWrap(T& data, const Size maxsize) noexcept :
				m_Data(data), m_MaxSize(maxsize)
			{}

			const Size MaxSize() const noexcept { return m_MaxSize; }

			T* operator->() const noexcept { return &m_Data; }
			T& operator*() const noexcept { return m_Data; }

		private:
			T& m_Data;
			const Size m_MaxSize{ 0 };
		};

		// Encoding as also used in Satoshi Nakamoto's Bitcoin code
		// size < 253 becomes 1 byte
		// size <= UInt16 becomes 253 + 2 bytes)
		// size <= UInt32 becomes 254 + 4 bytes)
		// size > UInt32 becomes 255 + 8 bytes)
		constexpr Size GetSizeOfEncodedSize(Size size) noexcept
		{
			if (size < MaxSize::_UINT8 - 2) return sizeof(UInt8);
			else if (size <= MaxSize::_UINT16) return (sizeof(UInt8) + sizeof(UInt16));
			else if (size <= MaxSize::_UINT32) return (sizeof(UInt8) + sizeof(UInt32));
			else return (sizeof(UInt8) + sizeof(UInt64));
		}

		template<typename T>
		std::enable_if_t<!std::is_enum_v<T>, Size> GetDataSize(const T& data) noexcept
		{
			static_assert(std::is_integral_v<T> || std::is_same_v<T, Byte>, "Unsupported type.");

			return sizeof(T);
		}

		template<typename T>
		std::enable_if_t<std::is_enum_v<T>, Size> GetDataSize(const T& data) noexcept
		{
			return sizeof(std::underlying_type_t<T>);
		}

		template<typename T> requires (Sizeable<T>)
		Size GetDataSize(const T& data) noexcept
		{
			return data.GetDataSize();
		}

		template<typename T>
		std::enable_if_t<std::is_integral_v<T> ||
			std::is_same_v<T, Byte> || std::is_enum_v<T> ||
			std::is_same_v<T, SerializedUUID>, Size> GetDataSize(const Vector<T>& data) noexcept
		{
			return (data.size() * sizeof(T));
		}

		template<Size MaxSize>
		Size GetDataSize(const StackBuffer<MaxSize>& data) noexcept
		{
			return data.GetSize();
		}

		template<typename T>
		Size GetDataSize(const SizeWrap<T>& data) noexcept
		{
			const auto size = GetDataSize(*data);
			return (GetSizeOfEncodedSize(size) + size);
		}

		// Specializations
		template<> Export Size GetDataSize(const BufferSpan& data) noexcept;
		template<> Export Size GetDataSize(const BufferView& data) noexcept;
		template<> Export Size GetDataSize(const String& data) noexcept;
		template<> Export Size GetDataSize(const Network::SerializedBinaryIPAddress& data) noexcept;
		template<> Export Size GetDataSize(const Network::SerializedBinaryBTHAddress& data) noexcept;
		template<> Export Size GetDataSize(const Network::SerializedIPEndpoint& data) noexcept;
		template<> Export Size GetDataSize(const Network::SerializedBTHEndpoint& data) noexcept;
		template<> Export Size GetDataSize(const SerializedUUID& data) noexcept;
		template<> Export Size GetDataSize(const Buffer& data) noexcept;
		template<> Export Size GetDataSize(const ProtectedBuffer& data) noexcept;

		template<typename... Args>
		Size GetDataSizes(const Args&... data) noexcept
		{
			return (GetDataSize(data) + ...);
		}
	}

	// Helper function
	template<typename T>
	const BufferIO::SizeWrap<T> WithSize(T& data, const MaxSize maxsize) noexcept
	{
		return { data, maxsize.GetSize() };
	}
}

#pragma pop_macro("max")