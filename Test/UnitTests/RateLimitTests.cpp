// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Common\RateLimit.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation;

namespace UnitTests
{
	TEST_CLASS(RateLimitTests)
	{
	public:
		TEST_METHOD(Construction)
		{
			// Constructor
			RateLimit<UInt16> rlimit;

			Assert::AreEqual(true, rlimit.GetCurrent() == 0);
			Assert::AreEqual(true, rlimit.GetMinimum() == 0);
			Assert::AreEqual(true, rlimit.GetMaximum() == std::numeric_limits<UInt16>::max());
			Assert::AreEqual(true, rlimit.GetAvailable() == std::numeric_limits<UInt16>::max());
			Assert::AreEqual(true, rlimit.CanAdd(std::numeric_limits<UInt16>::max()));
			rlimit.Add(std::numeric_limits<UInt16>::max());
			Assert::AreEqual(false, rlimit.CanAdd(std::numeric_limits<UInt16>::max()));
			Assert::AreEqual(true, rlimit.GetCurrent() == std::numeric_limits<UInt16>::max());
			Assert::AreEqual(true, rlimit.GetMinimum() == 0);
			Assert::AreEqual(true, rlimit.GetMaximum() == std::numeric_limits<UInt16>::max());
			Assert::AreEqual(true, rlimit.GetAvailable() == 0);

			// Value constructor
			const RateLimit<UInt16> rlimitv(10000);

			const RateLimit<UInt16> rlimitv1(10000000);
			Assert::AreEqual(true, rlimitv1.GetCurrent() == std::numeric_limits<UInt16>::max());

			const RateLimit<UInt16> rlimitv2(-1);
			Assert::AreEqual(true, rlimitv2.GetCurrent() == std::numeric_limits<UInt16>::min());

			const RateLimit<Int16, -200, 1000> rlimitv3(-1);
			Assert::AreEqual(true, rlimitv3.GetCurrent() == -1);

			const RateLimit<Int16, -200, 1000> rlimitv4(-201);
			Assert::AreEqual(true, rlimitv4.GetCurrent() == -200);

			// Copy constructor
			RateLimit<UInt16> rlimit2{ rlimit };
			Assert::AreEqual(false, rlimit2.CanAdd(1));
			Assert::AreEqual(true, rlimit2.CanSubtract(std::numeric_limits<UInt16>::max()));
			rlimit2.Subtract(std::numeric_limits<UInt16>::max());
			Assert::AreEqual(true, rlimit2.CanAdd(std::numeric_limits<UInt16>::max()));

			// Copy assignment
			auto rlimit3{ rlimit };
			Assert::AreEqual(false, rlimit3.CanAdd(1));
			Assert::AreEqual(true, rlimit3.CanSubtract(std::numeric_limits<UInt16>::max()));
			rlimit3.Subtract(std::numeric_limits<UInt16>::max());
			Assert::AreEqual(true, rlimit3.CanAdd(std::numeric_limits<UInt16>::max()));

			// Move constructor
			RateLimit<UInt16> rlimit4{ std::move(rlimit) };
			Assert::AreEqual(false, rlimit4.CanAdd(1));
			Assert::AreEqual(true, rlimit4.CanSubtract(std::numeric_limits<UInt16>::max()));
			rlimit4.Subtract(std::numeric_limits<UInt16>::max());
			Assert::AreEqual(true, rlimit4.CanAdd(std::numeric_limits<UInt16>::max()));

			rlimit4.Add(1000);

			Assert::AreEqual(true, rlimit4.GetCurrent() == 1000);
			Assert::AreEqual(true, rlimit4.GetMinimum() == 0);
			Assert::AreEqual(true, rlimit4.GetMaximum() == std::numeric_limits<UInt16>::max());
			Assert::AreEqual(true, rlimit4.GetAvailable() == std::numeric_limits<UInt16>::max() - 1000);
			Assert::AreEqual(true, rlimit4.CanAdd(std::numeric_limits<UInt16>::max() - 1000));
			Assert::AreEqual(false, rlimit4.CanAdd(std::numeric_limits<UInt16>::max()));

			// Move assignment
			auto rlimit5 = std::move(rlimit4);
			Assert::AreEqual(true, rlimit5.GetCurrent() == 1000);
			Assert::AreEqual(true, rlimit5.GetAvailable() == std::numeric_limits<UInt16>::max() - 1000);
			Assert::AreEqual(true, rlimit5.CanSubtract(500));
			rlimit5.Subtract(500);
			Assert::AreEqual(true, rlimit5.GetCurrent() == 500);
			Assert::AreEqual(true, rlimit5.GetAvailable() == std::numeric_limits<UInt16>::max() - 500);
			rlimit5.Subtract(500);
			Assert::AreEqual(true, rlimit5.GetCurrent() == 0);
			Assert::AreEqual(true, rlimit5.GetAvailable() == std::numeric_limits<UInt16>::max());
			Assert::AreEqual(false, rlimit5.CanAdd(10000000000000));
		}

		TEST_METHOD(AddAndSubtract)
		{
			{
				RateLimit<UInt8> rlimit;
				Assert::AreEqual(true, rlimit.CanAdd(std::numeric_limits<UInt8>::max()));
				Assert::AreEqual(false, rlimit.CanAdd(-1 * std::numeric_limits<UInt8>::max()));
				Assert::AreEqual(false, rlimit.CanSubtract(std::numeric_limits<UInt8>::max()));
				Assert::AreEqual(true, rlimit.CanSubtract(-1 * std::numeric_limits<UInt8>::max()));

				Assert::AreEqual(true, rlimit.CanAdd(10));
				rlimit.Add(10);
				Assert::AreEqual(true, rlimit.GetCurrent() == 10);
				Assert::AreEqual(false, rlimit.CanSubtract(11));
				Assert::AreEqual(true, rlimit.CanSubtract(10));
			}

			{
				RateLimit<Int16, -3000, 100> rlimit(0);
				Assert::AreEqual(false, rlimit.CanAdd(std::numeric_limits<Int16>::max()));
				Assert::AreEqual(false, rlimit.CanAdd(101));
				Assert::AreEqual(true, rlimit.CanAdd(100));
				Assert::AreEqual(true, rlimit.CanAdd(-3000));
				Assert::AreEqual(false, rlimit.CanAdd(-3001));
				Assert::AreEqual(false, rlimit.CanSubtract(std::numeric_limits<Int16>::max()));
				Assert::AreEqual(true, rlimit.CanSubtract(-100));
				Assert::AreEqual(true, rlimit.CanSubtract(3000));
				Assert::AreEqual(false, rlimit.CanSubtract(3001));

				Assert::AreEqual(true, rlimit.CanAdd(10));
				rlimit.Add(10);
				Assert::AreEqual(true, rlimit.GetCurrent() == 10);
				Assert::AreEqual(false, rlimit.CanSubtract(3011));
				Assert::AreEqual(true, rlimit.CanSubtract(3010));
				rlimit.Subtract(3010);
				Assert::AreEqual(true, rlimit.GetCurrent() == rlimit.GetMinimum());
				Assert::AreEqual(false, rlimit.CanSubtract(std::numeric_limits<UInt64>::max()));
				Assert::AreEqual(false, rlimit.CanSubtract(-1 * std::numeric_limits<UInt64>::max()));
				Assert::AreEqual(false, rlimit.CanAdd(std::numeric_limits<UInt64>::max()));
				Assert::AreEqual(false, rlimit.CanAdd(-1 * std::numeric_limits<UInt64>::max()));
			}
		}

		TEST_METHOD(Exceptions)
		{
			static_assert(noexcept(RateLimit<UInt8>{}), "Default constructor should be noexcept.");
			static_assert(noexcept(RateLimit<UInt8, 0, 10, false>{}), "Default constructor should be noexcept.");
			static_assert(noexcept(RateLimit<UInt8, 0, 10, true>{ 10 }), "Value constructor should be noexcept.");
			static_assert(!noexcept(RateLimit<UInt8, 0, 10, false>{ 10 }), "Value constructor should not be noexcept.");

			{
				RateLimit<UInt8> rlimit;
				static_assert(noexcept(rlimit.Add(1)), "Should be noexcept.");
				static_assert(noexcept(rlimit.Subtract(1)), "Should be noexcept.");
			}

			{
				RateLimit<UInt8, 0, 10, false> rlimit;
				static_assert(!noexcept(rlimit.Add(1)), "Should not be noexcept.");
				static_assert(!noexcept(rlimit.Subtract(1)), "Should not be noexcept.");

				Assert::ExpectException<std::invalid_argument>([&]()
				{
					rlimit.Add(40);
				});

				Assert::ExpectException<std::invalid_argument>([&]()
				{
					rlimit.Subtract(40);
				});

				Assert::ExpectException<std::invalid_argument>([&]()
				{
					RateLimit<UInt8, 0, 10, false> rlimit(2000000000);
				});

				Assert::ExpectException<std::invalid_argument>([&]()
				{
					RateLimit<UInt8, 0, 10, false> rlimit(-2000000000);
				});

				Assert::ExpectException<std::invalid_argument>([&]()
				{
					RateLimit<UInt8, 0, 10, false> rlimit(-1);
				});
			}
		}
	};
}