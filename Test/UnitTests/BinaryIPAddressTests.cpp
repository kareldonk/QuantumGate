// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"
#include "Network\BinaryIPAddress.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	TEST_CLASS(BinaryIPAddressTests)
	{
	public:
		TEST_METHOD(Constexpr)
		{
			// Byte constructor
			constexpr BinaryIPAddress ip1(BinaryIPAddress::Family::IPv4, Byte{ 192 }, Byte{ 168 }, Byte{ 10 }, Byte{ 12 });

			// UInt32 constructor
			constexpr BinaryIPAddress ip2(0xffff0000); // 255.255.0.0

			// UInt64 constructor
			constexpr BinaryIPAddress ipv6(0x0, 0x0000000000000001); // ::1
			
			// Copy constructor
			constexpr BinaryIPAddress ip3(ip1);

			// Move constructor
			constexpr BinaryIPAddress ip4(std::move(ip3));
			static_assert(ip4 == ip1, "Should match");

			// Copy assignment
			constexpr BinaryIPAddress ip5 = ip2;

			// Move assignment
			constexpr BinaryIPAddress ip6 = std::move(ip5);
			static_assert(ip6 == ip2, "Should match");

			{
				// Operator ^
				constexpr auto ipx = ip1 ^ ip2;

				// Operator &
				constexpr auto ipa = ip1 & ip2;

				// Operator |
				constexpr auto ipo = ip1 | ip2;

				// Operator ~
				constexpr auto ipf = ~ip1;
			}

			// GetNumAddressBytes
			static_assert(ip1.GetNumAddressBytes() == 4, "Should match");
			static_assert(ipv6.GetNumAddressBytes() == 16, "Should match");

			// GetAddressByte
			constexpr auto byte = BinaryIPAddress().GetAddressByte(0);

			// GetAddressRange
			constexpr auto range = BinaryIPAddress::GetAddressRange(ip1, ip2);

			// IsInAddressRange
			{
				constexpr BinaryIPAddress iprt(BinaryIPAddress::Family::IPv4, Byte{ 192 }, Byte{ 168 }, Byte{ 10 }, Byte{ 13 });

				constexpr auto result = BinaryIPAddress::IsInAddressRange(iprt, range->first, range->second);
				static_assert(result.first, "Should succeed");
				static_assert(result.second, "Should be in range");

				constexpr BinaryIPAddress iprt2(BinaryIPAddress::Family::IPv4, Byte{ 192 }, Byte{ 167 }, Byte{ 10 }, Byte{ 10 });

				constexpr auto result2 = BinaryIPAddress::IsInAddressRange(iprt2, range->first, range->second);
				static_assert(result2.first, "Should succeed");
				static_assert(!result2.second, "Should not be in range");
			}

			// CreateMask
			constexpr auto mask4 = BinaryIPAddress::CreateMask(BinaryIPAddress::Family::IPv4, 16);
			static_assert(mask4 == ip2, "Should match");

			constexpr auto mask6 = BinaryIPAddress::CreateMask(BinaryIPAddress::Family::IPv6, 48);
			static_assert(mask6 == BinaryIPAddress(0xffffffffffff0000, 0x0), "Should match");

			// IsMask
			static_assert(BinaryIPAddress::IsMask(mask4), "Should be a mask");
			static_assert(BinaryIPAddress::IsMask(mask6), "Should be a mask");
			static_assert(!BinaryIPAddress::IsMask(BinaryIPAddress(0xffff0001)), "Should not be a mask");
			static_assert(!BinaryIPAddress::IsMask(BinaryIPAddress(0xffef0000)), "Should not be a mask");
			static_assert(!BinaryIPAddress::IsMask(BinaryIPAddress(0xffffffffffff0010, 0x0)), "Should not be a mask");

			// GetNetwork
			constexpr BinaryIPAddress network1 = [](const BinaryIPAddress& bin_ipaddr,
												   const BinaryIPAddress& bin_mask) constexpr
			{
				BinaryIPAddress network;
				if (BinaryIPAddress::GetNetwork(bin_ipaddr, bin_mask, network)) return network;
				return BinaryIPAddress{};
			}(ip1, mask4);
			
			static_assert(network1 == BinaryIPAddress(0xc0a80000), "Should succeed");

			constexpr BinaryIPAddress network2 = [](const BinaryIPAddress& bin_ipaddr,
													const UInt8 cidr_lbits) constexpr
			{
				BinaryIPAddress network;
				if (BinaryIPAddress::GetNetwork(bin_ipaddr, cidr_lbits, network)) return network;
				return BinaryIPAddress{};
			}(ip1, 8);

			static_assert(network2 == BinaryIPAddress(0xc0000000), "Should succeed");

			// AreInSameNetwork
			{
				constexpr auto aisnres = BinaryIPAddress::AreInSameNetwork(ip1,
																			BinaryIPAddress(0xc0a80000), 16);
				static_assert(aisnres.first, "Should succeed");
				static_assert(aisnres.second, "Should be in same network");
			}

			{
				constexpr auto aisnres = BinaryIPAddress::AreInSameNetwork(ip1,
																		   BinaryIPAddress(0xc0180000), 16);
				static_assert(aisnres.first, "Should succeed");
				static_assert(!aisnres.second, "Should not be in same network");
			}
		}

		TEST_METHOD(GetAddressByte)
		{
			struct IPTest
			{
				BinaryIPAddress ip;
			};

			const std::vector<IPTest> iptests
			{
				{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 172 }, Byte{ 16 }, Byte{ 0 }, Byte{ 0 }) },
				{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 192 }, Byte{ 168 }, Byte{ 1 }, Byte{ 20 }) },
				{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 240 }, Byte{ 1 }, Byte{ 1 }, Byte{ 1 }) },
				{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 127 }, Byte{ 0 }, Byte{ 0 }, Byte{ 1 }) },
				{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 223 }, Byte{ 10 }, Byte{ 20 }, Byte{ 30 }) },
				{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 }) },
				{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 0 }, Byte{ 0 }, Byte{ 253 }, Byte{ 255 }) },
				{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 255 }, Byte{ 255 }, Byte{ 255 }, Byte{ 255 }) },
				{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 255 }, Byte{ 254 }, Byte{ 254 }, Byte{ 0 }) },
				{ BinaryIPAddress(0xfe80c11a3a9cef10, 0xe796000000000000) },
				{ BinaryIPAddress(0xdeadbeeffeedface, 0xcafebabebaadc0de) },
				{ BinaryIPAddress(0xe835625f48cec433, 0x7c5dea376c3ca00) },
				{ BinaryIPAddress(0xffffffffffffffff, 0xffffffffffffffff) },
				{ BinaryIPAddress(0xfffffff1fffe0000, 0x00000000ffffffff) },
				{ BinaryIPAddress(0x0000000000000000, 0x0000000000000000) },
				{ BinaryIPAddress(0x0000000000000000, 0x0000000000000001) }
			};

			for (const auto& test : iptests)
			{
				for (auto x = 0u; x < test.ip.GetNumAddressBytes(); ++x)
				{
					const auto b1 = test.ip.Bytes[x];
					const auto b2 = test.ip.GetAddressByte(x);
					Assert::AreEqual(true, b1 == b2);
				}
			}
		}
	};
}