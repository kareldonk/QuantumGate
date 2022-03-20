// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IMF.h"

namespace QuantumGate::Implementation::Network
{
	struct Export BinaryIMFAddress
	{
	public:
		static constexpr UInt8 MaxAddressStringLength = 254; // excluding null terminator
		static constexpr UInt8 MaxAddressLocalPartStringLength = 64; // excluding null terminator

		using Family = IMF::AddressFamily;

		Family AddressFamily{ Family::Unspecified };

	private:
		std::vector<WChar> Address;

	public:
		constexpr BinaryIMFAddress() noexcept {}

		constexpr BinaryIMFAddress(const Family family, const StringView addr_str) :
			AddressFamily(family)
		{
			const auto len = addr_str.size();
			if (len <= MaxAddressStringLength)
			{
				Address.resize(len+1, 0);
				std::copy(addr_str.begin(), addr_str.end(), Address.begin());
			}
			else
			{
				throw std::invalid_argument("Invalid Internet Message Format address");
			}
		}

		constexpr const WChar* GetChars() const noexcept { return Address.data(); }
		constexpr Size GetSize() const noexcept { return Address.size(); }

		constexpr StringView GetStringView() const noexcept
		{
			const auto size = GetSize();
			if (size > 0) return StringView(GetChars(), size-1);
			
			return {};
		}

		constexpr BinaryIMFAddress(const BinaryIMFAddress& other) :
			AddressFamily(other.AddressFamily), Address(other.Address)
		{}

		constexpr BinaryIMFAddress(BinaryIMFAddress&& other) noexcept :
			AddressFamily(other.AddressFamily), Address(std::move(other.Address))
		{
			other.AddressFamily = Family::Unspecified;
		}

		constexpr ~BinaryIMFAddress() = default;

		constexpr BinaryIMFAddress& operator=(const BinaryIMFAddress& other)
		{
			// Check for same object
			if (this == &other) return *this;

			AddressFamily = other.AddressFamily;
			Address = other.Address;

			return *this;
		}

		constexpr BinaryIMFAddress& operator=(BinaryIMFAddress&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			AddressFamily = std::exchange(other.AddressFamily, Family::Unspecified);
			Address = std::move(other.Address);

			return *this;
		}

		constexpr bool operator==(const BinaryIMFAddress& other) const noexcept
		{
			return (AddressFamily == other.AddressFamily &&
					Address == other.Address);
		}

		constexpr bool operator!=(const BinaryIMFAddress& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr void Clear() noexcept
		{
			AddressFamily = Family::Unspecified;
			Address.clear();
		}

		std::size_t GetHash() const noexcept;
	};
}

namespace std
{
	// Specialization for standard hash function for BinaryIMFAddress
	template<> struct hash<QuantumGate::Implementation::Network::BinaryIMFAddress>
	{
		std::size_t operator()(const QuantumGate::Implementation::Network::BinaryIMFAddress& addr) const noexcept
		{
			return addr.GetHash();
		}
	};
}
