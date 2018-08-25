// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"
#include "Network\IPAddress.h"
#include "Common\Endian.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	TEST_CLASS(IPAddressTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Default construction
			IPAddress ip1;

			// Construction
			IPAddress ip2(L"192.168.1.1");
			Assert::AreEqual(true, ip2.GetString() == L"192.168.1.1");

			// Copy construction
			IPAddress ip3(ip2);
			Assert::AreEqual(true, ip3.GetString() == L"192.168.1.1");

			// Equal and not equal
			Assert::AreEqual(true, ip2 == ip3);
			Assert::AreEqual(false, ip2 != ip3);
			Assert::AreEqual(true, ip1 != ip2);

			// Move construction
			IPAddress ip4(std::move(ip2));
			Assert::AreEqual(true, ip3 == ip4);

			// Copy assignment
			ip1 = ip3;
			Assert::AreEqual(true, ip3 == ip1);

			IPAddress ip5(L"dead:beef:feed:face:cafe:babe:baad:c0de");

			// Move assignment
			ip1 = std::move(ip5);
			Assert::AreEqual(false, ip3 == ip1);

			// GetString
			Assert::AreEqual(true, ip1.GetString() == L"dead:beef:feed:face:cafe:babe:baad:c0de");
			Assert::AreEqual(true, ip3.GetString() == L"192.168.1.1");

			// GetBinary
			Assert::AreEqual(true,
				(ip1.GetBinary().UInt64s[0] == 0xcefaedfeefbeadde &&
				 ip1.GetBinary().UInt64s[1] == 0xdec0adbabebafeca));

			Assert::AreEqual(true,
				(ip3.GetBinary().UInt32s[0] == 0x0101a8c0 &&
				 ip3.GetBinary().UInt32s[1] == 0 &&
				 ip3.GetBinary().UInt32s[2] == 0 &&
				 ip3.GetBinary().UInt32s[3] == 0));

			// GetFamily
			Assert::AreEqual(true, ip1.GetFamily() == IPAddressFamily::IPv6);
			Assert::AreEqual(true, ip3.GetFamily() == IPAddressFamily::IPv4);
		}

		TEST_METHOD(Input)
		{
			// Test invalid addresses
			Assert::ExpectException<std::invalid_argument>([] { IPAddress(L""); });
			Assert::ExpectException<std::invalid_argument>([] { IPAddress(L"0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0"); });
			Assert::ExpectException<std::invalid_argument>([] { IPAddress(L"0000000000000000000000000000000000000000000000000000"); });
			Assert::ExpectException<std::invalid_argument>([] { IPAddress(L"abcdadefbghtmjurfvbghtyhvfregthnmredfgertfghyjukiolj"); });
			Assert::ExpectException<std::invalid_argument>([] { IPAddress(L"192.168.019.14"); }); // 019 is invalid (octal)
			Assert::ExpectException<std::invalid_argument>([] { IPAddress(L"abcz::c11a:3a9c:ef10:e795"); });
			Assert::ExpectException<std::invalid_argument>([] { IPAddress(L"192.x8.12.14"); });
			Assert::ExpectException<std::invalid_argument>([] { IPAddress(L"192.168 .1.1"); });
			Assert::ExpectException<std::invalid_argument>([] { IPAddress(L"fd12:3456.789a:1::1"); });

			IPAddress address;
			Assert::AreEqual(false, IPAddress::TryParse(L"", address));
			Assert::AreEqual(false, IPAddress::TryParse(L"abcd", address));
			Assert::AreEqual(false, IPAddress::TryParse(L"192.168.019.14", address));
			Assert::AreEqual(false, IPAddress::TryParse(L"abcz::c11a:3a9c:ef10:e795", address));
			Assert::AreEqual(false, IPAddress::TryParse(L"192.x8.12.14", address));
			Assert::AreEqual(false, IPAddress::TryParse(L"192.168 .1.1", address));
			Assert::AreEqual(false, IPAddress::TryParse(L"fd12:3456.789a:1::1", address));
			Assert::AreEqual(false, IPAddress::TryParse(L"192.168.1.010", address)); // 192.168.1.8
			Assert::AreEqual(false, IPAddress::TryParse(L"192.168.1.0x0A", address)); // 192.168.1.10
			Assert::AreEqual(false, IPAddress::TryParse(L"0xC0.0xa8.1.010", address)); // 192.168.1.8
			Assert::AreEqual(false, IPAddress::TryParse(L"0xc0a8010a", address)); // 192.168.1.10

			// Test valid addresses
			Assert::AreEqual(true, IPAddress::TryParse(L"0.0.0.0", address));
			Assert::AreEqual(true, address.GetString() == L"0.0.0.0");
			Assert::AreEqual(true, address.GetFamily() == IPAddressFamily::IPv4);
			Assert::AreEqual(true, IPAddress::TryParse(L"255.255.0.0", address));
			Assert::AreEqual(true, address.GetFamily() == IPAddressFamily::IPv4);
			Assert::AreEqual(true, IPAddress::TryParse(L"192.168.1.1", address));
			Assert::AreEqual(true, address.GetString() == L"192.168.1.1");
			Assert::AreEqual(true, address.GetFamily() == IPAddressFamily::IPv4);
			Assert::AreEqual(true, IPAddress::TryParse(L"::", address));
			Assert::AreEqual(true, address.GetFamily() == IPAddressFamily::IPv6);
			Assert::AreEqual(true, IPAddress::TryParse(L"fd12:3456:789a:1::1", address));
			Assert::AreEqual(true, address.GetString() == L"fd12:3456:789a:1::1");
			Assert::AreEqual(true, address.GetFamily() == IPAddressFamily::IPv6);
			Assert::AreEqual(true, IPAddress::TryParse(L"fe80::c11a:3a9c:ef10:e795", address));
			Assert::AreEqual(true, address.GetString() == L"fe80::c11a:3a9c:ef10:e795");
			Assert::AreEqual(true, address.GetFamily() == IPAddressFamily::IPv6);
			Assert::AreEqual(true, IPAddress::TryParse(L"fd00::", address));
			Assert::AreEqual(true, address.GetString() == L"fd00::");
			Assert::AreEqual(true, address.GetFamily() == IPAddressFamily::IPv6);
		}

		TEST_METHOD(Mask)
		{
			// Test invalid masks
			IPAddress mask;
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"/abcde", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"/12a", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"/", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"//", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"/ 12", mask));

			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"/33", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"/129", mask));

			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"a.0.0.0", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"256.255.255.255", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"255.255.0.019", mask));

			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"abcz:ffff:ffff:ffff::", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"ffff.ffff: ffff:8000::", mask));

			// Test valid masks
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"/0", mask));
			Assert::AreEqual(L"0.0.0.0", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddressFamily::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"0.0.0.0").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"/32", mask));
			Assert::AreEqual(L"255.255.255.255", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddressFamily::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"255.255.255.255").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"/16", mask));
			Assert::AreEqual(L"255.255.0.0", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddressFamily::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"255.255.0.0").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"/12", mask));
			Assert::AreEqual(L"255.240.0.0", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddressFamily::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"255.240.0.0").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"/8", mask));
			Assert::AreEqual(L"255.0.0.0", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddressFamily::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"255.0.0.0").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"/0", mask));
			Assert::AreEqual(L"::", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddressFamily::IPv6);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"::").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"/128", mask));
			Assert::AreEqual(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddressFamily::IPv6);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"/64", mask));
			Assert::AreEqual(L"ffff:ffff:ffff:ffff::", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddressFamily::IPv6);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"ffff:ffff:ffff:ffff::").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"/12", mask));
			Assert::AreEqual(L"fff0::", mask.GetString().c_str());
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"fff0::").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"/49", mask));
			Assert::AreEqual(L"ffff:ffff:ffff:8000::", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddressFamily::IPv6);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"ffff:ffff:ffff:8000::").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"0.0.0.0", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"255.255.255.255", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"255.255.0.0", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"255.240.0.0", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv4, L"255.0.0.0", mask));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"::", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"ffff:ffff:ffff:ffff::", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"fff0::", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddressFamily::IPv6, L"ffff:ffff:ffff:8000::", mask));
		}
	};
}