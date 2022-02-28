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
			BTHAddress bth4(std::move(bth2));
			Assert::AreEqual(true, bth3 == bth4);

			// Copy assignment
			bth1 = bth3;
			Assert::AreEqual(true, bth3 == bth1);

			// Move assignment
			const auto bth5 = std::move(bth3);
			Assert::AreEqual(true, bth5 == bth1);

			// GetBinary
			Assert::AreEqual(true, bth1.GetBinary().AddressFamily == BinaryBTHAddress::Family::BTH);
			Assert::AreEqual(true, bth1.GetBinary().UInt64s == 0x925FD35B93B2);
			Assert::AreEqual(true,
							 (bth1.GetBinary().Bytes[0] == Byte{ 0xB2 }) &&
							 (bth1.GetBinary().Bytes[1] == Byte{ 0x93 }) &&
							 (bth1.GetBinary().Bytes[2] == Byte{ 0x5B }) &&
							 (bth1.GetBinary().Bytes[3] == Byte{ 0xD3 }) &&
							 (bth1.GetBinary().Bytes[4] == Byte{ 0x5F }) &&
							 (bth1.GetBinary().Bytes[5] == Byte{ 0x92 }));

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
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F:D3:5B:93:B2:"); }); // last : is invalid
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L":92:5F:D3:5B:93:B2:"); }); // first/last : is invalid
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F:D3:5B:93.B2)"); }); // . is invalid
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92.5F:D3:5B:93:B2)"); }); // . is invalid
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(9215F:D3:5B:93:B2)"); }); // 2 is invalid
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5Z:D3:5B:93:B2)"); }); // 5Z is invalid
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F:D3:5B:GA:B2)"); }); // GA is invalid
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F:D3:5B:93: B2)"); }); // Extra space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"92:5F:D3:5B:93:B2"); }); // No parenthesis
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L" 92:5F:D3:5B:93:B2 "); }); // No parenthesis
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"((2:5F:D3:5B:93:B2)"); }); // Extra parenthesis
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"( 2:5F:D3:5B:93:B2)"); }); // Space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(  :5F:D3:5B:93:B2)"); }); // Space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(   5F:D3:5B:93:B2)"); }); // Space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(2 :5F:D3:5B:93:B2)"); }); // Space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F: 3:5B:93:B2)"); }); // Space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F:3 :5B:93:B2)"); }); // Space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F:D3:5B:93:B )"); }); // Space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F:D3:5B:93: B)"); }); // Space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(92:5F:D3:5B:93:  )"); }); // Space
			Assert::ExpectException<std::invalid_argument>([] { BTHAddress(L"(9::5F:D3:5B:93:B2)"); }); // extra :

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

			Assert::AreEqual(true, BTHAddress::TryParse(L"(00:11:22:33:FF:EE)", address));
			Assert::AreEqual(true, BTHAddress::TryParse(L"(01:23:45:67:89:AB)", address));
			Assert::AreEqual(true, BTHAddress::TryParse(L"(00:25:96:12:34:56)", address));
			Assert::AreEqual(true, BTHAddress::TryParse(L"(00:0a:95:9d:68:16)", address));
			Assert::AreEqual(true, BTHAddress::TryParse(L"(3B:7D:25:2E:C6:87)", address));
			Assert::AreEqual(true, BTHAddress::TryParse(L"(17:52:06:A6:0F:96)", address));
		}

		TEST_METHOD(Constexpr)
		{
			// Default construction
			constexpr BTHAddress bth1;
			static_assert(bth1.GetFamily() == BTHAddress::Family::BTH, "Should be equal");
			static_assert(bth1.GetBinary() == BTHAddress::AnyBTH(), "Should be equal");
			Assert::AreEqual(true, bth1.GetString() == L"(00:00:00:00:00:00)");

			// Construction
			constexpr auto bin_bth = BinaryBTHAddress(BinaryBTHAddress::Family::BTH, 0x925FD35B93B2);
			constexpr BTHAddress bth2(bin_bth);
			constexpr auto bin_bth2 = bth2.GetBinary();
			constexpr auto family = bth2.GetFamily();

			static_assert(family == BTHAddress::Family::BTH, "Should be equal");
			static_assert(bin_bth2 == bin_bth, "Should be equal");

			Assert::AreEqual(true, family == BTHAddress::Family::BTH);
			Assert::AreEqual(true, bin_bth2 == bin_bth);

			// Copy construction
			constexpr BTHAddress bth3(bth2);
			static_assert(bth3.GetFamily() == BTHAddress::Family::BTH, "Should be equal");
			static_assert(bth3.GetBinary() == bin_bth2, "Should be equal");
			Assert::AreEqual(true, bth3.GetString() == L"(92:5F:D3:5B:93:B2)");

			// Equal and not equal
			static_assert(bth2 == bth3, "Should be equal");
			static_assert(!(bth2 != bth3), "Should not be equal");
			static_assert(bth1 != bth2, "Should not be equal");

			// Move construction
			constexpr BTHAddress bth4(std::move(bth2));
			static_assert(bth4 == bth2, "Should be equal");
			static_assert(bth4.GetFamily() == BTHAddress::Family::BTH, "Should be equal");
			static_assert(bth4.GetBinary() == bin_bth2, "Should be equal");
			Assert::AreEqual(true, bth4.GetString() == L"(92:5F:D3:5B:93:B2)");

			// Copy assignment
			constexpr auto bth5 = bth3;
			static_assert(bth5 == bth3, "Should be equal");

			// Move assignment
			constexpr auto bth6 = std::move(bth3);
			static_assert(bth6 == bth3, "Should be equal");
		}
	};
}