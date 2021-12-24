// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\BinaryBTHAddress.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	TEST_CLASS(BinaryBTHAddressTests)
	{
	public:
		TEST_METHOD(Constexpr)
		{
			// Default constructor
			constexpr BinaryBTHAddress bth;
			static_assert(bth.AddressFamily == BinaryBTHAddress::Family::Unspecified);
			static_assert(bth.UInt64s == 0);

			// UInt64 constructor
			constexpr BinaryBTHAddress bth1(BinaryBTHAddress::Family::BTH, 0x925FD35B93B2);
			static_assert(bth1.AddressFamily == BinaryBTHAddress::Family::BTH);
			static_assert(bth1.UInt64s == 0x925FD35B93B2);
			Assert::AreEqual(true,
							 (bth1.Bytes[0] == Byte{ 0xB2 }) &&
							 (bth1.Bytes[1] == Byte{ 0x93 }) &&
							 (bth1.Bytes[2] == Byte{ 0x5B }) &&
							 (bth1.Bytes[3] == Byte{ 0xD3 }) &&
							 (bth1.Bytes[4] == Byte{ 0x5F }) &&
							 (bth1.Bytes[5] == Byte{ 0x92 }));

			// Copy constructor
			constexpr BinaryBTHAddress bth2(bth1);
			static_assert(bth2.AddressFamily == BinaryBTHAddress::Family::BTH);
			static_assert(bth2.UInt64s == 0x925FD35B93B2);

			// Move constructor
			constexpr BinaryBTHAddress bth3(std::move(bth2));
			static_assert(bth3 == bth1, "Should match");
			static_assert(bth3 != bth, "Should not match");

			// Copy assignment
			constexpr BinaryBTHAddress bth4 = bth1;
			static_assert(bth4.AddressFamily == BinaryBTHAddress::Family::BTH);
			static_assert(bth4.UInt64s == 0x925FD35B93B2);

			// Move assignment
			constexpr BinaryBTHAddress bth5 = std::move(bth4);
			static_assert(bth5 == bth1, "Should match");
			static_assert(bth5 != bth, "Should not match");
		}
	};
}