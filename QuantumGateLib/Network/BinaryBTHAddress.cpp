// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BinaryBTHAddress.h"
#include "..\Common\Hash.h"

namespace QuantumGate::Implementation::Network
{
	std::size_t BinaryBTHAddress::GetHash() const noexcept
	{
		const QuantumGate::Implementation::Network::SerializedBinaryBTHAddress addr{ *this }; // Gets rid of padding bytes

		return static_cast<std::size_t>(
			QuantumGate::Implementation::Hash::GetNonPersistentHash(
				QuantumGate::Implementation::Memory::BufferView(reinterpret_cast<const QuantumGate::Byte*>(&addr),
																sizeof(addr))));
	}
}