// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerNoiseItem.h"
#include "..\..\Common\Random.h"

namespace QuantumGate::Implementation::Core::Peer
{
	NoiseItem::NoiseItem(const std::chrono::milliseconds& max_interval, const Size minsize, const Size maxsize) noexcept
	{
		ScheduleSteadyTime = Util::GetCurrentSteadyTime();
		ScheduleMilliseconds = std::chrono::milliseconds(abs(Random::GetPseudoRandomNumber(0, max_interval.count())));
		MinSize = minsize;
		MaxSize = maxsize;
	}
}
