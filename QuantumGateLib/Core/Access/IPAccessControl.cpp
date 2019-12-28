// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "IPAccessControl.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::Access
{
	IPAccessDetails::IPAccessDetails() noexcept
	{
		m_Reputation.LastImproveSteadyTime = Util::GetCurrentSteadyTime();
		m_ConnectionAttempts.LastResetSteadyTime = Util::GetCurrentSteadyTime();
		m_RelayConnectionAttempts.LastResetSteadyTime = Util::GetCurrentSteadyTime();
	}

	void IPAccessDetails::ImproveReputation(const std::chrono::seconds interval) noexcept
	{
		const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
			Util::GetCurrentSteadyTime() - m_Reputation.LastImproveSteadyTime);

		if (seconds >= interval)
		{
			Int64 factor{ 1 };
			if (interval.count() > 0)
			{
				factor = seconds.count() / interval.count();
			}

			Int64 new_score{ static_cast<Int64>(m_Reputation.Score) +
				(static_cast<Int64>(IPReputationUpdate::ImproveMinimal) * factor) };

			if (new_score > IPReputation::ScoreLimits::Maximum)
			{
				new_score = IPReputation::ScoreLimits::Maximum;
			}

			m_Reputation.Score = static_cast<Int16>(new_score);
			m_Reputation.LastImproveSteadyTime = Util::GetCurrentSteadyTime();
		}
	}

	bool IPAccessDetails::SetReputation(const Int16 score, const std::optional<Time>& time) noexcept
	{
		if (score < IPReputation::ScoreLimits::Minimum ||
			score > IPReputation::ScoreLimits::Maximum) return false;

		auto time_diff = 0ms;

		if (time.has_value())
		{
			const auto lutime = Util::ToTime(*time);
			const auto ctime = Util::GetCurrentSystemTime();

			// Last update time is in the future
			if (lutime > ctime) return false;

			time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(ctime - lutime);
		}

		m_Reputation.Score = score;
		m_Reputation.LastImproveSteadyTime = Util::GetCurrentSteadyTime() - time_diff;

		return true;
	}

	void IPAccessDetails::ResetReputation() noexcept
	{
		m_Reputation.Score = IPReputation::ScoreLimits::Maximum;
		m_Reputation.LastImproveSteadyTime = Util::GetCurrentSteadyTime();
	}

	const Int16 IPAccessDetails::UpdateReputation(const IPReputationUpdate rep_update) noexcept
	{
		m_Reputation.Score += static_cast<Int16>(rep_update);
		if (m_Reputation.Score > IPReputation::ScoreLimits::Maximum)
		{
			m_Reputation.Score = IPReputation::ScoreLimits::Maximum;
		}
		else if (m_Reputation.Score < IPReputation::ScoreLimits::Minimum)
		{
			m_Reputation.Score = IPReputation::ScoreLimits::Minimum;
		}

		return m_Reputation.Score;
	}

	const Int16 IPAccessDetails::UpdateReputation(const std::chrono::seconds interval,
												  const IPReputationUpdate rep_update) noexcept
	{
		ImproveReputation(interval);
		return UpdateReputation(rep_update);
	}

	const std::pair<Int16, Time> IPAccessDetails::GetReputation() const noexcept
	{
		// Elapsed time since last reputation update
		const auto tlru = std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() -
																				m_Reputation.LastImproveSteadyTime);
		return std::make_pair(m_Reputation.Score,
							  Util::ToTimeT(Util::GetCurrentSystemTime() - tlru));
	}

	bool IPAccessDetails::AddConnectionAttempt(ConnectionAttempts& attempts, const std::chrono::seconds interval,
											   const Size max_attempts) noexcept
	{
		auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
			Util::GetCurrentSteadyTime() - attempts.LastResetSteadyTime);

		// If enough time has passed reset the connection attempts
		// so that the IP gets a fresh amount of attempts for the next
		// time interval
		if (seconds >= interval)
		{
			attempts.Amount = 0;
			attempts.LastResetSteadyTime = Util::GetCurrentSteadyTime();
		}

		if (attempts.Amount < std::numeric_limits<Size>::max())
		{
			++attempts.Amount;
		}
		else return false;

		// If connection attempts go above maximum, update the
		// reputation for the IP address such that it will get
		// blocked eventually until the reputation has improved
		// again sufficiently
		if (attempts.Amount > max_attempts)
		{
			const auto rep = UpdateReputation(IPReputationUpdate::DeteriorateModerate);

			return IPAccessDetails::IsAcceptableReputation(rep);
		}

		return true;
	}

	IPAccessControl::IPAccessControl(const Settings_CThS& settings) noexcept :
		m_Settings(settings)
	{}

	Result<> IPAccessControl::SetReputation(const IPAddress& ip, const Int16 score,
											const std::optional<Time>& time) noexcept
	{
		auto ipad = GetIPAccessDetails(ip);
		if (ipad != nullptr)
		{
			if (ipad->SetReputation(score, time))
			{
				return ResultCode::Succeeded;
			}
		}

		return ResultCode::Failed;
	}

	Result<> IPAccessControl::ResetReputation(const IPAddress& ip) noexcept
	{
		auto ipad = GetIPAccessDetails(ip);
		if (ipad != nullptr)
		{
			ipad->ResetReputation();
			return ResultCode::Succeeded;
		}

		return ResultCode::AddressNotFound;
	}

	void IPAccessControl::ResetAllReputations() noexcept
	{
		for (auto& it : m_IPAccessDetails)
		{
			it.second.ResetReputation();
		}
	}

	Result<std::pair<Int16, bool>> IPAccessControl::UpdateReputation(const IPAddress& ip,
																	 const IPReputationUpdate rep_update) noexcept
	{
		const auto interval = m_Settings->Local.IPReputationImprovementInterval;

		auto ipad = GetIPAccessDetails(ip);
		if (ipad != nullptr)
		{
			auto rep = ipad->UpdateReputation(interval, rep_update);

			return std::make_pair(rep, IPAccessDetails::IsAcceptableReputation(rep));
		}

		return ResultCode::Failed;
	}

	bool IPAccessControl::HasAcceptableReputation(const IPAddress& ip) noexcept
	{
		const auto result = UpdateReputation(ip, IPReputationUpdate::None);
		if (result.Succeeded())
		{
			return result->second;
		}

		return false;
	}

	Result<Vector<IPReputation>> IPAccessControl::GetReputations() const noexcept
	{
		try
		{
			Vector<IPReputation> ipreputations;

			for (const auto& it : m_IPAccessDetails)
			{
				const auto[score, time] = it.second.GetReputation();

				auto& ipreputation = ipreputations.emplace_back();
				ipreputation.Address = it.first;
				ipreputation.Score = score;
				ipreputation.LastUpdateTime = time;
			}

			return std::move(ipreputations);
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	bool IPAccessControl::AddConnectionAttempt(const IPAddress& ip) noexcept
	{
		const auto& settings = m_Settings.GetCache();
		const auto interval = settings.Local.IPConnectionAttempts.Interval;
		const auto max_attempts = settings.Local.IPConnectionAttempts.MaxPerInterval;

		if (auto ipad = GetIPAccessDetails(ip); ipad != nullptr)
		{
			return ipad->AddConnectionAttempt(ipad->GetConnectionAttempts(), interval, max_attempts);
		}

		return false;
	}

	bool IPAccessControl::AddRelayConnectionAttempt(const IPAddress & ip) noexcept
	{
		const auto& settings = m_Settings.GetCache();
		const auto interval = settings.Relay.IPConnectionAttempts.Interval;
		const auto max_attempts = settings.Relay.IPConnectionAttempts.MaxPerInterval;

		if (auto ipad = GetIPAccessDetails(ip); ipad != nullptr)
		{
			return ipad->AddConnectionAttempt(ipad->GetRelayConnectionAttempts(), interval, max_attempts);
		}

		return false;
	}

	IPAccessDetails* IPAccessControl::GetIPAccessDetails(const IPAddress& ip) noexcept
	{
		try
		{
			const auto it = m_IPAccessDetails.find(ip.GetBinary());
			if (it != m_IPAccessDetails.end())
			{
				return &it->second;
			}
			else
			{
				const auto[it, success] = m_IPAccessDetails.insert({ ip.GetBinary(), IPAccessDetails() });
				if (success) return &it->second;
			}
		}
		catch (...) {}

		return nullptr;
	}
}