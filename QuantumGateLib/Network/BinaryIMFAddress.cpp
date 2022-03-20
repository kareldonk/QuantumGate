// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BinaryIMFAddress.h"
#include "..\Common\Hash.h"

namespace QuantumGate::Implementation::Network
{
	std::size_t BinaryIMFAddress::GetHash() const noexcept
	{
#pragma pack(push, 1) // Disable padding bytes
		struct hash_data final
		{
			BinaryIMFAddress::Family AddressFamily{ BinaryIMFAddress::Family::Unspecified };
			WChar Address[BinaryIMFAddress::MaxAddressStringLength]{ 0 };
		};
#pragma pack(pop)

		hash_data sip;
		sip.AddressFamily = AddressFamily;
		std::memcpy(&sip.Address, Address.data(), Address.size() * sizeof(WChar));

		return static_cast<std::size_t>(
			QuantumGate::Implementation::Hash::GetNonPersistentHash(
				QuantumGate::Implementation::Memory::BufferView(reinterpret_cast<const QuantumGate::Byte*>(&sip),
																sizeof(sip))));
	}
}