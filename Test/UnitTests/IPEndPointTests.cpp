// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"
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
	};
}