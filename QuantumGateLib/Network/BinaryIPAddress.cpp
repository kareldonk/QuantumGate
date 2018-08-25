// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "..\Common\Hash.h"

namespace QuantumGate::Implementation::Network
{
	std::size_t BinaryIPAddress::GetHash() const noexcept
	{
		const QuantumGate::Implementation::Network::SerializedBinaryIPAddress sip{ *this }; // Gets rid of padding bytes

		return static_cast<std::size_t>(
			QuantumGate::Implementation::Hash::GetNonPersistentHash(
				QuantumGate::Implementation::Memory::BufferView(reinterpret_cast<const QuantumGate::Byte*>(&sip),
																sizeof(sip))));
	}
}