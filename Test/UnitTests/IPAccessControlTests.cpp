// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"
#include "Settings.h"
#include "Core\Access\IPAccessControl.h"
#include "Common\Util.h"

#include <thread>

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation;
using namespace QuantumGate::Implementation::Core::Access;

namespace UnitTests
{
	TEST_CLASS(IPAccessControlTests)
	{
	public:
		TEST_METHOD(ReputationGeneral)
		{
			Settings_CThS settings;
			settings.UpdateValue([](Settings& set)
			{
				// For testing we let reputation improve every second
				set.Local.IPReputationImprovementInterval = 1s;
			});

			IPAccessControl reps(settings);
			IPAddress ipaddr(L"192.168.1.10");

			// New address should have good reputation
			Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr));

			// Should not be able to set reputation above maximum
			Assert::AreEqual(true, reps.SetReputation(ipaddr, IPReputation::ScoreLimits::Maximum + 1).Failed());

			// Should not be able to set reputation below minimum
			Assert::AreEqual(true, reps.SetReputation(ipaddr, IPReputation::ScoreLimits::Minimum - 1).Failed());

			Assert::AreEqual(true, reps.SetReputation(ipaddr, IPReputation::ScoreLimits::Base).Succeeded());

			// Base reputation is not acceptable
			Assert::AreEqual(false, reps.HasAcceptableReputation(ipaddr));

