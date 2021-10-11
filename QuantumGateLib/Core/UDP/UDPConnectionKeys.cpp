// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnectionKeys.h"
#include "..\..\..\QuantumGateCryptoLib\QuantumGateCryptoLib.h"

namespace QuantumGate::Implementation::Core::UDP
{
	void SymmetricKeys::CreateKeys(const ProtectedBuffer& global_sharedsecret, const BufferView key_input_data)
	{
		m_KeyData.Allocate(KeyDataLength);

		if (!global_sharedsecret.IsEmpty())
		{
			// siphash requires keysize of 16
			assert(global_sharedsecret.GetSize() >= 16);

			siphash(reinterpret_cast<const uint8_t*>(key_input_data.GetBytes()), key_input_data.GetSize(),
					reinterpret_cast<const uint8_t*>(global_sharedsecret.GetBytes()),
					reinterpret_cast<uint8_t*>(m_KeyData.GetBytes()), m_KeyData.GetSize());
		}
		else
		{
			assert(key_input_data.GetSize() >= KeyDataLength);

			std::memcpy(m_KeyData.GetBytes(), key_input_data.GetBytes(), KeyDataLength);
		}
	}
}