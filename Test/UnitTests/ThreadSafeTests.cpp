// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"
#include "Concurrency\ThreadSafe.h"
#include "Concurrency\DummyMutex.h"

#include <thread>
#include <atomic>
#include <condition_variable>

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Concurrency;

struct TestType
{
	constexpr TestType() noexcept {}
	constexpr TestType(int v) : Value(v) {}
	
	constexpr TestType(const TestType& other) noexcept : Value(other.Value) {}
	constexpr TestType(TestType&& other) noexcept : Value(other.Value) {}

	void operator()(int v) noexcept { Value = v; }
	int& operator[](int idx) noexcept { return Value; }

	int Value{ 0 };
};

struct TestTypeMA
{
	constexpr TestTypeMA() noexcept {}
	constexpr TestTypeMA(int v1, double v2) : Value1(v1), Value2(v2) {}

	constexpr TestTypeMA(const TestTypeMA& other) : Value1(other.Value1), Value2(other.Value2) {}
	constexpr TestTypeMA(TestTypeMA&& other) : Value1(other.Value1), Value2(other.Value2) {}

	void operator()(int v) { Value1 = v; }
	int& operator[](int idx) { return Value1; }

	int Value1{ 0 };
	double Value2{ 0 };
};

auto ExceptLambda = [](auto& val) {};
auto NoexceptLambda = [](auto& val) noexcept {};

