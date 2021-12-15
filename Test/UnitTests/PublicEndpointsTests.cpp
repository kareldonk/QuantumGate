// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Settings.h"
#include "Core\PublicEndpoints.h"
#include "Common\Util.h"

#include <thread>

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Core;
using namespace QuantumGate::Implementation::Network;

bool CheckAddresses(const Vector<Address>& addrs, const std::vector<Address>& exp_addrs)
{
	for (const auto& addr : addrs)
	{
		const auto it = std::find_if(exp_addrs.begin(), exp_addrs.end(),
									 [&](const auto& value)
		{
			return value == addr;
		});
		if (it == exp_addrs.end()) return false;
	}

	for (const auto& exp_addr : exp_addrs)
	{
		const auto it = std::find(addrs.begin(), addrs.end(), exp_addr);
		if (it == addrs.end()) return false;
	}

	return true;
}

bool CheckAddresses(const PublicEndpoints& pubendp, const std::vector<Address>& exp_addrs)
{
	Vector<Address> pub_addrs;
	const auto result = pubendp.AddAddresses(pub_addrs, false);
	if (!result.Succeeded()) return false;

	return CheckAddresses(pub_addrs, exp_addrs);
}

bool RemoveAddress(std::vector<Address>& list, const Address& addr)
{
	const auto it = std::find(list.begin(), list.end(), addr);
	if (it != list.end())
	{
		list.erase(it);
		return true;
	}

	return false;
}

namespace UnitTests
{
	TEST_CLASS(PublicEndpointsTests)
	{
	public:
		TEST_METHOD(General)
		{
			struct TestCases final
			{
				Endpoint PublicEndpoint;
				Endpoint ReportingPeer;
				PeerConnectionType ConnectionType{ PeerConnectionType::Unknown };
				bool Trusted{ false };
				bool Success{ false };
				std::pair<bool, bool> Result;
			};

			const std::vector<TestCases> tests
			{
				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"172.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, true)
				},

				{
					// Should fail because of unknown connection type
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"160.16.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"210.21.117.42"), 7000),
					PeerConnectionType::Unknown,
					false, false, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"160.16.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"210.21.117.42"), 7000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"), 9000),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"e835:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, true)
				},

