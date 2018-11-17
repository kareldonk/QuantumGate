// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Crypto\KeyData.h"
#include "..\..\Concurrency\SharedSpinMutex.h"

#include <queue>

namespace QuantumGate::Implementation::Core::KeyGeneration
{
	struct KeyQueue final
	{
		KeyQueue(const Algorithm::Asymmetric alg) noexcept : Algorithm(alg) {}

		Algorithm::Asymmetric Algorithm{ Algorithm::Asymmetric::Unknown };
		std::queue<Crypto::AsymmetricKeyData> Queue;
		Size NumPendingEvents{ 0 };
		bool Active{ true };
	};

	using KeyQueue_ThS = Concurrency::ThreadSafe<KeyQueue, Concurrency::SpinMutex>;

	class Event final
	{
	public:
		Event() = default;

		Event(KeyQueue_ThS* key_queueths) noexcept : m_KeyQueue(key_queueths)
		{
			m_KeyQueue->WithUniqueLock()->NumPendingEvents++;
		}

		~Event()
		{
			if (m_KeyQueue != nullptr) m_KeyQueue->WithUniqueLock()->NumPendingEvents--;
		}

		Event(const Event&) = delete;

		Event(Event&& other) noexcept
		{
			*this = std::move(other);
		}

		Event& operator=(const Event&) = delete;

		Event& operator=(Event&& other) noexcept
		{
			m_KeyQueue = std::exchange(other.m_KeyQueue, nullptr);
			return *this;
		}

		inline explicit operator bool() const noexcept { return (m_KeyQueue != nullptr); }

		inline KeyQueue_ThS* GetQueue() noexcept { return m_KeyQueue; }

	private:
		KeyQueue_ThS* m_KeyQueue{ nullptr };
	};
}