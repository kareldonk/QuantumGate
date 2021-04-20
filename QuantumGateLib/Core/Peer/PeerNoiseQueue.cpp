// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerNoiseQueue.h"
#include "..\..\Common\Random.h"

namespace QuantumGate::Implementation::Core::Peer
{
	bool NoiseQueue::QueueNoise(const Settings& settings, const bool inhandshake) noexcept
	{
		try
		{
			// If we should send messages
			if (settings.Noise.MaxMessagesPerInterval > 0 &&
				settings.Noise.MaxMessageSize > 0)
			{
				auto interval = settings.Noise.TimeInterval;
				auto minmsg = settings.Noise.MinMessagesPerInterval;
				auto maxmsg = settings.Noise.MaxMessagesPerInterval;

				// If we're in handshake state, noise gets handled differently
				// in order to guarantee a minimum amount of noise
				if (inhandshake)
				{
					const auto oint = interval.count() > 0 ? interval : std::chrono::seconds(1);
					interval = std::chrono::duration_cast<std::chrono::seconds>(2 * settings.Local.MaxHandshakeDelay);

					minmsg = 0;
					maxmsg = static_cast<Size>((static_cast<double>(interval.count()) /
												static_cast<double>(oint.count())) * static_cast<double>(maxmsg));

					// Guarantee possible maximum of 3 noise messages per second
					const auto msgps = static_cast<Size>(interval.count() * 3);
					if (maxmsg < msgps) maxmsg = msgps;

					Dbg(L"Handshake noise - Interval: %llds, MaxMsg: %zu", interval.count(), maxmsg);
				}

				// Random amount of noise messages
				const auto num = std::abs(Random::GetPseudoRandomNumber(minmsg, maxmsg));
				for (auto x = 0ll; x < num; ++x)
				{
					m_NoiseQueue.emplace(interval, settings.Noise.MinMessageSize, settings.Noise.MaxMessageSize);
				}
			}
		}
		catch (...)
		{
			return false;
		}

		return true;
	}

	std::optional<NoiseItem> NoiseQueue::GetQueuedNoise() noexcept
	{
		if (!m_NoiseQueue.empty())
		{
			if (m_NoiseQueue.top().IsTime())
			{
				std::optional<NoiseItem> noiseitm = m_NoiseQueue.top();
				m_NoiseQueue.pop();

				Dbg(L"\r\nQueued noiseitem - time:%lld, sec:%lldms, min:%zu, max:%zu\r\n",
					(noiseitm->ScheduleSteadyTime).time_since_epoch().count(),
					noiseitm->ScheduleMilliseconds.count(), noiseitm->MinSize, noiseitm->MaxSize);

				return noiseitm;
			}
		}

		return std::nullopt;
	}

	void NoiseQueue::Suspend() noexcept
	{
		assert(!m_SuspendSteadyTime.has_value());

		m_SuspendSteadyTime = Util::GetCurrentSteadyTime();
	}

	bool NoiseQueue::Resume() noexcept
	{
		assert(m_SuspendSteadyTime.has_value());

		try
		{
			if (!m_NoiseQueue.empty())
			{
				// Move all the items to a new queue which will reschedule
				// them using the current time
				NoiseItemQueue new_queue{ &NoiseItem::Compare };

				while (!m_NoiseQueue.empty())
				{
					const auto& noiseitm = m_NoiseQueue.top();

					const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(*m_SuspendSteadyTime -
																							 noiseitm.ScheduleSteadyTime);
					const auto interval = noiseitm.ScheduleMilliseconds - delta;
					
					new_queue.emplace(interval, noiseitm.MinSize, noiseitm.MaxSize);

					Dbg(L"Queued noiseitem - time:%lld, sec:%lldms rescheduled to sec:%lldms (delta: %lldms), min:%zu, max:%zu",
						(noiseitm.ScheduleSteadyTime).time_since_epoch().count(), noiseitm.ScheduleMilliseconds.count(),
						interval.count(), delta.count(), noiseitm.MinSize, noiseitm.MaxSize);

					m_NoiseQueue.pop();
				}

				m_NoiseQueue = std::move(new_queue);
			}
		}
		catch (...)
		{
			return false;
		}

		m_SuspendSteadyTime.reset();

		return true;
	}
}
