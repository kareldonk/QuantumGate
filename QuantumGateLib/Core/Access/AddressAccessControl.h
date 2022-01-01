// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\API\Access.h"
#include "..\..\Common\Containers.h"
#include "..\..\Network\Address.h"

namespace QuantumGate::Implementation::Core::Access
{
	using namespace QuantumGate::API::Access;

	enum class AddressReputationUpdate : Int16
	{
		None = 0,
		ImproveMinimal = 20,
		DeteriorateMinimal = -20,
		DeteriorateModerate = -50,
		DeteriorateSevere = -200
	};

	class AddressAccessDetails final
	{
		struct Reputation final
		{
			Int16 Score{ AddressReputation::ScoreLimits::Maximum };
			SteadyTime LastImproveSteadyTime;
		};

		struct ConnectionAttempts final
		{
			Size Amount{ 0 };
			SteadyTime LastResetSteadyTime;
		};

	public:
		AddressAccessDetails() noexcept;
		AddressAccessDetails(const AddressAccessDetails&) = delete;
		AddressAccessDetails(AddressAccessDetails&&) noexcept = default;
		~AddressAccessDetails() = default;
		AddressAccessDetails& operator=(const AddressAccessDetails&) = delete;
		AddressAccessDetails& operator=(AddressAccessDetails&&) noexcept = default;

		[[nodiscard]] bool SetReputation(const Int16 score, const std::optional<Time>& time = std::nullopt) noexcept;
		void ResetReputation() noexcept;
		Int16 UpdateReputation(const std::chrono::seconds interval, const AddressReputationUpdate rep_update) noexcept;
		[[nodiscard]] std::pair<Int16, Time> GetReputation() const noexcept;

		inline ConnectionAttempts& GetConnectionAttempts() noexcept { return m_ConnectionAttempts; }
		inline ConnectionAttempts& GetRelayConnectionAttempts() noexcept { return m_RelayConnectionAttempts; }

		[[nodiscard]] bool AddConnectionAttempt(ConnectionAttempts& attempts, const std::chrono::seconds interval,
												const Size max_attempts) noexcept;

		[[nodiscard]] inline static constexpr bool IsAcceptableReputation(const Int16 score) noexcept
		{
			return (score > AddressReputation::ScoreLimits::Base);
		}

	private:
		void ImproveReputation(const std::chrono::seconds interval) noexcept;
		Int16 UpdateReputation(const AddressReputationUpdate rep_update) noexcept;

	private:
		Reputation m_Reputation;

		ConnectionAttempts m_ConnectionAttempts;
		ConnectionAttempts m_RelayConnectionAttempts;
	};

	using AddressAccessDetailsMap = Containers::UnorderedMap<Address, AddressAccessDetails>;

	class AddressAccessControl final
	{
	public:
		AddressAccessControl() = delete;
		AddressAccessControl(const Settings_CThS& settings) noexcept;
		~AddressAccessControl() = default;
		AddressAccessControl(const AddressAccessControl&) = delete;
		AddressAccessControl(AddressAccessControl&&) noexcept = default;
		AddressAccessControl& operator=(const AddressAccessControl&) = delete;
		AddressAccessControl& operator=(AddressAccessControl&&) noexcept = default;

		Result<> SetReputation(const Address& addrp, const Int16 score,
							   const std::optional<Time>& time = std::nullopt) noexcept;
		Result<> ResetReputation(const Address& addr) noexcept;
		void ResetAllReputations() noexcept;
		Result<std::pair<Int16, bool>> UpdateReputation(const Address& addr,
														const AddressReputationUpdate rep_update) noexcept;
		[[nodiscard]] bool HasAcceptableReputation(const Address& addr) noexcept;

		Result<Vector<AddressReputation>> GetReputations() const noexcept;

		[[nodiscard]] bool AddConnectionAttempt(const Address& addrp) noexcept;
		[[nodiscard]] bool AddRelayConnectionAttempt(const Address& addr) noexcept;

	private:
		AddressAccessDetails* GetAddressAccessDetails(const Address& addr) noexcept;

	private:
		const Settings_CThS& m_Settings;

		AddressAccessDetailsMap m_AddressAccessDetails;
	};

	using AddressAccessControl_ThS = Concurrency::ThreadSafe<AddressAccessControl, std::shared_mutex>;
}