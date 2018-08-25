// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerNoiseItem.h"
#include "..\..\Concurrency\PriorityQueue.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class NoiseQueue
	{
		using NoiseItemQueue = Concurrency::PriorityQueue<NoiseItem, decltype(&NoiseItem::Compare)>;

	public:
		const bool QueueNoise(const Settings& settings, const bool inhandshake) noexcept;

		const std::optional<NoiseItem> GetQueuedNoise() noexcept;

		inline const bool IsQueuedNoiseReady() const noexcept
		{
			if (!m_NoiseQueue.Empty())
			{
				return m_NoiseQueue.Top().IsTime();
			}

			return false;
		}

		inline const Concurrency::EventCondition& GetEvent() const noexcept { return m_NoiseQueue.Event(); }

	private:
		NoiseItemQueue m_NoiseQueue{ &NoiseItem::Compare };
	};
}