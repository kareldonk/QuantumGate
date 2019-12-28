// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Settings.h"
#include "Core\PublicIPEndpoints.h"
#include "Common\Util.h"

#include <thread>

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Core;

bool CheckIPs(const Vector<BinaryIPAddress>& ips, const std::vector<BinaryIPAddress>& exp_ips)
{
	for (const auto& ip : ips)
	{
		const auto it = std::find_if(exp_ips.begin(), exp_ips.end(),
									 [&](const auto& value)
		{
			return value == ip;
		});
		if (it == exp_ips.end()) return false;
	}

	for (const auto& exp_ip : exp_ips)
	{
		const auto it = std::find(ips.begin(), ips.end(), exp_ip);
		if (it == ips.end()) return false;
	}

	return true;
}

bool CheckIPs(const PublicIPEndpoints& pubendp, const std::vector<BinaryIPAddress>& exp_ips)
{
	Vector<BinaryIPAddress> pub_ips;
	const auto result = pubendp.AddIPAddresses(pub_ips, false);
	if (!result.Succeeded()) return false;

	return CheckIPs(pub_ips, exp_ips);
}

bool RemoveIP(std::vector<BinaryIPAddress>& list, const BinaryIPAddress& ip)
{
	const auto it = std::find(list.begin(), list.end(), ip);
	if (it != list.end())
	{
		list.erase(it);
		return true;
	}

	return false;
}

