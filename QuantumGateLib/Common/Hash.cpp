// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Hash.h"
#include "..\Common\Random.h"

namespace QuantumGate::Implementation
{
	std::atomic_bool Hash::m_NonPersistentKeyInit = false;
	Concurrency::ThreadSafe<Memory::FreeBuffer, std::shared_mutex> Hash::m_NonPersistentKey;

	void Hash::InitNonPersistentKey() noexcept
	{
		m_NonPersistentKey.WithUniqueLock([&](auto& key)
		{
			key = Random::GetPseudoRandomBytes(m_KeySize);
			m_NonPersistentKeyInit = true;
		});
	}
}