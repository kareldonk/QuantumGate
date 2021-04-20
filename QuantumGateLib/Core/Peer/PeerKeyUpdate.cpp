// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerKeyUpdate.h"
#include "Peer.h"
#include "..\..\Common\Random.h"

namespace QuantumGate::Implementation::Core::Peer
{
	bool KeyUpdate::SetStatus(const Status status) noexcept
	{
		auto success = true;
		const auto prev_status = m_Status;

		if (prev_status == Status::Suspended)
		{
			m_Status = status;

			// Reset the update time to the current time minus the amount of time
			// that had already elapsed before getting suspended
			m_UpdateSteadyTime = Util::GetCurrentSteadyTime() - m_ResumeUpdateIntervalDelta;
		}
		else
		{
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
				case Status::Suspended:
					assert(prev_status != Status::Suspended && prev_status != Status::Unknown);
					if (prev_status != Status::Suspended && prev_status != Status::Unknown)
					{
						m_Status = status;

						m_ResumeStatus = prev_status;
						m_ResumeUpdateIntervalDelta =
							std::chrono::duration_cast<std::chrono::seconds>(Util::GetCurrentSteadyTime() - m_UpdateSteadyTime);
					}
					else success = false;
					break;
				default:
					// Shouldn't get here
					assert(false);
					success = false;
					break;
			}
		}

		return success;
	}

	bool KeyUpdate::UpdateTimedOut() const noexcept
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

	bool KeyUpdate::ShouldUpdate() noexcept
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

	bool KeyUpdate::BeginKeyUpdate() noexcept
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

	bool KeyUpdate::Suspend() noexcept
	{
		// Should not already be suspended and should be initialized
		assert(GetStatus() != Status::Suspended && GetStatus() != Status::Unknown);

		LogDbg(L"Suspending key update for peer %s", m_Peer.GetPeerName().c_str());

		return SetStatus(Status::Suspended);
	}

	bool KeyUpdate::Resume() noexcept
	{
		// Should be suspended
		assert(GetStatus() == Status::Suspended);

		LogDbg(L"Resuming key update for peer %s", m_Peer.GetPeerName().c_str());

		return SetStatus(m_ResumeStatus);
	}

	bool KeyUpdate::ProcessEvents() noexcept
	{
		// Nothing to process while suspended
		if (GetStatus() == Status::Suspended) return true;

		if (ShouldUpdate())
		{
			if (!BeginKeyUpdate())
			{
				LogErr(L"Couldn't initiate key update for peer %s; will disconnect", m_Peer.GetPeerName().c_str());
				return false;
			}
		}
		else if (UpdateTimedOut())
		{
			LogErr(L"Key update for peer %s timed out; will disconnect", m_Peer.GetPeerName().c_str());
			return false;
		}

		return true;
	}

	MessageProcessor::Result KeyUpdate::ProcessKeyUpdateMessage(MessageDetails&& msg) noexcept
	{
		MessageProcessor::Result result;

		switch (msg.GetMessageType())
		{
			case MessageType::BeginPrimaryKeyUpdateExchange:
			{
				if (m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
				{
					if (m_Peer.GetKeyUpdate().SetStatus(KeyUpdate::Status::PrimaryExchange))
					{
						if (m_Peer.InitializeKeyExchange())
						{
							result = m_Peer.GetMessageProcessor().ProcessKeyExchange(std::move(msg));
							if (result.Handled && result.Success)
							{
								result.Success = m_Peer.GetKeyUpdate().SetStatus(KeyUpdate::Status::SecondaryExchange);
							}
						}
					}
				}

				break;
			}
			case MessageType::EndPrimaryKeyUpdateExchange:
			{
				if (m_Peer.GetKeyUpdate().GetStatus() == KeyUpdate::Status::PrimaryExchange &&
					m_Peer.GetConnectionType() == PeerConnectionType::Inbound)
				{
					result = m_Peer.GetMessageProcessor().ProcessKeyExchange(std::move(msg));
					if (result.Handled && result.Success)
					{
						result.Success = m_Peer.GetKeyUpdate().SetStatus(KeyUpdate::Status::SecondaryExchange);
					}
				}

				break;
			}
			case MessageType::BeginSecondaryKeyUpdateExchange:
			{
				if (m_Peer.GetKeyUpdate().GetStatus() == KeyUpdate::Status::SecondaryExchange &&
					m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
				{
					result = m_Peer.GetMessageProcessor().ProcessKeyExchange(std::move(msg));
					if (result.Handled && result.Success)
					{
						result.Success = m_Peer.GetKeyUpdate().SetStatus(KeyUpdate::Status::ReadyWait);
					}
				}

				break;
			}
			case MessageType::EndSecondaryKeyUpdateExchange:
			{
				if (m_Peer.GetKeyUpdate().GetStatus() == KeyUpdate::Status::SecondaryExchange &&
					m_Peer.GetConnectionType() == PeerConnectionType::Inbound)
				{
					result = m_Peer.GetMessageProcessor().ProcessKeyExchange(std::move(msg));
					if (result.Handled && result.Success)
					{
						if (m_Peer.Send(MessageType::KeyUpdateReady, Buffer()))
						{
							result.Success = (m_Peer.GetKeyUpdate().SetStatus(KeyUpdate::Status::ReadyWait) &&
											  m_Peer.GetKeyUpdate().SetStatus(KeyUpdate::Status::UpdateWait));
						}
						else LogDbg(L"Couldn't send KeyUpdateReady message to peer %s",
									m_Peer.GetPeerName().c_str());
					}
				}

				break;
			}
			case MessageType::KeyUpdateReady:
			{
				if (m_Peer.GetKeyUpdate().GetStatus() == KeyUpdate::Status::ReadyWait &&
					m_Peer.GetConnectionType() == PeerConnectionType::Outbound)
				{
					result.Handled = true;

					if (auto& buffer = msg.GetMessageData(); buffer.IsEmpty())
					{
						// From now on we encrypt messages using the
						// secondary symmetric key-pair
						m_Peer.GetKeyExchange().StartUsingSecondarySymmetricKeyPairForEncryption();

						result.Success = m_Peer.GetKeyUpdate().SetStatus(KeyUpdate::Status::UpdateWait);
					}
					else LogDbg(L"Invalid KeyUpdateReady message from peer %s; no data expected",
								m_Peer.GetPeerName().c_str());
				}

				break;
			}
			default:
			{
				assert(false);
				break;
			}
		}

		return result;
	}
}