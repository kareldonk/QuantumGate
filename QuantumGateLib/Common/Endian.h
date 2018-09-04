// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation
{
	enum class EndianType
	{
		LittleEndian, BigEndian
	};

	class Endian
	{
	private:
		Endian() = default;

	public:
		template<typename T>
		static void ToNetworkByteOrder(const T& indata, T& outdata) noexcept
		{
			static_assert(std::is_same_v<T, Byte> || std::is_integral_v<T>, "Unsupported type");

			ToNetworkByteOrder(reinterpret_cast<const Byte*>(&indata), reinterpret_cast<Byte*>(&outdata), sizeof(indata));
		}

		template<typename T>
		static T ToNetworkByteOrder(const T& indata) noexcept
		{
			static_assert(std::is_same_v<T, Byte> || std::is_integral_v<T>, "Unsupported type");

			T outdata{ 0 };
			ToNetworkByteOrder(indata, outdata);
			return outdata;
		}

		template<typename T>
		static void FromNetworkByteOrder(const T& indata, T& outdata) noexcept
		{
			static_assert(std::is_same_v<T, Byte> || std::is_integral_v<T>, "Unsupported type");

			FromNetworkByteOrder(reinterpret_cast<const Byte*>(&indata), reinterpret_cast<Byte*>(&outdata), sizeof(indata));
		}

		template<typename T>
		static T FromNetworkByteOrder(const T& indata) noexcept
		{
			static_assert(std::is_same_v<T, Byte> || std::is_integral_v<T>, "Unsupported type");

			T outdata{ 0 };
			FromNetworkByteOrder(indata, outdata);
			return outdata;
		}

		static const EndianType GetLocalEndian() noexcept
		{
			const UInt16 s = 0x0201;
			const UInt8 c = *(reinterpret_cast<const UInt8*>(&s));
			return ((c == 1) ? EndianType::LittleEndian : EndianType::BigEndian);
		}

		static bool IsLittleEndian() noexcept
		{
			return (GetLocalEndian() == EndianType::LittleEndian);
		}

		static bool IsBigEndian() noexcept
		{
			return (GetLocalEndian() == EndianType::BigEndian);
		}

		static void ToNetworkByteOrder(const Byte* indata, Byte* outdata, const Size len) noexcept
		{
			assert(indata != nullptr && outdata != nullptr);

			if (GetLocalEndian() != EndianType::BigEndian)
			{
				for (Size i = 0; i < len; ++i)
				{
					// For endian conversion we copy the bytes in reverse order
					outdata[i] = indata[len - 1 - i];
				}
			}
			else memcpy(outdata, indata, len);
		}

		static void FromNetworkByteOrder(const Byte* indata, Byte* outdata, const Size len) noexcept
		{
			ToNetworkByteOrder(indata, outdata, len);
		}
	};
}