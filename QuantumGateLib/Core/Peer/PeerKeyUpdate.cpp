// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerKeyUpdate.h"
#include "Peer.h"
#include "..\..\Common\Random.h"

namespace QuantumGate::Implementation::Core::Peer
{
	const bool KeyUpdate::SetStatus(const Status status) noexcept
	{
		auto success = true;
		const auto prev_status = m_Status;

		switch (status)
		{
			case Status::UpdateWait:
				assert(prev_status == Status::Unknown || prev_status == Status::ReadyWait);
				if (prev_status == Status::Unknown || prev_status == Status::ReadyWait)
				{
					if (prev_status == Status::ReadyWait)
					{
						LogWarn(L"Key update for peer %s finished", m_Peer.GetPeerName().c_str());

						EndKeyUpdate();
					}

					m_Status = status;

					const auto settings = m_Peer.GetSettings();

					// Store the next time when we need to start
					// the key exchange sequence again
					m_UpdateSteadyTime = Util::GetCurrentSteadyTime();
					m_UpdateInterval =
						std::chrono::seconds(Random::GetPseudoRandomNumber(settings.Local.KeyUpdate.MinInterval.count(),
																		   settings.Local.KeyUpdate.MaxInterval.count()));
				}
				else success = false;
				break;
			case Status::PrimaryExchange:
				assert(prev_status == Status::UpdateWait);
				if (prev_status == Status::UpdateWait)
				{
					LogWarn(L"Beginning key update for peer %s", m_Peer.GetPeerName().c_str());

					m_Status = status;

					// Time is now the start of the key exchange sequence;
					// this is to check if it takes too long later
					m_UpdateSteadyTime = Util::GetCurrentSteadyTime();
				}
				else success = false;
				break;
			case Status::SecondaryExchange:
				assert(prev_status == Status::PrimaryExchange);
				if (prev_status == Status::PrimaryExchange) m_Status = status;
				else success = false;
				break;
			case Status::ReadyWait:
				assert(prev_status == Status::SecondaryExchange);
				if (prev_status == Status::SecondaryExchange) m_Status = status;
				else success = false;
				break;
		}

		return success;
	}

	const bool KeyUpdate::UpdateTimedOut() const noexcept
	{
		if (GetStatus() == Status::PrimaryExchange ||
			GetStatus() == Status::SecondaryExchange)
		{
			if ((Util::GetCurrentSteadyTime() - m_UpdateSteadyTime) >
				m_Peer.GetSettings().Local.KeyUpdate.MaxDuration)
			{
				return true;
			}
		}

		return false;
	}

	const bool KeyUpdate::ShouldUpdate() noexcept
	{
		if (GetStatus() == Status::UpdateWait &&
			m_Peer.GetConnectionType() == PeerConnectionType::Inbound &&
			m_Peer.GetStatus() == Core::Peer::Status::Ready)
		{
			const auto& settings = m_Peer.GetSettings();

			// If settings changed in the mean time get another
			// update interval, otherwise check if interval has expired
			if (settings.Local.KeyUpdate.MinInterval > m_UpdateInterval ||
				settings.Local.KeyUpdate.MaxInterval < m_UpdateInterval)
			{
				m_UpdateInterval =
					std::chrono::seconds(Random::GetPseudoRandomNumber(settings.Local.KeyUpdate.MinInterval.count(),
																	   settings.Local.KeyUpdate.MaxInterval.count()));
			}
			else if (Util::GetCurrentSteadyTime() - m_UpdateSteadyTime > m_UpdateInterval)
			{
				return true;
			}
			
			// If we processed the maximum number of bytes
			// the keys need to get updated
			if (m_Peer.GetKeys().HasNumBytesProcessedExceededForLatestKeyPair(
				settings.Local.KeyUpdate.RequireAfterNumProcessedBytes))
			{
				LogWarn(L"Number of bytes processed has been exceeded for latest symmetric keys for peer %s; will update",
						m_Peer.GetPeerName().c_str());
				return true;
			}
		}

		return false;
	}

	const bool KeyUpdate::BeginKeyUpdate() noexcept
	{
		// Should not already be updating
		assert(GetStatus() == Status::UpdateWait);

		if (m_Peer.InitializeKeyExchange())
		{
			if (m_Peer.GetMessageProcessor().SendBeginPrimaryKeyUpdateExchange())
			{
				return SetStatus(KeyUpdate::Status::PrimaryExchange);
			}
		}

		return false;
	}

	void KeyUpdate::EndKeyUpdate() noexcept
	{
		// Should be updating
		assert(GetStatus() == Status::ReadyWait);

		m_Peer.ReleaseKeyExchange();
	}
}