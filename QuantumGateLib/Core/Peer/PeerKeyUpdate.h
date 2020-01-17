// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core::Peer
{
	class Peer;

	class KeyUpdate final
	{
	public:
		enum class Status
		{
			Unknown, UpdateWait, PrimaryExchange, SecondaryExchange, ReadyWait
		};

		KeyUpdate(Peer& peer) noexcept : m_Peer(peer) {}
		KeyUpdate(const KeyUpdate&) = delete;
		KeyUpdate(KeyUpdate&&) noexcept = default;
		~KeyUpdate() = default;
		KeyUpdate& operator=(const KeyUpdate&) = delete;
		KeyUpdate& operator=(KeyUpdate&&) noexcept = default;

		[[nodiscard]] bool BeginKeyUpdate() noexcept;

		inline Status GetStatus() const noexcept { return m_Status; }
		[[nodiscard]] bool SetStatus(const Status status) noexcept;
		
		[[nodiscard]] inline bool HasEvents() noexcept { return ShouldUpdate() || UpdateTimedOut(); }

		[[nodiscard]] bool UpdateTimedOut() const noexcept;
		[[nodiscard]] bool ShouldUpdate() noexcept;
		
		[[nodiscard]] inline bool IsUpdating() const noexcept
		{
			return (GetStatus() == Status::PrimaryExchange || GetStatus() == Status::SecondaryExchange);
		}

	private:
		void EndKeyUpdate() noexcept;

	private:
		Peer& m_Peer;
		Status m_Status{ Status::Unknown };
		SteadyTime m_UpdateSteadyTime;
		std::chrono::seconds m_UpdateInterval{ 0 };
	};
}