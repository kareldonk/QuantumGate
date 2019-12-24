// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation
{
	class Endian final
	{
	private:
		Endian() noexcept = default;

	public:
		enum class Type
		{
			Unknown, Little, Big
		};

		template<typename T>
		static constexpr void ToNetworkByteOrder(const T& indata, T& outdata) noexcept
		{
			static_assert(std::is_same_v<T, Byte> || std::is_integral_v<T>, "Unsupported type");

			ToNetworkByteOrder(reinterpret_cast<const Byte*>(&indata), reinterpret_cast<Byte*>(&outdata), sizeof(indata));
		}

		template<typename T>
		static constexpr T ToNetworkByteOrder(const T& indata) noexcept
		{
			static_assert(std::is_same_v<T, Byte> || std::is_integral_v<T>, "Unsupported type");

			T outdata{ 0 };
			ToNetworkByteOrder(indata, outdata);
			return outdata;
		}

		template<typename T>
		static constexpr void FromNetworkByteOrder(const T& indata, T& outdata) noexcept
		{
			static_assert(std::is_same_v<T, Byte> || std::is_integral_v<T>, "Unsupported type");

			FromNetworkByteOrder(reinterpret_cast<const Byte*>(&indata), reinterpret_cast<Byte*>(&outdata), sizeof(indata));
		}

		template<typename T>
		static constexpr T FromNetworkByteOrder(const T& indata) noexcept
		{
			static_assert(std::is_same_v<T, Byte> || std::is_integral_v<T>, "Unsupported type");

			T outdata{ 0 };
			FromNetworkByteOrder(indata, outdata);
			return outdata;
		}

		static constexpr Type GetNative() noexcept
		{
			// 41 42 43 44 = 'ABCD' hex ASCII code
			constexpr UInt32 little{ 0x41424344 };

			// 44 43 42 41 = 'DCBA' hex ASCII code
			constexpr UInt32 big{ 0x44434241 };

			// Converts chars to uint32 on current platform
			constexpr UInt32 native{ 'ABCD' };

			if constexpr (little == native)
			{
				return Type::Little;
			}
			else if constexpr (big == native)
			{
				return Type::Big;
			}
			else
			{
				return Type::Unknown;
			}
		}

		static constexpr bool IsLittleEndian() noexcept
		{
			return (GetNative() == Type::Little);
		}

		static constexpr bool IsBigEndian() noexcept
		{
			return (GetNative() == Type::Big);
		}

		static constexpr void ToNetworkByteOrder(const Byte* indata, Byte* outdata, const Size len) noexcept
		{
			assert(indata != nullptr && outdata != nullptr);

			if constexpr (GetNative() != Type::Big)
			{
				for (Size i = 0; i < len; ++i)
				{
					// For endian conversion we copy the bytes in reverse order
					outdata[i] = indata[len - 1 - i];
				}
			}
			else
			{
				std::memcpy(outdata, indata, len);
			}
		}

		static constexpr void FromNetworkByteOrder(const Byte* indata, Byte* outdata, const Size len) noexcept
		{
			ToNetworkByteOrder(indata, outdata, len);
		}
	};

	static_assert(Endian::GetNative() != Endian::Type::Unknown, "Unable to determine native endianness");
}