namespace UnitTests
{
	TEST_CLASS(ThreadSafeTests)
	{
	public:
		TEST_METHOD(Constructors)
		{
			// Default
			{
				ThreadSafe<TestType> test;
				static_assert(std::is_nothrow_constructible_v<ThreadSafe<TestType>>, "Should be no throw constructible");

				Assert::AreEqual(true, test.WithUniqueLock()->Value == 0);
			}

			// Parameters
			{
				ThreadSafe<TestType> test(9);
				static_assert(!std::is_nothrow_constructible_v<ThreadSafe<TestType>, int>, "Should not be no throw constructible");

				Assert::AreEqual(true, test.WithUniqueLock()->Value == 9);
			}

			{
				ThreadSafe<TestTypeMA, DummyMutex> test(15, 20.5);
				Assert::AreEqual(true, test.WithUniqueLock()->Value1 == 15);
				Assert::AreEqual(true, test.WithUniqueLock()->Value2 == 20.5);
			}

			// Copy
			{
				TestType tt;
				tt.Value = 999;
				ThreadSafe<TestType> test(tt);
				static_assert(std::is_nothrow_constructible_v<ThreadSafe<TestType>, TestType>, "Should be no throw constructible");
				Assert::AreEqual(true, test.WithUniqueLock()->Value == 999);

				TestTypeMA ttma;
				ttma.Value1 = 1999;
				ttma.Value2 = 2999;
				ThreadSafe<TestTypeMA> testma(ttma);
				static_assert(!std::is_nothrow_constructible_v<ThreadSafe<TestTypeMA>, TestTypeMA>, "Should not be no throw constructible");
				Assert::AreEqual(true, testma.WithUniqueLock()->Value1 == 1999);
				Assert::AreEqual(true, testma.WithUniqueLock()->Value2 == 2999);
			}

			// Move
			{
				TestType tt;
				tt.Value = 333;
				ThreadSafe<TestType> test(std::move(tt));
				static_assert(std::is_nothrow_constructible_v<ThreadSafe<TestType>, TestType&&>, "Should be no throw constructible");
				Assert::AreEqual(true, test.WithUniqueLock()->Value == 333);

				TestTypeMA ttma;
				ttma.Value1 = 1333;
				ttma.Value2 = 2333;
				ThreadSafe<TestTypeMA> testma(std::move(ttma));
				static_assert(!std::is_nothrow_constructible_v<ThreadSafe<TestTypeMA>, TestTypeMA&&>, "Should not be no throw constructible");
				Assert::AreEqual(true, testma.WithUniqueLock()->Value1 == 1333);
				Assert::AreEqual(true, testma.WithUniqueLock()->Value2 == 2333);
			}
		}

		TEST_METHOD(UniqueLock)
		{
			ThreadSafe<TestType> test(111);
			std::condition_variable_any th1_cv1, th1_cv2;
			std::condition_variable_any th2_cv1, th2_cv2;
			std::atomic<int> flag{ 0 };

			auto thread1 = std::thread([&]()
			{
				DummyMutex mtx;
				std::unique_lock<DummyMutex> umtx(mtx);

				auto tulock = test.WithUniqueLock();
				
				flag = 1;
				th2_cv1.notify_one();
				th1_cv1.wait(umtx);

				tulock.Unlock();

				Assert::AreEqual(true, test.WithUniqueLock()->Value == 111);

				th2_cv2.notify_one();
				th1_cv2.wait(umtx);

				Assert::AreEqual(true, test.WithUniqueLock()->Value == 666);

				// Stage 2 Begin
				tulock.Lock();
				tulock->Value = 999;

				th2_cv2.notify_one();

				std::this_thread::sleep_for(2s);

				tulock.Unlock();
			});

			auto thread2 = std::thread([&]()
			{
				DummyMutex mtx;
				std::unique_lock<DummyMutex> umtx(mtx);
				th2_cv1.wait(umtx, [&]() { return (flag.load() == 1); });
				
				// This should not succeed
				test.IfUniqueLock([](auto& value)
				{
					value.Value = 369;
				});

				th1_cv1.notify_one();
				th2_cv2.wait(umtx);

				// This should succeed
				test.IfUniqueLock([](auto& value)
				{
					value.Value = 666;
				});

				th1_cv2.notify_one();

				// Stage 2 Begin
				th2_cv2.wait(umtx);

				auto curtime = std::chrono::steady_clock::now();

				test.WithUniqueLock([](auto& value)
				{
					Assert::AreEqual(true, value.Value == 999);
					value.Value = 339;
				});

				Assert::AreEqual(true, std::chrono::steady_clock::now() - curtime >= 2s);
				Assert::AreEqual(true, test.WithUniqueLock()->Value == 339);
			});

			thread1.join();
			thread2.join();
		}

		TEST_METHOD(SharedLock)
		{
			ThreadSafe<TestType, std::shared_mutex> test(111);
			std::condition_variable_any th1_cv1;
			std::condition_variable_any th2_cv1, th2_cv2;
			std::atomic<int> flag{ 0 };

			auto thread1 = std::thread([&]()
			{
				DummyMutex mtx;
				std::unique_lock<DummyMutex> umtx(mtx);

				auto tulock = test.WithSharedLock();

				flag = 1;
				th2_cv1.notify_one();
				th1_cv1.wait(umtx);

				tulock.UnlockShared();

				auto tulock2 = test.WithUniqueLock();

				th2_cv2.notify_one();

				tulock2->Value = 669;

				std::this_thread::sleep_for(2s);
				
				tulock2.Reset();
			});

			auto thread2 = std::thread([&]()
			{
				DummyMutex mtx;
				std::unique_lock<DummyMutex> umtx(mtx);
				th2_cv1.wait(umtx, [&]() { return (flag.load() == 1); });

				auto sflag = false;

				// This should succeed
				test.IfSharedLock([&](auto& value)
				{
					sflag = true;
					Assert::AreEqual(true, value.Value == 111);
				});

				Assert::AreEqual(true, sflag);

				th1_cv1.notify_one();
				th2_cv2.wait(umtx);

				auto curtime = std::chrono::steady_clock::now();

				// Will block for 2s
				test.WithSharedLock([](auto& value)
				{
					Assert::AreEqual(true, value.Value == 669);
				});

				Assert::AreEqual(true, std::chrono::steady_clock::now() - curtime >= 2s);
			});

			thread1.join();
			thread2.join();
		}

		TEST_METHOD(UniqueMethodsExceptionCheck)
		{
			{
				ThreadSafe<TestType, DummyMutex> test(9);
				Assert::AreEqual(true, test.WithUniqueLock()->Value == 9);

				static_assert(!noexcept(test.WithUniqueLock()), "Should not be no throw invocable");
				static_assert(!noexcept(test.WithUniqueLock(NoexceptLambda)), "Should not be no throw invocable");
				static_assert(!noexcept(test.WithUniqueLock(ExceptLambda)), "Should not be no throw invocable");

				static_assert(noexcept(test.IfUniqueLock(NoexceptLambda)), "Should be no throw invocable");
				static_assert(!noexcept(test.IfUniqueLock(ExceptLambda)), "Should not be no throw invocable");

				ThreadSafe<TestType, DummyMutex>::UniqueLockedType val;
				static_assert(!noexcept(test.TryUniqueLock(val)), "Should not be no throw invocable");
			}

			{
				const ThreadSafe<TestType, DummyMutex> test(9);
				Assert::AreEqual(true, test.WithUniqueLock()->Value == 9);

				static_assert(!noexcept(test.WithUniqueLock()), "Should not be no throw invocable");
				static_assert(!noexcept(test.WithUniqueLock(NoexceptLambda)), "Should not be no throw invocable");
				static_assert(!noexcept(test.WithUniqueLock(ExceptLambda)), "Should not be no throw invocable");

				static_assert(noexcept(test.IfUniqueLock(NoexceptLambda)), "Should be no throw invocable");
				static_assert(!noexcept(test.IfUniqueLock(ExceptLambda)), "Should not be no throw invocable");

				ThreadSafe<TestType, DummyMutex>::UniqueLockedConstType val;
				static_assert(!noexcept(test.TryUniqueLock(val)), "Should not be no throw invocable");
			}
		}

		TEST_METHOD(SharedMethodsExceptionCheck)
		{
			{
				ThreadSafe<TestType, DummyMutex> test(9);
				Assert::AreEqual(true, test.WithSharedLock()->Value == 9);

				static_assert(!noexcept(test.WithSharedLock()), "Should not be no throw invocable");
				static_assert(!noexcept(test.WithSharedLock(NoexceptLambda)), "Should not be no throw invocable");
				static_assert(!noexcept(test.WithSharedLock(ExceptLambda)), "Should not be no throw invocable");

				static_assert(noexcept(test.IfSharedLock(NoexceptLambda)), "Should be no throw invocable");
				static_assert(!noexcept(test.IfSharedLock(ExceptLambda)), "Should not be no throw invocable");

				ThreadSafe<TestType, std::shared_mutex> test2(9);
				static_assert(noexcept(test2.IfSharedLock(NoexceptLambda)), "Should be no throw invocable");
			}

			{
				const ThreadSafe<TestType, DummyMutex> test(9);
				Assert::AreEqual(true, test.WithSharedLock()->Value == 9);

				static_assert(!noexcept(test.WithSharedLock()), "Should not be no throw invocable");
				static_assert(!noexcept(test.WithSharedLock(NoexceptLambda)), "Should not be no throw invocable");
				static_assert(!noexcept(test.WithSharedLock(ExceptLambda)), "Should not be no throw invocable");

				static_assert(noexcept(test.IfSharedLock(NoexceptLambda)), "Should be no throw invocable");
				static_assert(!noexcept(test.IfSharedLock(ExceptLambda)), "Should not be no throw invocable");

				const ThreadSafe<TestType, std::shared_mutex> test2(9);
				static_assert(noexcept(test2.IfSharedLock(NoexceptLambda)), "Should be no throw invocable");
			}
		}

		TEST_METHOD(Constexpr)
		{
			constexpr ThreadSafe<TestType, DummyMutex> test;
			
			constexpr ThreadSafe<TestType, DummyMutex> test2(5);
			Assert::AreEqual(true, test2.WithUniqueLock()->Value == 5);

			constexpr ThreadSafe<TestTypeMA, DummyMutex> test3(15, 20.5);
			Assert::AreEqual(true, test3.WithUniqueLock()->Value1 == 15);
			Assert::AreEqual(true, test3.WithUniqueLock()->Value2 == 20.5);
		}

		TEST_METHOD(Value)
		{
			ThreadSafe<TestType, std::shared_mutex> test(111);
			auto testul = test.WithUniqueLock();

			ThreadSafe<TestTypeMA, std::shared_mutex> testma(999, 333);
			auto testmaul = testma.WithUniqueLock();
			
			// operator()
			{
				static_assert(noexcept(testul(333)), "Should be no throw invocable");
				testul(333);
				Assert::AreEqual(true, testul->Value == 333);

				static_assert(!noexcept(testmaul(666)), "Should not be no throw invocable");
				testmaul(666);
				Assert::AreEqual(true, testmaul->Value1 == 666);
			}

			// operator[]
			{
				static_assert(noexcept(testul[1]), "Should be no throw invocable");
				Assert::AreEqual(true, testul[1] == 333);

				static_assert(!noexcept(testmaul[1]), "Should not be no throw invocable");
				Assert::AreEqual(true, testmaul[1] == 666);
			}
		}
	};
}