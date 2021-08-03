// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerMessageProcessor.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Peer;

	class KeyUpdate final
	{
	public:
		enum class Status
		{
			Unknown, UpdateWait, PrimaryExchange, SecondaryExchange, ReadyWait, Suspended
		};

		KeyUpdate(Peer& peer) noexcept : m_Peer(peer) {}
		KeyUpdate(const KeyUpdate&) = delete;
		KeyUpdate(KeyUpdate&&) noexcept = default;
		~KeyUpdate() = default;
		KeyUpdate& operator=(const KeyUpdate&) = delete;
		KeyUpdate& operator=(KeyUpdate&&) noexcept = default;

		[[nodiscard]] inline bool Initialize() noexcept { return SetStatus(KeyUpdate::Status::UpdateWait); }

		[[nodiscard]] inline bool HasEvents(const SteadyTime current_steadytime) noexcept
		{
			// No events while suspended
			if (GetStatus() == Status::Suspended) return false;

			return ShouldUpdate(current_steadytime) || UpdateTimedOut(current_steadytime);
		}

		[[nodiscard]] bool ProcessEvents(const SteadyTime current_steadytime) noexcept;
		[[nodiscard]] MessageProcessor::Result ProcessKeyUpdateMessage(MessageDetails&& msg) noexcept;

		[[nodiscard]] bool  Suspend() noexcept;
		[[nodiscard]] bool  Resume() noexcept;

	private:
		[[nodiscard]] bool SetStatus(const Status status) noexcept;
		inline Status GetStatus() const noexcept { return m_Status; }

		[[nodiscard]] bool BeginKeyUpdate() noexcept;
		void EndKeyUpdate() noexcept;

		[[nodiscard]] bool UpdateTimedOut(const SteadyTime current_steadytime) const noexcept;
		[[nodiscard]] bool ShouldUpdate(const SteadyTime current_steadytime) noexcept;

	private:
		Peer& m_Peer;
		Status m_Status{ Status::Unknown };
		SteadyTime m_UpdateSteadyTime;
		std::chrono::seconds m_UpdateInterval{ 0 };
		Status m_ResumeStatus{ Status::Unknown };
		std::chrono::seconds m_ResumeUpdateIntervalDelta{ 0 };
	};
}