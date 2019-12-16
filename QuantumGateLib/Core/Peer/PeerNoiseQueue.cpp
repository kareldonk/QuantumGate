// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
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

					Dbg(L"Handshake noise - Interval: %us, MaxMsg: %u", interval.count(), maxmsg);
				}

				// Random amount of noise messages
				const auto num = std::abs(Random::GetPseudoRandomNumber(minmsg, maxmsg));
				for (auto x = 0ll; x < num; ++x)
				{
					m_NoiseQueue.Push(NoiseItem(interval,
												settings.Noise.MinMessageSize,
												settings.Noise.MaxMessageSize));
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
		if (!m_NoiseQueue.Empty())
		{
			if (m_NoiseQueue.Top().IsTime())
			{
				std::optional<NoiseItem> noiseitm = m_NoiseQueue.Top();
				m_NoiseQueue.Pop();

				Dbg(L"\r\nQueued noiseitem - time:%u, sec:%u, min:%u, max:%u\r\n",
					(noiseitm->ScheduleSteadyTime).time_since_epoch().count(),
					noiseitm->ScheduleMilliseconds, noiseitm->MinSize, noiseitm->MaxSize);

				return noiseitm;
			}
		}

		return std::nullopt;
	}
}
