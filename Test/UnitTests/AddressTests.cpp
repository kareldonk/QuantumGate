// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\Address.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	TEST_CLASS(AddressTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Default construction
			Address addr;
			Assert::AreEqual(true, addr.GetString() == L"Unspecified");
			Assert::AreEqual(true, addr.GetType() == Address::Type::Unspecified);
			Assert::AreEqual(true, addr.GetFamily() == Address::Family::Unspecified);

			// Construction
			Address addr2(BTHAddress(L"(92:5F:D3:5B:93:B2)"));
			Assert::AreEqual(true, addr2.GetString() == L"(92:5F:D3:5B:93:B2)");
			Assert::AreEqual(true, addr2.GetType() == Address::Type::BTH);
			Assert::AreEqual(true, addr2.GetFamily() == Address::Family::BTH);
			Assert::AreEqual(true, addr2.GetBTHAddress().GetBinary().UInt64s == 0x925FD35B93B2);

			Address addr3(IPAddress(L"192.168.0.1"));
			Assert::AreEqual(true, addr3.GetString() == L"192.168.0.1");
			Assert::AreEqual(true, addr3.GetType() == Address::Type::IP);
			Assert::AreEqual(true, addr3.GetFamily() == Address::Family::IPv4);
			Assert::AreEqual(true, addr3.GetIPAddress().GetBinary().UInt32s[0] == 0x0100A8C0);

			// Copy construction
			Address addr4(addr2);
			Assert::AreEqual(true, addr4.GetString() == L"(92:5F:D3:5B:93:B2)");
			Assert::AreEqual(true, addr4.GetFamily() == Address::Family::BTH);

			// Equal and not equal
			Assert::AreEqual(true, addr2 == addr4);
			Assert::AreEqual(false, addr2 != addr4);
			Assert::AreEqual(true, addr2 != addr3);

			// Move construction
			Address addr5(std::move(addr2));
			Assert::AreEqual(true, addr5 == addr4);

			// Copy assignment
			addr = addr5;
			Assert::AreEqual(true, addr.GetString() == L"(92:5F:D3:5B:93:B2)");
			Assert::AreEqual(true, addr.GetType() == Address::Type::BTH);
			Assert::AreEqual(true, addr.GetFamily() == Address::Family::BTH);
			Assert::AreEqual(true, addr.GetBTHAddress().GetBinary().UInt64s == 0x925FD35B93B2);

			Assert::AreEqual(true, addr5 == addr);

			// Move assignment
			const auto addr6 = std::move(addr5);
			Assert::AreEqual(true, addr6 == addr);

			// Move assignment of different address type
			addr = std::move(addr3);
			Assert::AreEqual(true, addr.GetString() == L"192.168.0.1");
			Assert::AreEqual(true, addr.GetType() == Address::Type::IP);
			Assert::AreEqual(true, addr.GetFamily() == Address::Family::IPv4);
			Assert::AreEqual(true, addr.GetIPAddress().GetBinary().UInt32s[0] == 0x0100A8C0);
		}

		TEST_METHOD(Constexpr)
		{
			// Default construction
			constexpr Address addr;
			static_assert(addr.GetType() == Address::Type::Unspecified, "Should be equal");
			static_assert(addr.GetFamily() == Address::Family::Unspecified, "Should be equal");
			Assert::AreEqual(true, addr.GetString() == L"Unspecified");

			// Construction
			constexpr auto bin_bth = BinaryBTHAddress(BinaryBTHAddress::Family::BTH, 0x925FD35B93B2);
			constexpr auto bth_addr = BTHAddress(bin_bth);
			constexpr Address addr2(bth_addr);
			constexpr auto bin_addr = addr2.GetBTHAddress().GetBinary();
			constexpr auto type = addr2.GetType();

			static_assert(type == Address::Type::BTH, "Should be equal");
			static_assert(bin_addr == bin_bth, "Should be equal");

			Assert::AreEqual(true, type == Address::Type::BTH);
			Assert::AreEqual(true, bin_addr == bin_bth);

			constexpr auto bin_ip = BinaryIPAddress(0xC0A80001);
			constexpr auto ip_addr = IPAddress(bin_ip);
			constexpr Address addr3(ip_addr);
			constexpr auto bin_addr2 = addr3.GetIPAddress().GetBinary();
			constexpr auto type2 = addr3.GetType();

			static_assert(type2 == Address::Type::IP, "Should be equal");
			static_assert(bin_addr2 == bin_ip, "Should be equal");

			// Copy construction
			constexpr Address addr4(addr2);
			static_assert(addr4.GetFamily() == Address::Family::BTH, "Should be equal");
			Assert::AreEqual(true, addr4.GetString() == L"(92:5F:D3:5B:93:B2)");

			// Equal and not equal
			static_assert(addr2 == addr4, "Should be equal");
			static_assert(!(addr2 != addr4), "Should not be equal");
			static_assert(addr2 != addr3, "Should not be equal");

			// Move construction
			constexpr Address addr5(std::move(addr2));
			static_assert(addr5 == addr4, "Should be equal");
			static_assert(addr5.GetType() == Address::Type::BTH, "Should be equal");
			static_assert(addr5.GetFamily() == Address::Family::BTH, "Should be equal");
			static_assert(addr5.GetBTHAddress().GetBinary() == bin_addr, "Should be equal");
			Assert::AreEqual(true, addr5.GetString() == L"(92:5F:D3:5B:93:B2)");

			// Copy assignment
			constexpr Address addr6 = addr5;
			static_assert(addr6 == addr5, "Should be equal");
			static_assert(addr6.GetType() == Address::Type::BTH, "Should be equal");
			static_assert(addr6.GetFamily() == Address::Family::BTH, "Should be equal");
			static_assert(addr6.GetBTHAddress().GetBinary().UInt64s == 0x925FD35B93B2, "Should be equal");
			Assert::AreEqual(true, addr6.GetString() == L"(92:5F:D3:5B:93:B2)");

			// Move assignment
			constexpr auto addr7 = std::move(addr5);
			static_assert(addr7 == addr5, "Should be equal");
			static_assert(addr7.GetType() == Address::Type::BTH, "Should be equal");
			static_assert(addr7.GetFamily() == Address::Family::BTH, "Should be equal");
			static_assert(addr7.GetBTHAddress().GetBinary() == bin_addr, "Should be equal");
			Assert::AreEqual(true, addr7.GetString() == L"(92:5F:D3:5B:93:B2)");

			// Move assignment of different address type
			constexpr auto addr8 = std::move(addr3);
			static_assert(addr8.GetType() == Address::Type::IP, "Should be equal");
			static_assert(addr8.GetFamily() == Address::Family::IPv4, "Should be equal");
			static_assert(addr8.GetIPAddress().GetBinary() == bin_ip, "Should be equal");
			Assert::AreEqual(true, addr8.GetString() == L"192.168.0.1");
		}
	};
}