// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerNoiseItem.h"
#include "..\..\Common\Containers.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class NoiseQueue final
	{
		using NoiseItemQueue = Containers::PriorityQueue<NoiseItem, Vector<NoiseItem>, decltype(&NoiseItem::Compare)>;

	public:
		[[nodiscard]] bool QueueNoise(const Settings& settings, const bool inhandshake) noexcept;

		[[nodiscard]] std::optional<NoiseItem> GetQueuedNoise() noexcept;

		[[nodiscard]] inline bool IsQueuedNoiseReady() const noexcept
		{
			if (!m_NoiseQueue.empty() && !m_SuspendSteadyTime.has_value())
			{
				return m_NoiseQueue.top().IsTime();
			}

			return false;
		}

		[[nodiscard]] inline bool IsEmpty() const noexcept { return m_NoiseQueue.empty(); }

		void Suspend() noexcept;
		[[nodiscard]] bool Resume() noexcept;

	private:
		NoiseItemQueue m_NoiseQueue{ &NoiseItem::Compare };
		std::optional<SteadyTime> m_SuspendSteadyTime;
	};
}