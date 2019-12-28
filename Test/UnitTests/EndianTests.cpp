// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Common\Endian.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(EndianTests)
	{
	public:
		TEST_METHOD(General)
		{
			UInt16 num16{ 0xFA00 };
			auto num16nbo = Endian::ToNetworkByteOrder(num16);
			Assert::AreEqual(true, num16nbo == 0x00FA);
			auto num16le = Endian::FromNetworkByteOrder(num16nbo);
			Assert::AreEqual(true, num16le == num16);

			UInt32 num32{ 0xEDFCBAEE };
			auto num32nbo = Endian::ToNetworkByteOrder(num32);
			Assert::AreEqual(true, num32nbo == 0xEEBAFCED);
			auto num32le = Endian::FromNetworkByteOrder(num32nbo);
			Assert::AreEqual(true, num32le == num32);

			UInt64 num64{ 0xBDBDECADECADFFCC };
			auto num64nbo = Endian::ToNetworkByteOrder(num64);
			Assert::AreEqual(true, num64nbo == 0xCCFFADECADECBDBD);
			auto num64le = Endian::FromNetworkByteOrder(num64nbo);
			Assert::AreEqual(true, num64le == num64);
		}
	};
}