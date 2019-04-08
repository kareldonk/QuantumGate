// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core::Peer
{
	struct NoiseItem final
	{
		NoiseItem() = default;
		NoiseItem(const std::chrono::milliseconds& max_interval, const Size minsize, const Size maxsize) noexcept;

		inline bool IsTime() const noexcept
		{
			if ((Util::GetCurrentSteadyTime() - ScheduleSteadyTime) >= ScheduleMilliseconds) return true;

			return false;
		}

		inline static bool Compare(const NoiseItem& item1, const NoiseItem& item2) noexcept
		{
			return (item1.ScheduleSteadyTime + item1.ScheduleMilliseconds) > (item2.ScheduleSteadyTime + item2.ScheduleMilliseconds);
		}

		SteadyTime ScheduleSteadyTime;
		std::chrono::milliseconds ScheduleMilliseconds{ 0 };
		Size MinSize{ 0 };
		Size MaxSize{ 512 };
	};
}