			{
				// Base reputation with minimal improvement should be acceptable
				const auto result = reps.UpdateReputation(ipaddr,
														  IPReputationUpdate::ImproveMinimal);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr));
			}

			{
				// Minimal deterioration brings reputation back to base value
				const auto result = reps.UpdateReputation(ipaddr,
														  IPReputationUpdate::DeteriorateMinimal);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(IPReputation::ScoreLimits::Base, result->first);
			}

			{
				// Should not be able to improve reputation beyond maximum
				Assert::AreEqual(true, reps.SetReputation(ipaddr, IPReputation::ScoreLimits::Maximum).Succeeded());
				const auto result = reps.UpdateReputation(ipaddr,
														  IPReputationUpdate::ImproveMinimal);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(IPReputation::ScoreLimits::Maximum, result->first);

				Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr));
			}

			{
				// Reputation deterioration from maximum should result in reputation not being acceptable
				const auto result = reps.UpdateReputation(ipaddr,
														  IPReputationUpdate::DeteriorateSevere);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(false, reps.HasAcceptableReputation(ipaddr));
			}

			// Two times moderate deterioration from maximum should result in reputation not being acceptable
			IPAddress ipaddr2(L"200.1.157.11");

			{
				const auto result = reps.UpdateReputation(ipaddr2,
														  IPReputationUpdate::DeteriorateModerate);
				Assert::AreEqual(true, result.Succeeded());
			}

			{
				const auto result = reps.UpdateReputation(ipaddr2,
														  IPReputationUpdate::DeteriorateModerate);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(false, reps.HasAcceptableReputation(ipaddr2));
			}

			// Reputations should improve to acceptable in 6s
			std::this_thread::sleep_for(3s);
			Assert::AreEqual(false, reps.HasAcceptableReputation(ipaddr));
			Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr2));
			std::this_thread::sleep_for(3s);
			Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr));
		}

		TEST_METHOD(ReputationWithTime)
		{
			Settings_CThS settings;
			settings.UpdateValue([](Settings& set)
			{
				// For testing we let reputation improve every second
				set.Local.IPReputationImprovementInterval = 1s;
			});

			IPAccessControl reps(settings);

			{
				IPAddress ipaddr(L"200.1.157.11");
				auto score{ -100 };

				// How many seconds needed to get to base reputation?
				auto secs = std::chrono::seconds(std::abs(score /
														  static_cast<Int16>(IPReputationUpdate::ImproveMinimal)));
				secs += 1s;

				auto lutime = Util::ToTimeT(Util::GetCurrentSystemTime() - secs);

				Assert::AreEqual(true, reps.SetReputation(ipaddr, score, lutime).Succeeded());

				// Since reputation improves every second, it should now
				// have gone to above the base reputation score
				Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr));

				{
					const auto result = reps.UpdateReputation(ipaddr, IPReputationUpdate::None);
					Assert::AreEqual(true, result.Succeeded());
					Assert::AreEqual(true, result->first < IPReputation::ScoreLimits::Maximum);
				}

				{
					// Reset to full positive reputation score
					Assert::AreEqual(true, reps.ResetReputation(ipaddr).Succeeded());

					const auto result = reps.UpdateReputation(ipaddr, IPReputationUpdate::None);
					Assert::AreEqual(true, result.Succeeded());
					Assert::AreEqual(true, result->first == IPReputation::ScoreLimits::Maximum);
				}
			}

			{
				IPAddress ipaddr(L"200.1.157.22");
				auto score{ -200 };

				// How many seconds needed to get to base reputation?
				auto secs = std::chrono::seconds(std::abs(score /
														  static_cast<Int16>(IPReputationUpdate::ImproveMinimal)));

				auto lutime = Util::ToTimeT(Util::GetCurrentSystemTime() - secs);

				Assert::AreEqual(true, reps.SetReputation(ipaddr, score, lutime).Succeeded());

				// Since reputation improves every second, it should now
				// be equal to base reputation score
				Assert::AreEqual(false, reps.HasAcceptableReputation(ipaddr));

				std::this_thread::sleep_for(1s);
				Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr));
			}

			{
				IPAddress ipaddr(L"200.1.157.33");
				auto score{ -200 };

				// How many seconds needed to get to base reputation?
				auto secs1 = std::chrono::seconds(std::abs(score /
														   static_cast<Int16>(IPReputationUpdate::ImproveMinimal)));

				// How many seconds needed to get to max reputation?
				auto secs2 = std::chrono::seconds(IPReputation::ScoreLimits::Maximum /
												  static_cast<Int16>(IPReputationUpdate::ImproveMinimal));

				auto lutime = Util::ToTimeT(Util::GetCurrentSystemTime() - (secs1 + secs2 + 10s));

				Assert::AreEqual(true, reps.SetReputation(ipaddr, score, lutime).Succeeded());

				// Since reputation improves every second, it should now
				// be at the maximum reputation score
				Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr));
				const auto result = reps.UpdateReputation(ipaddr, IPReputationUpdate::None);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(true, result->first == IPReputation::ScoreLimits::Maximum);
				Assert::AreEqual(true, result->second);
			}

			{
				IPAddress ipaddr(L"200.1.157.44");
				auto score{ 50 };
				auto lutime = Util::ToTimeT(Util::GetCurrentSystemTime() + 2s);

				// Trying to set reputation with last update time in the future should fail
				Assert::AreEqual(false, reps.SetReputation(ipaddr, score, lutime).Succeeded());
			}

			{
				const auto result = reps.GetReputations();
				Assert::AreEqual(true, result.Succeeded());

				// Should have 4 items
				Assert::AreEqual(true, result->size() == 4);

				for (const auto& rep : result.GetValue())
				{
					const auto result2 = reps.UpdateReputation(rep.Address,
															   IPReputationUpdate::DeteriorateSevere);
					Assert::AreEqual(true, result2.Succeeded());
					Assert::AreEqual(false, reps.HasAcceptableReputation(rep.Address));
				}

				// Reset all reputations to maximum score
				reps.ResetAllReputations();

				for (const auto& rep : result.GetValue())
				{
					Assert::AreEqual(true, reps.HasAcceptableReputation(rep.Address));
					const auto result2 = reps.UpdateReputation(rep.Address, IPReputationUpdate::None);
					Assert::AreEqual(true, result2.Succeeded());
					Assert::AreEqual(true, result2->first == IPReputation::ScoreLimits::Maximum);
				}
			}
		}

		TEST_METHOD(ConnectionAttempts)
		{
			Settings_CThS settings;
			settings.UpdateValue([](Settings& set)
			{
				set.Local.IPConnectionAttempts.MaxPerInterval = 2;
				set.Local.IPConnectionAttempts.Interval = 3s;
			});

			IPAccessControl ac(settings);

			// Connections should be accepted
			{
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.10")));
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.10")));
				std::this_thread::sleep_for(4s);
			}

			// Connections should be accepted after 4 seconds when number
			// of attempts are reset
			{
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.10")));
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.10")));
				std::this_thread::sleep_for(4s);
			}

			// Connections should be accepted after 4 seconds when number
			// of attempts are reset
			{
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.10")));
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.10")));

				// Will be accepted but reputation will go down
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.10")));

				// Blocked
				Assert::AreEqual(false, ac.AddConnectionAttempt(IPAddress(L"192.168.1.10")));

				// Other IP should be accepted
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.11")));
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.11")));

				// Will be accepted but reputation will go down
				Assert::AreEqual(true, ac.AddConnectionAttempt(IPAddress(L"192.168.1.11")));

				// Blocked
				Assert::AreEqual(false, ac.AddConnectionAttempt(IPAddress(L"192.168.1.11")));
			}
		}

		TEST_METHOD(RelayConnectionAttempts)
		{
			Settings_CThS settings;
			settings.UpdateValue([](Settings& set)
			{
				set.Relay.IPConnectionAttempts.MaxPerInterval = 2;
				set.Relay.IPConnectionAttempts.Interval = 3s;
			});

			IPAccessControl ac(settings);

			// Connections should be accepted
			{
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.10")));
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.10")));
				std::this_thread::sleep_for(4s);
			}

			// Connections should be accepted after 4 seconds when number
			// of attempts are reset
			{
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.10")));
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.10")));
				std::this_thread::sleep_for(4s);
			}

			// Connections should be accepted after 4 seconds when number
			// of attempts are reset
			{
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.10")));
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.10")));

				// Will be accepted but reputation will go down
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.10")));

				// Blocked
				Assert::AreEqual(false, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.10")));

				// Other IP should be accepted
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.11")));
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.11")));

				// Will be accepted but reputation will go down
				Assert::AreEqual(true, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.11")));

				// Blocked
				Assert::AreEqual(false, ac.AddRelayConnectionAttempt(IPAddress(L"192.168.1.11")));
			}
		}
	};
}