				{
					// Should fail because of different IP address types
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"160.16.5.51"), 9000),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"e825:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					PeerConnectionType::Inbound,
					false, false, std::make_pair(false, false)
				},

				{
					// Should fail because of different IP address types
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"e825:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"160.16.5.51"), 9000),
					PeerConnectionType::Outbound,
					false, false, std::make_pair(false, false)
				},

				{
					// Should get accepted but not a new address because 160.16.5.51
					// was already added previously; port will get added
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"160.16.5.51"), 3333),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"83.21.117.20"), 4500),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, false)
				},

				{
					// Should get accepted but not a new address because 160.16.5.51
					// was already added previously; protocol and port will get added
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"160.16.5.51"), 6666),
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"83.121.117.20"), 4500),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, false)
				},

				{
					// Should not get accepted because reporting IP 210.21.117.20 is on
					// same /16 network as previous reporting IP 210.21.117.42
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"120.16.115.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"210.21.117.20"), 7000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(false, false)
				},

				{
					// Should not get accepted because reporting IP 210.21.217.42 is on
					// same /16 network as previous reporting IP 210.21.117.42
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"170.216.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"210.21.217.42"), 7000),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(false, false)
				},

				{
					// Should not get accepted because reporting IP e835:625f:48ce:c333:: is on
					// same /48 network as previous reporting IP e835:625f:48ce:c433:7c5d:ea3:76c3:ca0
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"e835:625f:48ce:c333::"), 2100),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(false, false)
				},

				{
					// Should get accepted now because even though reporting IP e835:625f:48ce:c333:: is on
					// same /48 network as previous reporting IP e835:625f:48ce:c433:7c5d:ea3:76c3:ca0,
					// this is from a trusted peer
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"e835:625f:48ce:c333::"), 2100),
					PeerConnectionType::Inbound,
					true, true, std::make_pair(true, true)
				},

				{
					// Outgoing connection won't get port added
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"199.111.110.30"), 6666),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"120.221.17.2"), 8000),
					PeerConnectionType::Outbound,
					true, true, std::make_pair(true, true)
				},

				{
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(D1:C2:D3:FE:15:32)"), 4),
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, true)
				},

				{
					// Should get accepted but not a new address because (D1:C2:D3:FE:15:32)
					// was already added previously
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(D1:C2:D3:FE:15:32)"), 4),
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(22:5D:D3:5B:93:B1)"), 9),
					PeerConnectionType::Inbound,
					false, true, std::make_pair(true, false)
				}
			};

			Settings_CThS settings;

			PublicEndpoints pubendp{ settings };
			Assert::AreEqual(false, pubendp.IsInitialized());
			Assert::AreEqual(true, pubendp.Initialize());
			Assert::AreEqual(true, pubendp.IsInitialized());

			for (const auto& test : tests)
			{
				const auto result = pubendp.AddEndpoint(test.PublicEndpoint, test.ReportingPeer,
														test.ConnectionType, test.Trusted);
				Assert::AreEqual(test.Success, result.Succeeded());
				if (result.Succeeded())
				{
					Assert::AreEqual(test.Result.first, result->first);
					Assert::AreEqual(test.Result.second, result->second);
				}
			}

			{
				struct ExpectedAddress final
				{
					Address Address;
					bool Trusted{ false };
					Set<UInt16> TCPPorts;
					Set<UInt16> UDPPorts;
					Set<UInt16> RFCOMMPorts;
					Size NumReportingPeerNetworks{ 0 };
				};

				std::vector<ExpectedAddress> expected_addrs
				{
					{ IPAddress(L"200.168.5.51"), false, { 999 }, {}, {}, 1 },
					{ IPAddress(L"160.16.5.51"), false, { 999, 3333 }, { 6666 }, {}, 3 },
					{ IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"), false, { 9000 }, {}, {}, 1 },
					{ IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"), true, { 999 }, {}, {}, 1 },
					{ IPAddress(L"199.111.110.30"), true, {}, {}, {}, 1 },
					{ BTHAddress(L"(D1:C2:D3:FE:15:32)"), false, {}, {}, { 4 }, 2 }
				};

				// Check that we got back the expected addresses
				{
					Vector<Address> pub_addrs;
					const auto result = pubendp.AddAddresses(pub_addrs, false);
					Assert::AreEqual(true, result.Succeeded());

					for (const auto& addr : pub_addrs)
					{
						const auto it = std::find_if(expected_addrs.begin(), expected_addrs.end(),
													 [&](const auto& value)
						{
							return value.Address == addr;
						});
						Assert::AreEqual(true, it != expected_addrs.end());
					}

					for (const auto& exp_details : expected_addrs)
					{
						const auto it = std::find(pub_addrs.begin(), pub_addrs.end(), exp_details.Address);
						Assert::AreEqual(true, it != pub_addrs.end());
					}
				}

				// Check that the Endpoint details are what we expect
				{
					auto endpoints = pubendp.GetEndpoints().WithSharedLock();

					for (const auto& exp_details : expected_addrs)
					{
						const auto it2 = endpoints->find(exp_details.Address);
						Assert::AreEqual(true, it2 != endpoints->end());

						Assert::AreEqual(true, it2->second.Trusted == exp_details.Trusted);
						Assert::AreEqual(true, it2->second.ReportingPeerNetworkHashes.size() ==
										 exp_details.NumReportingPeerNetworks);

						if (exp_details.TCPPorts.size() > 0 || exp_details.UDPPorts.size() > 0 ||
							exp_details.RFCOMMPorts.size() > 0)
						{
							Assert::AreEqual(true, it2->second.PortsMap.size() > 0);
						}

						for (auto ports : it2->second.PortsMap)
						{
							if (ports.first == Network::Protocol::TCP)
							{
								Assert::AreEqual(true, ports.second == exp_details.TCPPorts);
							}
							else if (ports.first == Network::Protocol::UDP)
							{
								Assert::AreEqual(true, ports.second == exp_details.UDPPorts);
							}
							else if (ports.first == Network::Protocol::RFCOMM)
							{
								Assert::AreEqual(true, ports.second == exp_details.RFCOMMPorts);
							}
							else
							{
								Assert::Fail();
							}
						}
					}
				}

				pubendp.Deinitialize();
				Assert::AreEqual(false, pubendp.IsInitialized());
			}
		}

		TEST_METHOD(RemoveLeastRelevantEndpoints)
		{
			struct TestCases final
			{
				Endpoint PublicEndpoint;
				Endpoint ReportingPeer;
				PeerConnectionType ConnectionType{ PeerConnectionType::Unknown };
				bool Trusted{ false };
				bool Verified{ false };
				bool Success{ false };
				std::pair<bool, bool> Result;
			};

			const std::vector<TestCases> tests
			{
				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"172.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"173.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"174.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"e835:625f:48ce:c333::"), 2100),
					PeerConnectionType::Inbound,
					true, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"160.16.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"210.21.117.42"), 7000),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"), 9000),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"e845:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"160.16.5.51"), 3333),
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"83.21.117.20"), 4500),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"199.111.110.30"), 6666),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"120.221.17.2"), 8000),
					PeerConnectionType::Outbound,
					true, false, true, std::make_pair(true, true)
				},

				{
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(D1:C2:D3:FE:15:32)"), 4),
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				}
			};

			Settings_CThS settings;

			PublicEndpoints pubendp{ settings };
			Assert::AreEqual(true, pubendp.Initialize());

			for (const auto& test : tests)
			{
				const auto result = pubendp.AddEndpoint(test.PublicEndpoint, test.ReportingPeer,
														test.ConnectionType, test.Trusted, test.Verified);
				Assert::AreEqual(test.Success, result.Succeeded());
				if (result.Succeeded())
				{
					Assert::AreEqual(test.Result.first, result->first);
					Assert::AreEqual(test.Result.second, result->second);
				}

				std::this_thread::sleep_for(100ms);
			}

			std::vector<Address> expected_addrs
			{
				// These are in expected order from least recent updated and least trusted
				// to most recent updated and most trusted
				IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"),
				IPAddress(L"160.16.5.51"),
				BTHAddress(L"(D1:C2:D3:FE:15:32)"),
				IPAddress(L"200.168.5.51"),
				IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"),
				IPAddress(L"199.111.110.30")
			};

			auto endpoints = pubendp.GetEndpoints().WithUniqueLock();

			pubendp.RemoveLeastRelevantEndpoints(1, *endpoints);
			Assert::AreEqual(true, RemoveAddress(expected_addrs, IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193")));
			endpoints.Unlock();
			Assert::AreEqual(true, CheckAddresses(pubendp, expected_addrs));

			endpoints.Lock();
			pubendp.RemoveLeastRelevantEndpoints(2, *endpoints);
			Assert::AreEqual(true, RemoveAddress(expected_addrs, IPAddress(L"160.16.5.51")));
			Assert::AreEqual(true, RemoveAddress(expected_addrs, BTHAddress(L"(D1:C2:D3:FE:15:32)")));
			endpoints.Unlock();
			Assert::AreEqual(true, CheckAddresses(pubendp, expected_addrs));

			endpoints.Lock();
			pubendp.RemoveLeastRelevantEndpoints(1, *endpoints);
			Assert::AreEqual(true, RemoveAddress(expected_addrs, IPAddress(L"200.168.5.51")));
			endpoints.Unlock();
			Assert::AreEqual(true, CheckAddresses(pubendp, expected_addrs));

			endpoints.Lock();
			pubendp.RemoveLeastRelevantEndpoints(1, *endpoints);
			Assert::AreEqual(true, RemoveAddress(expected_addrs, IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de")));
			endpoints.Unlock();
			Assert::AreEqual(true, CheckAddresses(pubendp, expected_addrs));

			endpoints.Lock();
			pubendp.RemoveLeastRelevantEndpoints(4, *endpoints); // Attempt to remove larger number than exists
			Assert::AreEqual(true, RemoveAddress(expected_addrs, IPAddress(L"199.111.110.30")));
			endpoints.Unlock();
			Assert::AreEqual(true, CheckAddresses(pubendp, expected_addrs));
		}

		TEST_METHOD(CheckMaxEndpoints)
		{
			Settings_CThS settings;

			PublicEndpoints pubendp{ settings };
			Assert::AreEqual(true, pubendp.Initialize());

			// Intentionally add more unique IP addresses from unique networks
			// to overflow the maximum number of endpoints we manage
			constexpr auto max = PublicEndpoints::MaxEndpoints + 10;
			static_assert(max <= 255, "Should be smaller");
			for (auto x = 0u; x < max; ++x)
			{
				auto pubip_str = Util::FormatString(L"180.100.90.%u", x);
				auto repip_str = Util::FormatString(L"18.%u.40.100", x);

				const auto result = pubendp.AddEndpoint(IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(pubip_str), 999),
														IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(repip_str), 5000),
														PeerConnectionType::Inbound, true, false);
				Assert::AreEqual(true, result.Succeeded());
				if (result.Succeeded())
				{
					Assert::AreEqual(true, result->first);
					Assert::AreEqual(true, result->second);
				}
			}

			Assert::AreEqual(true, pubendp.GetEndpoints().WithUniqueLock()->size() == PublicEndpoints::MaxEndpoints);
		}

		TEST_METHOD(AddAddresses)
		{
			struct TestCases final
			{
				Endpoint PublicEndpoint;
				Endpoint ReportingPeer;
				PeerConnectionType ConnectionType{ PeerConnectionType::Unknown };
				bool Trusted{ false };
				bool Verified{ false };
				bool Success{ false };
				std::pair<bool, bool> Result;
			};

			const std::vector<TestCases> tests
			{
				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"172.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"173.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"200.168.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"174.217.17.142"), 5000),
					PeerConnectionType::Inbound,
					false, true, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"e835:625f:48ce:c333::"), 2100),
					PeerConnectionType::Inbound,
					true, true, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"160.16.5.51"), 999),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"210.21.117.42"), 7000),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"), 9000),
					IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"e845:625f:48ce:c433:7c5d:ea3:76c3:ca0"), 2000),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"160.16.5.51"), 3333),
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"83.21.117.20"), 4500),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, false)
				},

				{
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"199.111.110.30"), 6666),
					IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"120.221.17.2"), 8000),
					PeerConnectionType::Outbound,
					true, false, true, std::make_pair(true, true)
				},

				{
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(D1:C2:D3:FE:15:32)"), 0),
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, true)
				},

				{
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(D1:C2:D3:FE:15:32)"), 0),
					BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(12:5F:E3:5B:93:B2)"), 9),
					PeerConnectionType::Inbound,
					false, false, true, std::make_pair(true, false)
				}
			};

			Settings_CThS settings;

			PublicEndpoints pubendp{ settings };
			Assert::AreEqual(true, pubendp.Initialize());

			for (const auto& test : tests)
			{
				const auto result = pubendp.AddEndpoint(test.PublicEndpoint, test.ReportingPeer,
														test.ConnectionType, test.Trusted, test.Verified);
				Assert::AreEqual(test.Success, result.Succeeded());
				if (result.Succeeded())
				{
					Assert::AreEqual(test.Result.first, result->first);
					Assert::AreEqual(test.Result.second, result->second);
				}

				std::this_thread::sleep_for(100ms);
			}

			std::vector<Address> expected_addrs
			{
				IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"),
				IPAddress(L"160.16.5.51"),
				IPAddress(L"200.168.5.51"),
				IPAddress(L"bdb0:434d:96c9:17d9:661c:db34:2ec0:21de"),
				IPAddress(L"199.111.110.30"),
				BTHAddress(L"(D1:C2:D3:FE:15:32)")
			};

			Vector<Address> addrs;
			const auto result = pubendp.AddAddresses(addrs, false);
			Assert::AreEqual(true, result.Succeeded());
			Assert::AreEqual(true, CheckAddresses(addrs, expected_addrs));

			addrs.clear();

			Assert::AreEqual(true, RemoveAddress(expected_addrs, IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193")));
			Assert::AreEqual(true, RemoveAddress(expected_addrs, IPAddress(L"160.16.5.51")));
			Assert::AreEqual(true, RemoveAddress(expected_addrs, BTHAddress(L"(D1:C2:D3:FE:15:32)")));

			const auto result2 = pubendp.AddAddresses(addrs, true);
			Assert::AreEqual(true, result2.Succeeded());
			Assert::AreEqual(true, CheckAddresses(addrs, expected_addrs));
		}
	};
}