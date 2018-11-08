// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
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
			Assert::AreEqual(true, ip1.GetString() == L"0.0.0.0");
			Assert::AreEqual(true, ip1.GetFamily() == IPAddress::Family::IPv4);

			// Construction
			IPAddress ip2(L"192.168.1.1");
			Assert::AreEqual(true, ip2.GetString() == L"192.168.1.1");
			Assert::AreEqual(true, ip2.GetFamily() == IPAddress::Family::IPv4);

			// Copy construction
			IPAddress ip3(ip2);
			Assert::AreEqual(true, ip3.GetString() == L"192.168.1.1");
			Assert::AreEqual(true, ip3.GetFamily() == IPAddress::Family::IPv4);

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
			Assert::AreEqual(true, ip1.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, ip3.GetFamily() == IPAddress::Family::IPv4);

			const auto any_ip4 = IPAddress::AnyIPv4();
			Assert::AreEqual(true, any_ip4.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, any_ip4.GetString() == L"0.0.0.0");

			const auto any_ip6 = IPAddress::AnyIPv6();
			Assert::AreEqual(true, any_ip6.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, any_ip6.GetString() == L"::");

			const auto lb_ip4 = IPAddress::LoopbackIPv4();
			Assert::AreEqual(true, lb_ip4.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, lb_ip4.GetString() == L"127.0.0.1");

			const auto lb_ip6 = IPAddress::LoopbackIPv6();
			Assert::AreEqual(true, lb_ip6.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, lb_ip6.GetString() == L"::1");
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
			Assert::AreEqual(false, IPAddress::TryParse(L"abcz::c11a:3a9c:ef10:e795%1", address));
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
			Assert::AreEqual(true, address.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, IPAddress::TryParse(L"255.255.0.0", address));
			Assert::AreEqual(true, address.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, IPAddress::TryParse(L"192.168.1.1", address));
			Assert::AreEqual(true, address.GetString() == L"192.168.1.1");
			Assert::AreEqual(true, address.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, IPAddress::TryParse(L"::", address));
			Assert::AreEqual(true, address.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, IPAddress::TryParse(L"fd12:3456:789a:1::1", address));
			Assert::AreEqual(true, address.GetString() == L"fd12:3456:789a:1::1");
			Assert::AreEqual(true, address.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, IPAddress::TryParse(L"fe80::c11a:3a9c:ef10:e795", address));
			Assert::AreEqual(true, address.GetString() == L"fe80::c11a:3a9c:ef10:e795");
			Assert::AreEqual(true, IPAddress::TryParse(L"fe80::c11a:3a9c:ef10:e795%2", address));
			Assert::AreEqual(true, address.GetString() == L"fe80::c11a:3a9c:ef10:e795");
			Assert::AreEqual(true, address.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, IPAddress::TryParse(L"fd00::", address));
			Assert::AreEqual(true, address.GetString() == L"fd00::");
			Assert::AreEqual(true, address.GetFamily() == IPAddress::Family::IPv6);
		}

		TEST_METHOD(Mask)
		{
			// Test invalid masks
			IPAddress mask;
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv4, L" ", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"/abcde", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"/12a", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"/", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"//", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"/ 12", mask));

			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"/33", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"/129", mask));

			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"a.0.0.0", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"256.255.255.255", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"255.255.0.019", mask));

			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"abcz:ffff:ffff:ffff::", mask));
			Assert::AreEqual(false, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"ffff.ffff: ffff:8000::", mask));

			// Test valid masks
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"/0", mask));
			Assert::AreEqual(L"0.0.0.0", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"0.0.0.0").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"/32", mask));
			Assert::AreEqual(L"255.255.255.255", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"255.255.255.255").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"/16", mask));
			Assert::AreEqual(L"255.255.0.0", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"255.255.0.0").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"/12", mask));
			Assert::AreEqual(L"255.240.0.0", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"255.240.0.0").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"/8", mask));
			Assert::AreEqual(L"255.0.0.0", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddress::Family::IPv4);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"255.0.0.0").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"/0", mask));
			Assert::AreEqual(L"::", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"::").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"/128", mask));
			Assert::AreEqual(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"/64", mask));
			Assert::AreEqual(L"ffff:ffff:ffff:ffff::", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"ffff:ffff:ffff:ffff::").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"/12", mask));
			Assert::AreEqual(L"fff0::", mask.GetString().c_str());
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"fff0::").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"/49", mask));
			Assert::AreEqual(L"ffff:ffff:ffff:8000::", mask.GetString().c_str());
			Assert::AreEqual(true, mask.GetFamily() == IPAddress::Family::IPv6);
			Assert::AreEqual(true, (mask.GetBinary() == IPAddress(L"ffff:ffff:ffff:8000::").GetBinary()));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"0.0.0.0", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"255.255.255.255", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"255.255.0.0", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"255.240.0.0", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv4, L"255.0.0.0", mask));

			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"::", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"ffff:ffff:ffff:ffff::", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"fff0::", mask));
			Assert::AreEqual(true, IPAddress::TryParseMask(IPAddress::Family::IPv6, L"ffff:ffff:ffff:8000::", mask));
		}

		TEST_METHOD(CreateMask)
		{
			IPAddress ipmask;
			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv4, 0, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"0.0.0.0"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv4, 1, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"128.0.0.0"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv4, 2, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"192.0.0.0"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv4, 4, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"240.0.0.0"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv4, 15, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"255.254.0.0"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv4, 16, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"255.255.0.0"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv4, 17, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"255.255.128.0"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv4, 31, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"255.255.255.254"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv4, 32, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"255.255.255.255"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv6, 0, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"::"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv6, 1, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"8000::"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv6, 7, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"fe00::"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv6, 63, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"ffff:ffff:ffff:fffe::"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv6, 64, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"ffff:ffff:ffff:ffff::"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv6, 65, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"ffff:ffff:ffff:ffff:8000::"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv6, 67, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"ffff:ffff:ffff:ffff:e000::"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv6, 127, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe"));

			Assert::AreEqual(true, IPAddress::CreateMask(IPAddress::Family::IPv6, 128, ipmask));
			Assert::AreEqual(true, ipmask == IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
		}

		TEST_METHOD(GetNetwork)
		{
			struct IPTest
			{
				BinaryIPAddress ip;
				BinaryIPAddress network;
				UInt8 cidr{ 0 };
				bool success{ false };
			};

			const std::vector<IPTest> iptests
			{
				{ IPAddress(L"192.168.1.10").GetBinary(), IPAddress(L"192.168.0.0").GetBinary(), 16, true },
				{ IPAddress(L"192.168.1.20").GetBinary(), IPAddress(L"192.168.0.0").GetBinary(), 16, true },
				{ IPAddress(L"172.217.7.238").GetBinary(), IPAddress(L"172.217.0.0").GetBinary(), 16, true },
				{ IPAddress(L"172.217.4.138").GetBinary(), IPAddress(L"172.217.0.0").GetBinary(), 16, true },
				{ IPAddress(L"172.117.4.138").GetBinary(), IPAddress(L"172.117.0.0").GetBinary(), 16, true },
				{ IPAddress(L"172.117.4.138").GetBinary(), IPAddress(L"172.117.0.0").GetBinary(), 35, false },
				{ IPAddress(L"172.117.4.138").GetBinary(), IPAddress(L"172.117.4.138").GetBinary(), 32, true },
				{ IPAddress(L"172.117.4.138").GetBinary(), IPAddress(L"172.0.0.0").GetBinary(), 8, true },
				{ IPAddress(L"200.1.157.11").GetBinary(), IPAddress(L"200.1.128.0").GetBinary(), 17, true },
				{ IPAddress(L"200.1.157.11").GetBinary(), IPAddress(L"200.0.0.0").GetBinary(), 10, true },
				{ IPAddress(L"200.1.157.11").GetBinary(), IPAddress(L"200.0.0.0").GetBinary(), 14, true },
				{ IPAddress(L"200.1.157.11").GetBinary(), IPAddress(L"200.1.157.0").GetBinary(), 25, true },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e796::").GetBinary(), IPAddress(L"fe80:c11a:3a9c::").GetBinary(), 48, true },
				{ IPAddress(L"e835:625f:48ce:c433:7c5d:ea3:76c3:ca0").GetBinary(), IPAddress(L"e800::").GetBinary(), 8, true },
				{ IPAddress(L"e835:625f:48ce:c433:7c5d:ea3:76c3:ca0").GetBinary(), IPAddress(L"e835:6200::").GetBinary(), 23, true },
				{ IPAddress(L"e835:625f:48ce:c433:7c5d:ea3:76c3:ca0").GetBinary(), IPAddress(L"e835:625f:48ce::").GetBinary(), 48, true },
				{ IPAddress(L"e835:625f:48ce:c433:7c5d:ea3:76c3:ca0").GetBinary(), IPAddress(L"e835:625f:48ce:c433:7c5d:e80::").GetBinary(), 90, true },
				{ IPAddress(L"e835:625f:48ce:c433:7c5d:ea3:76c3:ca0").GetBinary(), IPAddress(L"e835:625f:48ce:c433:7c5d:ea3:76c3:ca0").GetBinary(), 128, true },
				{ IPAddress(L"e835:625f:48ce:c433:7c5d:ea3:76c3:ca0").GetBinary(), IPAddress(L"e835:625f:48ce:c433:7c5d:e80::").GetBinary(), 129, false }
			};

			for (const auto& test : iptests)
			{
				BinaryIPAddress network;
				const auto success = BinaryIPAddress::GetNetwork(test.ip, test.cidr, network);
				Assert::AreEqual(test.success, success);
				if (success)
				{
					Assert::AreEqual(true, test.network == network);
				}
			}
		}

		TEST_METHOD(AreInSameNetwork)
		{
			struct IPTest
			{
				BinaryIPAddress ip1;
				BinaryIPAddress ip2;
				UInt8 cidr{ 0 };
				bool success{ false };
				bool same_network{ false };
			};

			const std::vector<IPTest> iptests
			{
				{ IPAddress(L"192.168.1.10").GetBinary(), IPAddress(L"192.168.1.20").GetBinary(), 32, true, false },
				{ IPAddress(L"192.168.1.10").GetBinary(), IPAddress(L"192.168.1.20").GetBinary(), 24, true, true },
				{ IPAddress(L"192.168.1.10").GetBinary(), IPAddress(L"200.168.5.51").GetBinary(), 24, true, false },
				{ IPAddress(L"192.168.1.10").GetBinary(), IPAddress(L"200.168.5.51").GetBinary(), 16, true, false },
				{ IPAddress(L"192.168.1.10").GetBinary(), IPAddress(L"200.168.5.51").GetBinary(), 8, true, false },
				{ IPAddress(L"192.168.1.10").GetBinary(), IPAddress(L"200.168.5.51").GetBinary(), 128, false, false },
				{ IPAddress(L"192.168.1.10").GetBinary(), IPAddress(L"200.168.5.51").GetBinary(), 0, true, true },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e795::").GetBinary(), IPAddress(L"200.168.5.51").GetBinary(), 128, true, false },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e795::").GetBinary(), IPAddress(L"200.168.5.51").GetBinary(), 48, true, false },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e795::").GetBinary(), IPAddress(L"200.168.5.51").GetBinary(), 0, true, false },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e795::").GetBinary(), IPAddress(L"fe80:c11a:3a9c:ef11:e795::").GetBinary(), 130, false, false },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e795::").GetBinary(), IPAddress(L"fe80:c11a:3a9c:ef11:e795::").GetBinary(), 128, true, false },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e795::").GetBinary(), IPAddress(L"fe80:c11a:3a9c:ef11:e795::").GetBinary(), 64, true, false },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e795::").GetBinary(), IPAddress(L"fe80:c11a:3a9c:ef11:e795::").GetBinary(), 48, true, true }
			};

			for (const auto& test : iptests)
			{
				const auto[success, same_network] = BinaryIPAddress::AreInSameNetwork(test.ip1, test.ip2, test.cidr);
				Assert::AreEqual(test.success, success);
				Assert::AreEqual(test.same_network, same_network);
			}
		}

		TEST_METHOD(IsInAddressRange)
		{
			struct IPTest
			{
				IPAddress ip;
				IPAddress netip;
				UInt8 cidr{ 0 };
				bool success{ false };
				bool inrange{ false };
			};

			const std::vector<IPTest> iptests
			{
				{ IPAddress(L"5529:f4b2:3ff9:a074:d03a:d18e:760d:b193"), IPAddress(L"ff00::"), 8, true, false },
				{ IPAddress(L"ff80:f4b2:3ff9:a074:d03a:d18e:760d:b193"), IPAddress(L"ff00::"), 8, true, true },
				{ IPAddress(L"ffc0:f4b2:3ff9:a074:d03a:d18e:760d:b193"), IPAddress(L"ff00::"), 8, true, true },
				{ IPAddress(L"::1"), IPAddress(L"::"), 127, true, true },
				{ IPAddress(L"::"), IPAddress(L"::"), 127, true, true },
				{ IPAddress(L"::2"), IPAddress(L"::"), 127, true, false },

				{ IPAddress(L"192.168.1.1"), IPAddress(L"192.168.0.0"), 16, true, true },
				{ IPAddress(L"192.168.100.30"), IPAddress(L"192.168.0.0"), 16, true, true },
				{ IPAddress(L"192.167.1.1"), IPAddress(L"192.168.0.0"), 16, true, false },
				{ IPAddress(L"192.169.1.1"), IPAddress(L"192.168.0.0"), 16, true, false },
				{ IPAddress(L"192.172.1.1"), IPAddress(L"192.168.0.0"), 16, true, false },

				{ IPAddress(L"172.16.1.1"), IPAddress(L"172.16.0.0"), 12, true, true },
				{ IPAddress(L"172.16.100.53"), IPAddress(L"172.16.0.0"), 12, true, true },
				{ IPAddress(L"172.24.2.5"), IPAddress(L"172.16.0.0"), 12, true, true },
				{ IPAddress(L"172.40.10.50"), IPAddress(L"172.16.0.0"), 12, true, false },
				{ IPAddress(L"172.15.10.50"), IPAddress(L"172.16.0.0"), 12, true, false },
				{ IPAddress(L"172.17.10.50"), IPAddress(L"172.16.0.0"), 12, true, true },
				{ IPAddress(L"172.16.0.0"), IPAddress(L"172.16.0.0"), 12, true, true },
				{ IPAddress(L"172.31.255.255"), IPAddress(L"172.16.0.0"), 12, true, true }
			};

			for (const auto& test : iptests)
			{
				BinaryIPAddress mask;
				if (BinaryIPAddress::CreateMask(test.netip.GetFamily(), test.cidr, mask))
				{
					const auto range = BinaryIPAddress::GetAddressRange(test.netip.GetBinary(), mask);
					if (range)
					{
						const auto[success, inrange] = BinaryIPAddress::IsInAddressRange(test.ip.GetBinary(),
																						 range->first, range->second);
						Assert::AreEqual(test.success, success);
						Assert::AreEqual(test.inrange, inrange);
					}
				}
			}
		}

		TEST_METHOD(GetAddressRange)
		{
			struct IPTest
			{
				IPAddress netip;
				UInt8 cidr{ 0 };
				bool success{ false };
				IPAddress start;
				IPAddress end;
			};

			const std::vector<IPTest> iptests
			{
				{ IPAddress(L"172.16.0.0"), 12, true, IPAddress(L"172.16.0.0"), IPAddress(L"172.31.255.255") },
				{ IPAddress(L"169.254.0.0"), 16, true, IPAddress(L"169.254.0.0"), IPAddress(L"169.254.255.255") },
				{ IPAddress(L"169.254.0.0"), 33, false, IPAddress(L"169.254.0.0"), IPAddress(L"169.254.255.255") },
				{ IPAddress(L"127.0.0.0"), 8, true, IPAddress(L"127.0.0.0"), IPAddress(L"127.255.255.255") },
				{ IPAddress(L"192.168.0.0"), 16, true, IPAddress(L"192.168.0.0"), IPAddress(L"192.168.255.255") },
				{ IPAddress(L"10.0.0.0"), 8, true, IPAddress(L"10.0.0.0"), IPAddress(L"10.255.255.255") },
				{ IPAddress(L"0.0.0.0"), 8, true, IPAddress(L"0.0.0.0"), IPAddress(L"0.255.255.255") },

				{ IPAddress(L"fc00::"), 7, true, IPAddress(L"fc00::"), IPAddress(L"fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff") },
				{ IPAddress(L"fd00::"), 8, true, IPAddress(L"fd00::"), IPAddress(L"fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff") },
				{ IPAddress(L"fe80::"), 10, true, IPAddress(L"fe80::"), IPAddress(L"febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff") },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), 129, false, IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), IPAddress(L"fe80:c11a:3a9c:ef10:e796:0:ffff:ffff") },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), 96, true, IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), IPAddress(L"fe80:c11a:3a9c:ef10:e796:0:ffff:ffff") },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), 80, true, IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), IPAddress(L"fe80:c11a:3a9c:ef10:e796:ffff:ffff:ffff") },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), 56, true, IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), IPAddress(L"fe80:c11a:3a9c:efff:ffff:ffff:ffff:ffff") },
				{ IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), 1, true, IPAddress(L"fe80:c11a:3a9c:ef10:e796::"), IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff") }
			};

			for (const auto& test : iptests)
			{
				BinaryIPAddress mask;
				if (BinaryIPAddress::CreateMask(test.netip.GetFamily(), test.cidr, mask))
				{
					const auto range = BinaryIPAddress::GetAddressRange(test.netip.GetBinary(), mask);
					Assert::AreEqual(test.success, range.has_value());
					if (range)
					{
						Assert::AreEqual(true, test.start == range->first);
						Assert::AreEqual(true, test.end == range->second);
					}
				}
			}
		}

		TEST_METHOD(IsMask)
		{
			Assert::AreEqual(true, IPAddress(L"0.0.0.0").IsMask());
			Assert::AreEqual(true, IPAddress(L"128.0.0.0").IsMask());
			Assert::AreEqual(true, IPAddress(L"192.0.0.0").IsMask());
			Assert::AreEqual(true, IPAddress(L"255.255.255.255").IsMask());
			Assert::AreEqual(true, IPAddress(L"255.255.255.0").IsMask());
			Assert::AreEqual(true, IPAddress(L"255.255.254.0").IsMask());
			Assert::AreEqual(true, IPAddress(L"255.255.0.0").IsMask());
			Assert::AreEqual(true, IPAddress(L"255.0.0.0").IsMask());
			Assert::AreEqual(true, IPAddress(L"255.192.0.0").IsMask());
			Assert::AreEqual(true, IPAddress(L"255.254.0.0").IsMask());
			Assert::AreEqual(false, IPAddress(L"255.254.254.0").IsMask());
			Assert::AreEqual(false, IPAddress(L"255.254.111.0").IsMask());
			Assert::AreEqual(false, IPAddress(L"255.255.255.232").IsMask());
			Assert::AreEqual(false, IPAddress(L"0.0.255.255").IsMask());
			Assert::AreEqual(false, IPAddress(L"0.111.255.255").IsMask());
			Assert::AreEqual(false, IPAddress(L"232.0.0.0").IsMask());
			Assert::AreEqual(false, IPAddress(L"254.255.255.255").IsMask());
			Assert::AreEqual(false, IPAddress(L"0.0.0.1").IsMask());

			Assert::AreEqual(true, IPAddress(L"::").IsMask());
			Assert::AreEqual(true, IPAddress(L"8000::").IsMask());
			Assert::AreEqual(true, IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").IsMask());
			Assert::AreEqual(true, IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff::").IsMask());
			Assert::AreEqual(true, IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:fff8::").IsMask());
			Assert::AreEqual(true, IPAddress(L"ffff:ffff:ffff:ffff:fffc::").IsMask());

			Assert::AreEqual(false, IPAddress(L"ffff:ffff:ffff:ffff:fffd:ffff:ffff:ffff").IsMask());
			Assert::AreEqual(false, IPAddress(L"0000:ffff:ffff:ffff:ffff:ffff:ffff:ffff").IsMask());
			Assert::AreEqual(false, IPAddress(L"0001:ffff:ffff:ffff:ffff:ffff:ffff:ffff").IsMask());
			Assert::AreEqual(false, IPAddress(L"ffff:ffff:ffff:8000:ffff:ffff:ffff:ffff").IsMask());
			Assert::AreEqual(false, IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:fffd::").IsMask());
			Assert::AreEqual(false, IPAddress(L"::ffff:ffff:ffff:ffff:ffff:ffff").IsMask());
			Assert::AreEqual(false, IPAddress(L"::000f").IsMask());
		}

		TEST_METHOD(IsLocal)
		{
			Assert::AreEqual(true, IPAddress(L"127.0.0.1").IsLocal());
			Assert::AreEqual(true, IPAddress(L"127.10.0.1").IsLocal());
			Assert::AreEqual(true, IPAddress(L"0.20.110.14").IsLocal());
			Assert::AreEqual(true, IPAddress(L"169.254.10.114").IsLocal());
			Assert::AreEqual(true, IPAddress(L"192.168.110.214").IsLocal());
			Assert::AreEqual(true, IPAddress(L"10.167.110.214").IsLocal());
			Assert::AreEqual(true, IPAddress(L"172.16.110.214").IsLocal());
			Assert::AreEqual(true, IPAddress(L"172.17.110.214").IsLocal());

			Assert::AreEqual(false, IPAddress(L"128.10.0.1").IsLocal());
			Assert::AreEqual(false, IPAddress(L"1.20.110.14").IsLocal());
			Assert::AreEqual(false, IPAddress(L"169.255.10.114").IsLocal());
			Assert::AreEqual(false, IPAddress(L"192.167.110.214").IsLocal());
			Assert::AreEqual(false, IPAddress(L"11.167.110.214").IsLocal());
			Assert::AreEqual(false, IPAddress(L"172.50.110.214").IsLocal());
			Assert::AreEqual(false, IPAddress(L"172.0.110.214").IsLocal());
			Assert::AreEqual(false, IPAddress(L"171.16.110.214").IsLocal());

			Assert::AreEqual(true, IPAddress(L"::").IsLocal());
			Assert::AreEqual(true, IPAddress(L"00f0::").IsLocal());
			Assert::AreEqual(true, IPAddress(L"fc00:3a9c:ef10:e796::").IsLocal());
			Assert::AreEqual(true, IPAddress(L"fc10:3a9c:ef10:e796::").IsLocal());
			Assert::AreEqual(true, IPAddress(L"fd00:3a9c:ef10:e796::").IsLocal());
			Assert::AreEqual(true, IPAddress(L"fd01:3a9c:ef10:e796::").IsLocal());
			Assert::AreEqual(true, IPAddress(L"fec0:3a9c:ef10:e796::").IsLocal());
			Assert::AreEqual(true, IPAddress(L"fe80:3a9c:ef10:e796::").IsLocal());
			Assert::AreEqual(true, IPAddress(L"feb0:3a9c:ef10:e796::").IsLocal());
			Assert::AreEqual(true, IPAddress(L"::1").IsLocal());
			Assert::AreEqual(false, IPAddress(L"01f0:3a9c:ef10:e796::").IsLocal());
			Assert::AreEqual(false, IPAddress(L"fe00:3a9c:ef10:e796::").IsLocal());
			Assert::AreEqual(false, IPAddress(L"ff00:3a9c:ef10:e796::").IsLocal());
		}

		TEST_METHOD(IsMulticast)
		{
			Assert::AreEqual(true, IPAddress(L"225.120.10.44").IsMulticast());
			Assert::AreEqual(true, IPAddress(L"232.220.110.14").IsMulticast());
			Assert::AreEqual(false, IPAddress(L"240.20.10.34").IsMulticast());
			Assert::AreEqual(false, IPAddress(L"140.120.50.24").IsMulticast());
			Assert::AreEqual(false, IPAddress(L"0.0.0.0").IsMulticast());
			Assert::AreEqual(false, IPAddress(L"255.255.255.255").IsMulticast());

			Assert::AreEqual(true, IPAddress(L"ff80:c11a:3a9c:ef10:e796::").IsMulticast());
			Assert::AreEqual(true, IPAddress(L"ffc0:e11a:3a9c:ef10:e796::").IsMulticast());
			Assert::AreEqual(true, IPAddress(L"ff70:c11a:3a9c:ef10:e796::").IsMulticast());
			Assert::AreEqual(false, IPAddress(L"fd70:c11a:3a9c:ef10:e796::").IsMulticast());
			Assert::AreEqual(false, IPAddress(L"fe90:c11a::").IsMulticast());
			Assert::AreEqual(false, IPAddress(L"::").IsMulticast());
			Assert::AreEqual(true, IPAddress(L"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").IsMulticast());
		}

		TEST_METHOD(IsReserved)
		{
			Assert::AreEqual(true, IPAddress(L"240.0.0.0").IsReserved());
			Assert::AreEqual(true, IPAddress(L"240.10.20.30").IsReserved());
			Assert::AreEqual(true, IPAddress(L"241.10.20.30").IsReserved());
			Assert::AreEqual(true, IPAddress(L"248.10.20.30").IsReserved());
			Assert::AreEqual(true, IPAddress(L"250.10.20.30").IsReserved());

			Assert::AreEqual(false, IPAddress(L"224.10.20.30").IsReserved());
			Assert::AreEqual(false, IPAddress(L"223.10.20.30").IsReserved());
			Assert::AreEqual(false, IPAddress(L"208.10.20.30").IsReserved());
			Assert::AreEqual(false, IPAddress(L"15.10.20.30").IsReserved());
		}

		TEST_METHOD(IsClassX)
		{
			Assert::AreEqual(true, IPAddress(L"0.1.1.1").IsClassA());
			Assert::AreEqual(true, IPAddress(L"5.1.1.1").IsClassA());
			Assert::AreEqual(true, IPAddress(L"45.25.1.1").IsClassA());
			Assert::AreEqual(true, IPAddress(L"127.25.1.1").IsClassA());
			Assert::AreEqual(false, IPAddress(L"128.25.1.1").IsClassA());
			Assert::AreEqual(false, IPAddress(L"200.25.1.1").IsClassA());

			Assert::AreEqual(true, IPAddress(L"160.1.1.1").IsClassB());
			Assert::AreEqual(true, IPAddress(L"128.1.1.1").IsClassB());
			Assert::AreEqual(false, IPAddress(L"45.25.1.1").IsClassB());
			Assert::AreEqual(false, IPAddress(L"127.25.1.1").IsClassB());
			Assert::AreEqual(false, IPAddress(L"0.25.1.1").IsClassB());

			Assert::AreEqual(true, IPAddress(L"205.1.1.1").IsClassC());
			Assert::AreEqual(true, IPAddress(L"208.1.1.1").IsClassC());
			Assert::AreEqual(false, IPAddress(L"176.25.1.1").IsClassC());
			Assert::AreEqual(false, IPAddress(L"127.25.1.1").IsClassC());
			Assert::AreEqual(false, IPAddress(L"0.25.1.1").IsClassC());

			Assert::AreEqual(true, IPAddress(L"224.1.1.1").IsClassD());
			Assert::AreEqual(true, IPAddress(L"239.1.1.1").IsClassD());
			Assert::AreEqual(false, IPAddress(L"255.25.1.1").IsClassD());
			Assert::AreEqual(false, IPAddress(L"127.25.1.1").IsClassD());
			Assert::AreEqual(false, IPAddress(L"0.25.1.1").IsClassD());

			Assert::AreEqual(true, IPAddress(L"240.1.1.1").IsClassE());
			Assert::AreEqual(true, IPAddress(L"255.1.1.1").IsClassE());
			Assert::AreEqual(false, IPAddress(L"208.25.1.1").IsClassE());
			Assert::AreEqual(false, IPAddress(L"127.25.1.1").IsClassE());
			Assert::AreEqual(false, IPAddress(L"0.25.1.1").IsClassE());
		}

		TEST_METHOD(Constexpr)
		{
			constexpr auto bin_ip = BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 192 }, Byte{ 168 }, Byte{ 1 }, Byte{ 1 });
			constexpr IPAddress ip(bin_ip);
			constexpr BinaryIPAddress bin_ip2 = ip.GetBinary();
			constexpr auto family = ip.GetFamily();

			static_assert(family == IPAddress::Family::IPv4, "Should be equal");
			static_assert(bin_ip2 == bin_ip, "Should be equal");

			Assert::AreEqual(true, family == IPAddress::Family::IPv4);
			Assert::AreEqual(true, bin_ip2 == bin_ip);

			constexpr auto ipa4 = IPAddress::AnyIPv4();
			constexpr auto ipa6 = IPAddress::AnyIPv6();

			constexpr auto iplb4 = IPAddress::LoopbackIPv4();
			static_assert(iplb4.GetFamily() == IPAddress::Family::IPv4, "Should be equal");

			constexpr auto iplb6 = IPAddress::LoopbackIPv6();
			static_assert(iplb6.GetFamily() == IPAddress::Family::IPv6, "Should be equal");

			constexpr auto bin_ip3 = BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 127 }, Byte{ 0 }, Byte{ 0 }, Byte{ 1 });
			constexpr IPAddress iplb42(bin_ip3);
			static_assert(iplb4 == iplb42, "Should be equal");
			Assert::AreEqual(true, iplb4 == iplb42);
		}
	};
}