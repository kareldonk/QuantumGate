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
			IPEndpoint ep2(IPEndpoint::Protocol::TCP, IPAddress(L"192.168.1.1"), 80, 1, 1);

			// Copy construction
			IPEndpoint ep3(ep2);

			// Equal and not equal
			{
				Assert::AreEqual(true, ep2 == ep3);
				Assert::AreEqual(false, ep2 != ep3);
				Assert::AreEqual(true, ep1 != ep2);

				IPEndpoint ep2a(IPEndpoint::Protocol::UDP, IPAddress(L"192.168.1.1"), 80, 1, 1);
				Assert::AreEqual(true, ep2 != ep2a);
				Assert::AreEqual(false, ep2 == ep2a);

				IPEndpoint ep2b(IPEndpoint::Protocol::TCP, IPAddress(L"192.168.1.0"), 80, 1, 1);
				Assert::AreEqual(true, ep2 != ep2b);
				Assert::AreEqual(false, ep2 == ep2b);

				IPEndpoint ep2c(IPEndpoint::Protocol::TCP, IPAddress(L"192.168.1.1"), 81, 1, 1);
				Assert::AreEqual(true, ep2 != ep2c);
				Assert::AreEqual(false, ep2 == ep2c);

				IPEndpoint ep2d(IPEndpoint::Protocol::TCP, IPAddress(L"192.168.1.1"), 80, 2, 1);
				Assert::AreEqual(true, ep2 != ep2d);
				Assert::AreEqual(false, ep2 == ep2d);

				IPEndpoint ep2e(IPEndpoint::Protocol::TCP, IPAddress(L"192.168.1.1"), 80, 1, 2);
				Assert::AreEqual(true, ep2d != ep2e);
				Assert::AreEqual(false, ep2d == ep2e);
			}

			// Move construction
			IPEndpoint ep4(std::move(ep2));
			Assert::AreEqual(true, ep3 == ep4);

			// Copy assignment
			ep1 = ep3;
			Assert::AreEqual(true, ep3 == ep1);

			IPEndpoint ep5(IPEndpoint::Protocol::TCP, IPAddress(L"fe80::c11a:3a9c:ef10:e795"), 8080);

			// Move assignment
			ep1 = std::move(ep5);
			Assert::AreEqual(false, ep3 == ep1);

			IPEndpoint ep6(IPEndpoint::Protocol::UDP, IPAddress(L"fe80::c11a:3a9c:ef10:e795"), 8080, 336699, 4);

			// GetString
			Assert::AreEqual(true, ep1.GetString() == L"TCP:[fe80::c11a:3a9c:ef10:e795]:8080");
			auto ss = ep3.GetString();
			Assert::AreEqual(true, ep3.GetString() == L"TCP:192.168.1.1:80:1:1");
			Assert::AreEqual(true, ep6.GetString() == L"UDP:[fe80::c11a:3a9c:ef10:e795]:8080:336699:4");

			// GetPort
			Assert::AreEqual(true, ep1.GetPort() == 8080);
			Assert::AreEqual(true, ep3.GetPort() == 80);

			// GetRelayPort
			Assert::AreEqual(true, ep3.GetRelayPort() == 1);
			Assert::AreEqual(true, ep4.GetRelayPort() == 1);
			Assert::AreEqual(true, ep6.GetRelayPort() == 336699);

			// GetRelayHop
			Assert::AreEqual(true, ep3.GetRelayHop() == 1);
			Assert::AreEqual(true, ep4.GetRelayHop() == 1);
			Assert::AreEqual(true, ep6.GetRelayHop() == 4);

			// GetIPAddress
			Assert::AreEqual(true, ep1.GetIPAddress() == IPAddress(L"fe80::c11a:3a9c:ef10:e795"));
			Assert::AreEqual(true, ep3.GetIPAddress() == IPAddress(L"192.168.1.1"));
		}

		TEST_METHOD(Input)
		{
			// Test invalid addresses
			Assert::ExpectException<std::invalid_argument>([] { IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L""), 80); });
			Assert::ExpectException<std::invalid_argument>([] { IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"abcd"), 80); });
			Assert::ExpectException<std::invalid_argument>([] { IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"fd12:3456.789a:1::1"), 80); });

			// Test invalid protocol
			Assert::ExpectException<std::invalid_argument>([] { IPEndpoint(IPEndpoint::Protocol::Unspecified, IPAddress(L"200.1.20.1"), 80); });
			Assert::ExpectException<std::invalid_argument>([] { IPEndpoint(static_cast<IPEndpoint::Protocol>(200), IPAddress(L"192.168.0.1"), 80); });

			// Test valid addresses
			try
			{
				IPEndpoint ep1(IPEndpoint::Protocol::TCP, IPAddress(L"0.0.0.0"), 80);
				IPEndpoint ep2(IPEndpoint::Protocol::UDP, IPAddress(L"192.168.1.1"), 0);
				IPEndpoint ep3(IPEndpoint::Protocol::ICMP, IPAddress(L"192.168.1.1"), 0);
				IPEndpoint ep4(IPEndpoint::Protocol::TCP, IPAddress(L"::"), 9000);
				IPEndpoint ep5(IPEndpoint::Protocol::TCP, IPAddress(L"fd12:3456:789a:1::1"), 443);
				IPEndpoint ep6(IPEndpoint::Protocol::UDP, IPAddress(L"fd00::"), 8080);
				IPEndpoint ep7(IPEndpoint::Protocol::ICMP, IPAddress(L"fd12:3456:789a:1::1"), 0);
			}
			catch (...)
			{
				Assert::Fail(L"Exception thrown while creating IPEndpoints");
			}
		}

		TEST_METHOD(Constexpr)
		{
			constexpr auto ip = BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 192 }, Byte{ 168 }, Byte{ 1 }, Byte{ 1 });
			constexpr IPEndpoint ep(IPEndpoint::Protocol::TCP, IPAddress(ip), 80, 9000, 1);
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

			constexpr IPEndpoint ep2(IPEndpoint::Protocol::TCP, IPAddress::LoopbackIPv4(), 80);
			
			constexpr IPEndpoint ep3(IPEndpoint::Protocol::TCP, IPAddress::LoopbackIPv6(), 80);
			
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