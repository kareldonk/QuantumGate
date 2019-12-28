// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Common\Hash.h"
#include "Common\Util.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(HashTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Persistent hash; these hashes should always be the same
			// when relaunching the program/test
			{
				UInt64 num1 = 18446744073709551615;
				UInt64 num2 = 369369369369369369;

				UInt64 hash1 = Hash::GetPersistentHash(num1);
				UInt64 hash2 = Hash::GetPersistentHash(num2);

				String quote = L"If you want to be incrementally better: Be competitive."
								"If you want to be exponentially better: Be cooperative. – Unknown";

				UInt64 hash3 = Hash::GetPersistentHash(quote);

				auto len = quote.size() * sizeof(String::value_type);
				Buffer buf(reinterpret_cast<Byte*>(quote.data()), len);

				UInt64 hash4 = Hash::GetPersistentHash(buf);

				Assert::AreEqual((UInt64)13026701458831495580, hash1);
				Assert::AreEqual((UInt64)529285802039577983, hash2);
				Assert::AreEqual((UInt64)13229672043214846096, hash3);
				Assert::AreEqual((UInt64)13229672043214846096, hash4);
			}

			// Nonpersistent hash; these hashes will be unique with each run,
			// but different everytime
			{
				UInt64 num1 = 18446744073709551615;

				UInt64 hash1 = Hash::GetNonPersistentHash(num1);
				UInt64 hash2 = Hash::GetNonPersistentHash(num1);

				String quote = L"If you want to be incrementally better: Be competitive."
								"If you want to be exponentially better: Be cooperative. – Unknown";

				UInt64 hash3 = Hash::GetNonPersistentHash(quote);

				auto len = quote.size() * sizeof(String::value_type);
				Buffer buf(reinterpret_cast<Byte*>(quote.data()), len);

				UInt64 hash4 = Hash::GetNonPersistentHash(quote);

				Assert::AreEqual(hash1, hash2);
				Assert::AreEqual(hash3, hash4);
				Assert::AreEqual(false, hash1 == hash3);
			}
		}
	};
}