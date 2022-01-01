// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "AddressAccessControl.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::Access
{
	AddressAccessDetails::AddressAccessDetails() noexcept
	{
		m_Reputation.LastImproveSteadyTime = Util::GetCurrentSteadyTime();
		m_ConnectionAttempts.LastResetSteadyTime = Util::GetCurrentSteadyTime();
		m_RelayConnectionAttempts.LastResetSteadyTime = Util::GetCurrentSteadyTime();
	}

	void AddressAccessDetails::ImproveReputation(const std::chrono::seconds interval) noexcept
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
				(static_cast<Int64>(AddressReputationUpdate::ImproveMinimal) * factor) };

			if (new_score > AddressReputation::ScoreLimits::Maximum)
			{
				new_score = AddressReputation::ScoreLimits::Maximum;
			}

			m_Reputation.Score = static_cast<Int16>(new_score);
			m_Reputation.LastImproveSteadyTime = Util::GetCurrentSteadyTime();
		}
	}

	bool AddressAccessDetails::SetReputation(const Int16 score, const std::optional<Time>& time) noexcept
	{
		if (score < AddressReputation::ScoreLimits::Minimum ||
			score > AddressReputation::ScoreLimits::Maximum) return false;

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

	void AddressAccessDetails::ResetReputation() noexcept
	{
		m_Reputation.Score = AddressReputation::ScoreLimits::Maximum;
		m_Reputation.LastImproveSteadyTime = Util::GetCurrentSteadyTime();
	}

	Int16 AddressAccessDetails::UpdateReputation(const AddressReputationUpdate rep_update) noexcept
	{
		m_Reputation.Score += static_cast<Int16>(rep_update);
		if (m_Reputation.Score > AddressReputation::ScoreLimits::Maximum)
		{
			m_Reputation.Score = AddressReputation::ScoreLimits::Maximum;
		}
		else if (m_Reputation.Score < AddressReputation::ScoreLimits::Minimum)
		{
			m_Reputation.Score = AddressReputation::ScoreLimits::Minimum;
		}

		return m_Reputation.Score;
	}

	Int16 AddressAccessDetails::UpdateReputation(const std::chrono::seconds interval,
												 const AddressReputationUpdate rep_update) noexcept
	{
		ImproveReputation(interval);
		return UpdateReputation(rep_update);
	}

	std::pair<Int16, Time> AddressAccessDetails::GetReputation() const noexcept
	{
		// Elapsed time since last reputation update
		const auto tlru = std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() -
																				m_Reputation.LastImproveSteadyTime);
		return std::make_pair(m_Reputation.Score,
							  Util::ToTimeT(Util::GetCurrentSystemTime() - tlru));
	}

	bool AddressAccessDetails::AddConnectionAttempt(ConnectionAttempts& attempts, const std::chrono::seconds interval,
													const Size max_attempts) noexcept
	{
		auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
			Util::GetCurrentSteadyTime() - attempts.LastResetSteadyTime);

		// If enough time has passed reset the connection attempts
		// so that the address gets a fresh amount of attempts for the next
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
		// reputation for the address such that it will get
		// blocked eventually until the reputation has improved
		// again sufficiently
		if (attempts.Amount > max_attempts)
		{
			const auto rep = UpdateReputation(AddressReputationUpdate::DeteriorateModerate);

			return AddressAccessDetails::IsAcceptableReputation(rep);
		}

		return true;
	}

	AddressAccessControl::AddressAccessControl(const Settings_CThS& settings) noexcept :
		m_Settings(settings)
	{}

	Result<> AddressAccessControl::SetReputation(const Address& addr, const Int16 score,
												 const std::optional<Time>& time) noexcept
	{
		auto aad = GetAddressAccessDetails(addr);
		if (aad != nullptr)
		{
			if (aad->SetReputation(score, time))
			{
				return ResultCode::Succeeded;
			}
		}

		return ResultCode::Failed;
	}

	Result<> AddressAccessControl::ResetReputation(const Address& addr) noexcept
	{
		auto aad = GetAddressAccessDetails(addr);
		if (aad != nullptr)
		{
			aad->ResetReputation();
			return ResultCode::Succeeded;
		}

		return ResultCode::AddressNotFound;
	}

	void AddressAccessControl::ResetAllReputations() noexcept
	{
		for (auto& it : m_AddressAccessDetails)
		{
			it.second.ResetReputation();
		}
	}

	Result<std::pair<Int16, bool>> AddressAccessControl::UpdateReputation(const Address& addr,
																		  const AddressReputationUpdate rep_update) noexcept
	{
		const auto interval = m_Settings->Local.AddressReputationImprovementInterval;

		auto aad = GetAddressAccessDetails(addr);
		if (aad != nullptr)
		{
			auto rep = aad->UpdateReputation(interval, rep_update);

			return std::make_pair(rep, AddressAccessDetails::IsAcceptableReputation(rep));
		}

		return ResultCode::Failed;
	}

	bool AddressAccessControl::HasAcceptableReputation(const Address& addr) noexcept
	{
		const auto result = UpdateReputation(addr, AddressReputationUpdate::None);
		if (result.Succeeded())
		{
			return result->second;
		}

		return false;
	}

	Result<Vector<AddressReputation>> AddressAccessControl::GetReputations() const noexcept
	{
		try
		{
			Vector<AddressReputation> addr_reps;

			for (const auto& it : m_AddressAccessDetails)
			{
				const auto [score, time] = it.second.GetReputation();

				auto& addr_rep = addr_reps.emplace_back();
				addr_rep.Address = it.first;
				addr_rep.Score = score;
				addr_rep.LastUpdateTime = time;
			}

			return std::move(addr_reps);
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	bool AddressAccessControl::AddConnectionAttempt(const Address& addr) noexcept
	{
		const auto& settings = m_Settings.GetCache();
		const auto interval = settings.Local.ConnectionAttempts.Interval;
		const auto max_attempts = settings.Local.ConnectionAttempts.MaxPerInterval;

		if (auto aad = GetAddressAccessDetails(addr); aad != nullptr)
		{
			return aad->AddConnectionAttempt(aad->GetConnectionAttempts(), interval, max_attempts);
		}

		return false;
	}

	bool AddressAccessControl::AddRelayConnectionAttempt(const Address& addr) noexcept
	{
		const auto& settings = m_Settings.GetCache();
		const auto interval = settings.Relay.ConnectionAttempts.Interval;
		const auto max_attempts = settings.Relay.ConnectionAttempts.MaxPerInterval;

		if (auto aad = GetAddressAccessDetails(addr); aad != nullptr)
		{
			return aad->AddConnectionAttempt(aad->GetRelayConnectionAttempts(), interval, max_attempts);
		}

		return false;
	}

	AddressAccessDetails* AddressAccessControl::GetAddressAccessDetails(const Address& addr) noexcept
	{
		try
		{
			const auto it = m_AddressAccessDetails.find(addr);
			if (it != m_AddressAccessDetails.end())
			{
				return &it->second;
			}
			else
			{
				const auto [it, success] = m_AddressAccessDetails.insert({ addr, AddressAccessDetails() });
				if (success) return &it->second;
			}
		}
		catch (...) {}

		return nullptr;
	}
}