// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\BTHAddress.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	TEST_CLASS(BTHAddressTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Default construction
			BTHAddress bth1;
			Assert::AreEqual(true, bth1.GetString() == L"(00:00:00:00:00:00)");
			Assert::AreEqual(true, bth1.GetFamily() == BTHAddress::Family::BTH);

			// Construction
			BTHAddress bth2(L"(92:5F:D3:5B:93:B2)");
			Assert::AreEqual(true, bth2.GetString() == L"(92:5F:D3:5B:93:B2)");
			Assert::AreEqual(true, bth2.GetFamily() == BTHAddress::Family::BTH);

			// Copy construction
			BTHAddress bth3(bth2);
			Assert::AreEqual(true, bth3.GetString() == L"(92:5F:D3:5B:93:B2)");
			Assert::AreEqual(true, bth3.GetFamily() == BTHAddress::Family::BTH);

			// Equal and not equal
			Assert::AreEqual(true, bth2 == bth3);
			Assert::AreEqual(false, bth2 != bth3);
			Assert::AreEqual(true, bth1 != bth2);

			// Move construction
			BTHAddress ip4(std::move(bth2));
			Assert::AreEqual(true, bth3 == ip4);

			// Copy assignment
			bth1 = bth3;
			Assert::AreEqual(true, bth3 == bth1);

			// Move assignment
			const auto bth4 = std::move(bth3);
			Assert::AreEqual(true, bth4 == bth1);

			// GetBinary
			Assert::AreEqual(true, bth1.GetBinary().AddressFamily == BinaryBTHAddress::Family::BTH);
			Assert::AreEqual(true, bth1.GetBinary().UInt64s == 0x925FD35B93B2);
			Assert::AreEqual(true,
							 (bth1.GetBinary().Bytes[0] == Byte{ 0xB2 }) &&
							 (bth1.GetBinary().Bytes[1] == Byte{ 0x93 }) &&
							 (bth1.GetBinary().Bytes[2] == Byte{ 0x5B }) &&
							 (bth1.GetBinary().Bytes[3] == Byte{ 0xD3 }) &&
							 (bth1.GetBinary().Bytes[4] == Byte{ 0x5F }) &&
							 (bth1.GetBinary().Bytes[5] == Byte{ 0x92 }) &&
							 (bth1.GetBinary().Bytes[6] == Byte{ 0x00 }) &&
							 (bth1.GetBinary().Bytes[7] == Byte{ 0x00 }));

			// GetFamily
			Assert::AreEqual(true, bth1.GetFamily() == BTHAddress::Family::BTH);

			const auto any_bth = BTHAddress::AnyBTH();
			Assert::AreEqual(true, any_bth.GetFamily() == BTHAddress::Family::BTH);
			Assert::AreEqual(true, any_bth.GetString() == L"(00:00:00:00:00:00)");
		}

		TEST_METHOD(Input)
		{
			// Test invalid addresses
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L""); });
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(00:00:00:00:00:00:00:00:00:00:00)"); });
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(0000000000000000000000000000000000)"); });
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"abcdadefbghtmjurfvbghtyhvfregthnmredfgertfghyjukiolj"); });
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5Z:D3:5B:93:B2)"); }); // 5Z is invalid
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F:D3:5B:93: B2)"); });

			BTHAddress address;
			Assert::AreEqual(false, BTHAddress::TryParse(L"", address));
			Assert::AreEqual(false, BTHAddress::TryParse(L"abcd", address));
			Assert::AreEqual(false, BTHAddress::TryParse(L"(92:5Z:D3:5B:93:B2)", address));
			Assert::AreEqual(false, BTHAddress::TryParse(L"(92:5F:D3:5B:93: B2)", address));

			// Test valid addresses
			Assert::AreEqual(true, BTHAddress::TryParse(L"(00:00:00:00:00:00)", address));
			Assert::AreEqual(true, address.GetString() == L"(00:00:00:00:00:00)");
			Assert::AreEqual(true, address.GetFamily() == BTHAddress::Family::BTH);

			Assert::AreEqual(true, BTHAddress::TryParse(L"(92:5F:D3:5B:93:B2)", address));
			Assert::AreEqual(true, address.GetString() == L"(92:5F:D3:5B:93:B2)");
			Assert::AreEqual(true, address.GetFamily() == BTHAddress::Family::BTH);
		}

		TEST_METHOD(Constexpr)
		{
			constexpr auto bin_bth = BinaryBTHAddress(BinaryBTHAddress::Family::BTH, 0x925FD35B93B2);
			constexpr BTHAddress bth(bin_bth);
			constexpr BinaryBTHAddress bin_bth2 = bth.GetBinary();
			constexpr auto family = bth.GetFamily();

			static_assert(family == BTHAddress::Family::BTH, "Should be equal");
			static_assert(bin_bth2 == bin_bth, "Should be equal");

			Assert::AreEqual(true, family == BTHAddress::Family::BTH);
			Assert::AreEqual(true, bin_bth2 == bin_bth);

			constexpr auto btha = BTHAddress::AnyBTH();
		}
	};
}