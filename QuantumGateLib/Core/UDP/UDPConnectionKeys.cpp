// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnectionKeys.h"
#include "..\..\..\QuantumGateCryptoLib\QuantumGateCryptoLib.h"

namespace QuantumGate::Implementation::Core::UDP
{
	void SymmetricKeys::CreateKeys(const PeerConnectionType connection_type, const ProtectedBuffer& global_sharedsecret,
								   const BufferView key_input_data)
	{
		m_KeyData.Allocate(KeyDataLength);

		// Should be exact multiples of 2
		assert(key_input_data.GetSize() % 2 == 0);

		const auto half_input_data_len = key_input_data.GetSize() / 2;
		const auto local_input = key_input_data.GetFirst(half_input_data_len);
		const auto peer_input = key_input_data.GetLast(half_input_data_len);

		auto local_output = BufferSpan(m_KeyData).GetFirst(KeyDataLength / 2);
		auto peer_output = BufferSpan(m_KeyData).GetLast(KeyDataLength / 2);

		// Siphash requires output size of 8 or 16
		assert(local_output.GetSize() == 16);
		assert(peer_output.GetSize() == 16);

		// Switch output keys
		if (connection_type == PeerConnectionType::Inbound)
		{
			local_output = std::exchange(peer_output, std::move(local_output));
		}

		if (!global_sharedsecret.IsEmpty())
		{
			// Siphash requires keysize of 16
			assert(global_sharedsecret.GetSize() >= 16);

			siphash(reinterpret_cast<const uint8_t*>(local_input.GetBytes()), local_input.GetSize(),
					reinterpret_cast<const uint8_t*>(global_sharedsecret.GetBytes()),
					reinterpret_cast<uint8_t*>(local_output.GetBytes()), local_output.GetSize());

			siphash(reinterpret_cast<const uint8_t*>(peer_input.GetBytes()), peer_input.GetSize(),
					reinterpret_cast<const uint8_t*>(global_sharedsecret.GetBytes()),
					reinterpret_cast<uint8_t*>(peer_output.GetBytes()), peer_output.GetSize());
		}
		else
		{
			assert(local_input.GetSize() >= local_output.GetSize());
			assert(peer_input.GetSize() >= peer_output.GetSize());

			std::memcpy(local_output.GetBytes(), local_input.GetBytes(), local_output.GetSize());
			std::memcpy(peer_output.GetBytes(), peer_input.GetBytes(), peer_output.GetSize());
		}
	}
}