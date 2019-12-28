// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\IPEndpoint.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	TEST_CLASS(IPEndpointTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Default construction
			IPEndpoint ep1;

			// Construction
			IPEndpoint ep2(IPAddress(L"192.168.1.1"), 80);

			// Copy construction
			IPEndpoint ep3(ep2);

			// Equal and not equal
			Assert::AreEqual(true, ep2 == ep3);
			Assert::AreEqual(false, ep2 != ep3);
			Assert::AreEqual(true, ep1 != ep2);

			// Move construction
			IPEndpoint ep4(std::move(ep2));
			Assert::AreEqual(true, ep3 == ep4);

			// Copy assignment
			ep1 = ep3;
			Assert::AreEqual(true, ep3 == ep1);

			IPEndpoint ep5(IPAddress(L"fe80::c11a:3a9c:ef10:e795"), 8080);

			// Move assignment
			ep1 = std::move(ep5);
			Assert::AreEqual(false, ep3 == ep1);

			// GetString
			Assert::AreEqual(true, ep1.GetString() == L"[fe80::c11a:3a9c:ef10:e795]:8080");
			Assert::AreEqual(true, ep3.GetString() == L"192.168.1.1:80");

			// GetPort
			Assert::AreEqual(true, ep1.GetPort() == 8080);
			Assert::AreEqual(true, ep3.GetPort() == 80);

			// GetIPAddress
			Assert::AreEqual(true, ep1.GetIPAddress() == IPAddress(L"fe80::c11a:3a9c:ef10:e795"));
			Assert::AreEqual(true, ep3.GetIPAddress() == IPAddress(L"192.168.1.1"));
		}

		TEST_METHOD(Input)
		{
			// Test invalid addresses
			Assert::ExpectException<std::invalid_argument>([] { IPEndpoint(IPAddress(L""), 80); });
			Assert::ExpectException<std::invalid_argument>([] { IPEndpoint(IPAddress(L"abcd"), 80); });
			Assert::ExpectException<std::invalid_argument>([] { IPEndpoint(IPAddress(L"fd12:3456.789a:1::1"), 80); });

			// Test valid addresses
			try
			{
				IPEndpoint ep1(IPAddress(L"0.0.0.0"), 80);
				IPEndpoint ep2(IPAddress(L"192.168.1.1"), 0);
				IPEndpoint ep3(IPAddress(L"::"), 9000);
				IPEndpoint ep4(IPAddress(L"fd12:3456:789a:1::1"), 443);
				IPEndpoint ep5(IPAddress(L"fd00::"), 8080);
			}
			catch (...)
			{
				Assert::Fail(L"Exception thrown while creating IPEndpoints");
			}
		}

		TEST_METHOD(Constexpr)
		{
			constexpr auto ip = BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 192 }, Byte{ 168 }, Byte{ 1 }, Byte{ 1 });
			constexpr IPEndpoint ep(IPAddress(ip), 80, 9000, 1);
			constexpr IPAddress ip2 = ep.GetIPAddress();
			constexpr auto port = ep.GetPort();
			constexpr auto rport = ep.GetRelayPort();
			constexpr auto rhop = ep.GetRelayHop();

			static_assert(port == 80, "Should be equal");
			static_assert(rport == 9000, "Should be equal");
			static_assert(rhop == 1, "Should be equal");

			Assert::AreEqual(true, ip == ip2.GetBinary());
			Assert::AreEqual(true, port == 80);
			Assert::AreEqual(true, rport == 9000);
			Assert::AreEqual(true, rhop == 1);

			constexpr IPEndpoint ep2(IPAddress::LoopbackIPv4(), 80);
			
			constexpr IPEndpoint ep3(IPAddress::LoopbackIPv6(), 80);
			
			constexpr IPEndpoint ep4(std::move(ep));
			static_assert(ep4.GetPort() == 80, "Should be equal");
			static_assert(ep4.GetRelayPort() == 9000, "Should be equal");
			static_assert(ep4.GetRelayHop() == 1, "Should be equal");
			Assert::AreEqual(true, ep4.GetIPAddress() == ip2);
			Assert::AreEqual(true, ep4.GetPort() == 80);
			Assert::AreEqual(true, ep4.GetRelayPort() == 9000);
			Assert::AreEqual(true, ep4.GetRelayHop() == 1);

			constexpr IPEndpoint ep5 = std::move(ep4);
			Assert::AreEqual(true, ep5.GetIPAddress() == ip2);
			Assert::AreEqual(true, ep5.GetPort() == 80);
			Assert::AreEqual(true, ep5.GetRelayPort() == 9000);
			Assert::AreEqual(true, ep5.GetRelayHop() == 1);

			constexpr auto ep6 = ep5;
			Assert::AreEqual(true, ep6.GetIPAddress() == ip2);
			Assert::AreEqual(true, ep6.GetPort() == 80);
			Assert::AreEqual(true, ep6.GetRelayPort() == 9000);
			Assert::AreEqual(true, ep6.GetRelayHop() == 1);
		}
	};
}