namespace UnitTests
{
	TEST_CLASS(PublicIPEndpointsTests)
	{
	public:
		TEST_METHOD(General)
		{
			struct TestCases
			{
				IPEndpoint PublicIPEndpoint;
				IPEndpoint ReportingPeer;
				PeerConnectionType ConnectionType{ PeerConnectionType::Unknown };
				bool Trusted{ false };
				bool Success{ false };
				std::pair<bool, bool> Result;
			};

			const std::vector<TestCases> tests
			{
				{
					IPEndpoint(IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPAddress(L"172.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, true)
				},

				{
					// Should fail because of unknown connection type
					IPEndpoint(IPAddress(L"160.16.5.51"), 999),
					IPEndpoint(IPAddress(L"210.21.117.42"), 7000),
					PeerConnectionType::Unknown,
					false, false, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"160.16.5.51"), 999),
					IPEndpoint(IPAddress(L"210.21.117.42"), 7000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"), 9000),
					IPEndpoint(IPAddress(L"e835:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, true)
				},

				{
					// Should fail because of different IP address types
					IPEndpoint(IPAddress(L"160.16.5.51"), 9000),
					IPEndpoint(IPAddress(L"e825:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					PeerConnectionType::Inbound,
					false, false, std::make_pair(false, false)
				},

				{
					// Should fail because of different IP address types
					IPEndpoint(IPAddress(L"e825:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					IPEndpoint(IPAddress(L"160.16.5.51"), 9000),
					PeerConnectionType::Outbound,
					false, false, std::make_pair(false, false)
				},

				{
					// Should get accepted but not a new address because 160.16.5.51
					// was already added previously; port will get added
					IPEndpoint(IPAddress(L"160.16.5.51"), 3333),
					IPEndpoint(IPAddress(L"83.21.117.20"), 4500),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, false)
				},

				{
					// Should not get accepted because reporting IP 210.21.117.20 is on
					// same /16 network as previous reporting IP 210.21.117.42
					IPEndpoint(IPAddress(L"120.16.115.51"), 999),
					IPEndpoint(IPAddress(L"210.21.117.20"), 7000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(false, false)
				},

				{
					// Should not get accepted because reporting IP 210.21.217.42 is on
					// same /16 network as previous reporting IP 210.21.117.42
					IPEndpoint(IPAddress(L"170.216.5.51"), 999),
					IPEndpoint(IPAddress(L"210.21.217.42"), 7000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(false, false)
				},

				{
					// Should not get accepted because reporting IP e835:625f:48ce:c333:: is on
					// same /48 network as previous reporting IP e835:625f:48ce:c433:7c5d:ea3:76c3:ca0
					IPEndpoint(IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"), 999),
					IPEndpoint(IPAddress(L"e835:625f:48ce:c333::"), 2100),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(false, false)
				},

				{
					// Should get accepted now because even though reporting IP e835:625f:48ce:c333:: is on
					// same /48 network as previous reporting IP e835:625f:48ce:c433:7c5d:ea3:76c3:ca0,
					// this is from a trusted peer
					IPEndpoint(IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"), 999),
					IPEndpoint(IPAddress(L"e835:625f:48ce:c333::"), 2100),
					PeerConnectionType::Inbound,
					true, true, std::make_pair(true, true)
				},

				{
					// Outgoing connection won't get port added
					IPEndpoint(IPAddress(L"199.111.110.30"), 6666),
					IPEndpoint(IPAddress(L"120.221.17.2"), 8000),
					PeerConnectionType::Outbound,
					true, true, std::make_pair(true, true)
				}
			};

			Settings_CThS settings;

			PublicIPEndpoints pubendp{ settings };
			Assert::AreEqual(false, pubendp.IsInitialized());
			Assert::AreEqual(true, pubendp.Initialize());
			Assert::AreEqual(true, pubendp.IsInitialized());

			for (const auto& test : tests)
			{
				const auto result = pubendp.AddIPEndpoint(test.PublicIPEndpoint, test.ReportingPeer,
														  test.ConnectionType, test.Trusted);
				Assert::AreEqual(test.Success, result.Succeeded());
				if (result.Succeeded())
				{
					Assert::AreEqual(test.Result.first, result->first);
					Assert::AreEqual(test.Result.second, result->second);
				}
			}

			{
				struct ExpectedIP
				{
					BinaryIPAddress IPAddress;
					bool Trusted{ false };
					Set<UInt16> Ports;
					Size NumReportingPeerNetworks{ 0 };
				};

				std::vector<ExpectedIP> expected_ips
				{
					{ IPAddress(L"200.168.5.51").GetBinary(), false, { 999 }, 1 },
					{ IPAddress(L"160.16.5.51").GetBinary(), false, { 999, 3333 }, 2 },
					{ IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193").GetBinary(), false, { 9000 }, 1 },
					{ IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de").GetBinary(), true, { 999 }, 1 },
					{ IPAddress(L"199.111.110.30").GetBinary(), true, {}, 1 }
				};

				// Check that we got back the expected IPs
				{
					Vector<BinaryIPAddress> pub_ips;
					const auto result = pubendp.AddIPAddresses(pub_ips, false);
					Assert::AreEqual(true, result.Succeeded());

					for (const auto& ip : pub_ips)
					{
						const auto it = std::find_if(expected_ips.begin(), expected_ips.end(),
													 [&](const auto& value)
						{
							return value.IPAddress == ip;
						});
						Assert::AreEqual(true, it != expected_ips.end());
					}

					for (const auto& exp_details : expected_ips)
					{
						const auto it = std::find(pub_ips.begin(), pub_ips.end(), exp_details.IPAddress);
						Assert::AreEqual(true, it != pub_ips.end());
					}
				}

				// Check that the IPEndpoint details are what we expect
				{
					auto endpoints = pubendp.GetIPEndpoints().WithSharedLock();

					for (const auto& exp_details : expected_ips)
					{
						const auto it2 = endpoints->find(exp_details.IPAddress);
						Assert::AreEqual(true, it2 != endpoints->end());

						Assert::AreEqual(true, it2->second.Trusted == exp_details.Trusted);
						Assert::AreEqual(true, it2->second.ReportingPeerNetworkHashes.size() ==
										 exp_details.NumReportingPeerNetworks);
						Assert::AreEqual(true, it2->second.Ports == exp_details.Ports);
					}
				}

				pubendp.Deinitialize();
				Assert::AreEqual(false, pubendp.IsInitialized());
			}
		}

		TEST_METHOD(RemoveLeastRelevantIPEndpoints)
		{
			struct TestCases
			{
				IPEndpoint PublicIPEndpoint;
				IPEndpoint ReportingPeer;
				PeerConnectionType ConnectionType{ PeerConnectionType::Unknown };
				bool Trusted{ false };
				bool Verified{ false };
				bool Success{ false };
				std::pair<bool, bool> Result;
			};

			const std::vector<TestCases> tests
			{
				{
					IPEndpoint(IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPAddress(L"172.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPAddress(L"173.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPAddress(L"174.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"), 999),
					IPEndpoint(IPAddress(L"e835:625f:48ce:c333::"), 2100),
					PeerConnectionType::Inbound,
					true, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"160.16.5.51"), 999),
					IPEndpoint(IPAddress(L"210.21.117.42"), 7000),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"), 9000),
					IPEndpoint(IPAddress(L"e845:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"160.16.5.51"), 3333),
					IPEndpoint(IPAddress(L"83.21.117.20"), 4500),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPAddress(L"199.111.110.30"), 6666),
					IPEndpoint(IPAddress(L"120.221.17.2"), 8000),
					PeerConnectionType::Outbound,
					true, false, true, std::make_pair(true, true)
				}
			};

			Settings_CThS settings;

			PublicIPEndpoints pubendp{ settings };
			Assert::AreEqual(true, pubendp.Initialize());

			for (const auto& test : tests)
			{
				const auto result = pubendp.AddIPEndpoint(test.PublicIPEndpoint, test.ReportingPeer,
														  test.ConnectionType, test.Trusted, test.Verified);
				Assert::AreEqual(test.Success, result.Succeeded());
				if (result.Succeeded())
				{
					Assert::AreEqual(test.Result.first, result->first);
					Assert::AreEqual(test.Result.second, result->second);
				}

				std::this_thread::sleep_for(100ms);
			}

			std::vector<BinaryIPAddress> expected_ips
			{
				// These are in expected order from least recent updated and least trusted
				// to most recent updated and most trusted
				IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193").GetBinary(),
				IPAddress(L"160.16.5.51").GetBinary(),
				IPAddress(L"200.168.5.51").GetBinary(),
				IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de").GetBinary(),
				IPAddress(L"199.111.110.30").GetBinary()
			};

			auto endpoints = pubendp.GetIPEndpoints().WithUniqueLock();

			pubendp.RemoveLeastRelevantIPEndpoints(1, *endpoints);
			Assert::AreEqual(true, RemoveIP(expected_ips, IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193").GetBinary()));
			endpoints.Unlock();
			Assert::AreEqual(true, CheckIPs(pubendp, expected_ips));

			endpoints.Lock();
			pubendp.RemoveLeastRelevantIPEndpoints(2, *endpoints);
			Assert::AreEqual(true, RemoveIP(expected_ips, IPAddress(L"160.16.5.51").GetBinary()));
			Assert::AreEqual(true, RemoveIP(expected_ips, IPAddress(L"200.168.5.51").GetBinary()));
			endpoints.Unlock();
			Assert::AreEqual(true, CheckIPs(pubendp, expected_ips));

			endpoints.Lock();
			pubendp.RemoveLeastRelevantIPEndpoints(1, *endpoints);
			Assert::AreEqual(true, RemoveIP(expected_ips, IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de").GetBinary()));
			endpoints.Unlock();
			Assert::AreEqual(true, CheckIPs(pubendp, expected_ips));

			endpoints.Lock();
			pubendp.RemoveLeastRelevantIPEndpoints(4, *endpoints); // Attempt to remove larger number than exists
			Assert::AreEqual(true, RemoveIP(expected_ips, IPAddress(L"199.111.110.30").GetBinary()));
			endpoints.Unlock();
			Assert::AreEqual(true, CheckIPs(pubendp, expected_ips));
		}

		TEST_METHOD(CheckMaxIPEndpoints)
		{
			Settings_CThS settings;

			PublicIPEndpoints pubendp{ settings };
			Assert::AreEqual(true, pubendp.Initialize());

			// Intentionally add more unique IP addresses from unique networks
			// to overflow the maximum number of endpoints we manage
			for (auto x = 0u; x < (PublicIPEndpoints::MaxIPEndpoints + 10); ++x)
			{
				auto pubip_str = Util::FormatString(L"180.100.90.%u", x);
				auto repip_str = Util::FormatString(L"18.%u.40.100", x);

				const auto result = pubendp.AddIPEndpoint(IPEndpoint(IPAddress(pubip_str), 999),
														  IPEndpoint(IPAddress(repip_str), 5000),
														  PeerConnectionType::Inbound, true, false);
				Assert::AreEqual(true, result.Succeeded());
				if (result.Succeeded())
				{
					Assert::AreEqual(true, result->first);
					Assert::AreEqual(true, result->second);
				}
			}

			Assert::AreEqual(true, pubendp.GetIPEndpoints().WithUniqueLock()->size() ==
							 PublicIPEndpoints::MaxIPEndpoints);
		}

		TEST_METHOD(AddIPAddresses)
		{
			struct TestCases
			{
				IPEndpoint PublicIPEndpoint;
				IPEndpoint ReportingPeer;
				PeerConnectionType ConnectionType{ PeerConnectionType::Unknown };
				bool Trusted{ false };
				bool Verified{ false };
				bool Success{ false };
				std::pair<bool, bool> Result;
			};

			const std::vector<TestCases> tests
			{
				{
					IPEndpoint(IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPAddress(L"172.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPAddress(L"173.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPAddress(L"174.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"), 999),
					IPEndpoint(IPAddress(L"e835:625f:48ce:c333::"), 2100),
					PeerConnectionType::Inbound,
					true, true, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"160.16.5.51"), 999),
					IPEndpoint(IPAddress(L"210.21.117.42"), 7000),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"), 9000),
					IPEndpoint(IPAddress(L"e845:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPAddress(L"160.16.5.51"), 3333),
					IPEndpoint(IPAddress(L"83.21.117.20"), 4500),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPAddress(L"199.111.110.30"), 6666),
					IPEndpoint(IPAddress(L"120.221.17.2"), 8000),
					PeerConnectionType::Outbound,
					true, false, true, std::make_pair(true, true)
				}
			};

			Settings_CThS settings;

			PublicIPEndpoints pubendp{ settings };
			Assert::AreEqual(true, pubendp.Initialize());

			for (const auto& test : tests)
			{
				const auto result = pubendp.AddIPEndpoint(test.PublicIPEndpoint, test.ReportingPeer,
														  test.ConnectionType, test.Trusted, test.Verified);
				Assert::AreEqual(test.Success, result.Succeeded());
				if (result.Succeeded())
				{
					Assert::AreEqual(test.Result.first, result->first);
					Assert::AreEqual(test.Result.second, result->second);
				}

				std::this_thread::sleep_for(100ms);
			}

			std::vector<BinaryIPAddress> expected_ips
			{
				IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193").GetBinary(),
				IPAddress(L"160.16.5.51").GetBinary(),
				IPAddress(L"200.168.5.51").GetBinary(),
				IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de").GetBinary(),
				IPAddress(L"199.111.110.30").GetBinary()
			};

			Vector<BinaryIPAddress> ips;
			const auto result = pubendp.AddIPAddresses(ips, false);
			Assert::AreEqual(true, result.Succeeded());
			Assert::AreEqual(true, CheckIPs(ips, expected_ips));

			ips.clear();

			Assert::AreEqual(true, RemoveIP(expected_ips, IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193").GetBinary()));
			Assert::AreEqual(true, RemoveIP(expected_ips, IPAddress(L"160.16.5.51").GetBinary()));

			const auto result2 = pubendp.AddIPAddresses(ips, true);
			Assert::AreEqual(true, result2.Succeeded());
			Assert::AreEqual(true, CheckIPs(ips, expected_ips));
		}
	};
}