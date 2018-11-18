// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Network\IPAddress.h"

#include <unordered_map>

namespace QuantumGate::Implementation::Core::Access
{
	enum class IPReputationUpdate : Int16
	{
		None = 0,
		ImproveMinimal = 20,
		DeteriorateMinimal = -20,
		DeteriorateModerate = -50,
		DeteriorateSevere = -200
	};

	class IPAccessDetails final
	{
		struct Reputation final
		{
			Int16 Score{ IPReputation::ScoreLimits::Maximum };
			SteadyTime LastImproveSteadyTime;
		};

		struct ConnectionAttempts final
		{
			Size Amount{ 0 };
			SteadyTime LastResetSteadyTime;
		};

	public:
		IPAccessDetails() noexcept;
		IPAccessDetails(const IPAccessDetails&) = delete;
		IPAccessDetails(IPAccessDetails&&) = default;
		~IPAccessDetails() = default;
		IPAccessDetails& operator=(const IPAccessDetails&) = delete;
		IPAccessDetails& operator=(IPAccessDetails&&) = default;

		[[nodiscard]] const bool SetReputation(const Int16 score,
											   const std::optional<Time>& time = std::nullopt) noexcept;
		void ResetReputation() noexcept;
		const Int16 UpdateReputation(const std::chrono::seconds interval, const IPReputationUpdate rep_update) noexcept;
		[[nodiscard]] const std::pair<Int16, Time> GetReputation() const noexcept;

		inline ConnectionAttempts& GetConnectionAttempts() noexcept { return m_ConnectionAttempts; }
		inline ConnectionAttempts& GetRelayConnectionAttempts() noexcept { return m_RelayConnectionAttempts; }

		[[nodiscard]] const bool AddConnectionAttempt(ConnectionAttempts& attempts, const std::chrono::seconds interval,
													  const Size max_attempts) noexcept;

		[[nodiscard]] inline static constexpr const bool IsAcceptableReputation(const Int16 score) noexcept { return (score > IPReputation::ScoreLimits::Base); }

	private:
		void ImproveReputation(const std::chrono::seconds interval) noexcept;
		const Int16 UpdateReputation(const IPReputationUpdate rep_update) noexcept;

	private:
		Reputation m_Reputation;

		ConnectionAttempts m_ConnectionAttempts;
		ConnectionAttempts m_RelayConnectionAttempts;
	};

	using IPAccessDetailsMap = std::unordered_map<Network::BinaryIPAddress, IPAccessDetails>;

	class IPAccessControl final
	{
	public:
		IPAccessControl() = delete;
		IPAccessControl(const Settings_CThS& settings) noexcept;
		~IPAccessControl() = default;
		IPAccessControl(const IPAccessControl&) = delete;
		IPAccessControl(IPAccessControl&&) = default;
		IPAccessControl& operator=(const IPAccessControl&) = delete;
		IPAccessControl& operator=(IPAccessControl&&) = default;

		Result<> SetReputation(const IPAddress& ip, const Int16 score,
							   const std::optional<Time>& time = std::nullopt) noexcept;
		Result<> ResetReputation(const IPAddress& ip) noexcept;
		void ResetAllReputations() noexcept;
		Result<std::pair<Int16, bool>> UpdateReputation(const IPAddress& ip,
														const IPReputationUpdate rep_update) noexcept;
		[[nodiscard]] const bool HasAcceptableReputation(const IPAddress& ip) noexcept;

		Result<Vector<IPReputation>> GetReputations() const noexcept;

		[[nodiscard]] const bool AddConnectionAttempt(const IPAddress& ip) noexcept;
		[[nodiscard]] const bool AddRelayConnectionAttempt(const IPAddress& ip) noexcept;

	private:
		IPAccessDetails* GetIPAccessDetails(const IPAddress& ip) noexcept;

	private:
		const Settings_CThS& m_Settings;

		IPAccessDetailsMap m_IPAccessDetails;
	};

	using IPAccessControl_ThS = Concurrency::ThreadSafe<IPAccessControl, std::shared_mutex>;
}