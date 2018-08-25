// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"
#include "Settings.h"
#include "Core\Access\IPAccessControl.h"

#include <thread>

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Core::Access;

namespace UnitTests
{
	TEST_CLASS(IPAccessControlTests)
	{
	public:
		TEST_METHOD(General)
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
			Assert::AreEqual(true, reps.SetReputation(ipaddr, IPAccessDetails::Reputation::Maximum + 1).Failed());

			Assert::AreEqual(true, reps.SetReputation(ipaddr, IPAccessDetails::Reputation::Base).Succeeded());

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
				Assert::AreEqual(IPAccessDetails::Reputation::Base, result->first);
			}

			{
				// Should not be able to improve reputation beyond maximum
				Assert::AreEqual(true, reps.SetReputation(ipaddr, IPAccessDetails::Reputation::Maximum).Succeeded());
				const auto result = reps.UpdateReputation(ipaddr,
														  IPReputationUpdate::ImproveMinimal);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(IPAccessDetails::Reputation::Maximum, result->first);

				Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr));
			}

			{
				// Reputation deterioration from maximum should result in reputation not being acceptable
				const auto result = reps.UpdateReputation(ipaddr,
														  IPReputationUpdate::DeteriorateSevere);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(static_cast<Int16>(-100), result->first);
				Assert::AreEqual(false, reps.HasAcceptableReputation(ipaddr));
			}

			// Two times moderate deterioration from maximum should result in reputation not being acceptable
			IPAddress ipaddr2(L"200.1.157.11");

			{
				const auto result = reps.UpdateReputation(ipaddr2,
														  IPReputationUpdate::DeteriorateModerate);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(static_cast<Int16>(50), result->first);
			}

			{
				const auto result = reps.UpdateReputation(ipaddr2,
														  IPReputationUpdate::DeteriorateModerate);
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(false, reps.HasAcceptableReputation(ipaddr2));
			}

			// Reputation should improve to acceptable in 6s
			std::this_thread::sleep_for(3s);
			Assert::AreEqual(false, reps.HasAcceptableReputation(ipaddr));
			Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr2));
			std::this_thread::sleep_for(3s);
			Assert::AreEqual(true, reps.HasAcceptableReputation(ipaddr));
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