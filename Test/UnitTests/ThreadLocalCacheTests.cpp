// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Concurrency\ThreadLocalCache.h"

#include <thread>
#include <condition_variable>

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

struct TLTest
{
	TLTest() noexcept = default;

	TLTest(int val)
	{
		Value = val;
	}

	int Value{ 0 };
};

namespace UnitTests
{
	TEST_CLASS(ThreadLocalCacheTests)
	{
	public:
		TEST_METHOD(General)
		{
			Concurrency::ThreadLocalCache<TLTest, Concurrency::SpinMutex, 1> tlc1(33);
			Concurrency::ThreadLocalCache<TLTest, Concurrency::SpinMutex, 2> tlc2;

			// Cache should have initial value
			Assert::AreEqual(33, tlc1->Value);

			tlc1.UpdateValue([](TLTest& tlt)
			{
				tlt.Value = 369;
			});

			// Cache should not yet be updated
			Assert::AreEqual(33, tlc1.GetCache(false).Value);

			// Cache should be updated
			Assert::AreEqual(369, tlc1.GetCache(true).Value);

			tlc1.UpdateValue([](TLTest& tlt)
			{
				tlt.Value = 369369;
			});

			// Cache should not yet be updated
			Assert::AreEqual(369, tlc1.GetCache(false).Value);

			// Cache should be updated
			Assert::AreEqual(369369, tlc1->Value);

			// Should have initial value
			Assert::AreEqual(0, tlc2->Value);
		}

		TEST_METHOD(Threads)
		{
			Concurrency::ThreadLocalCache<TLTest, Concurrency::SpinMutex, 1> tlc1(33);
			Concurrency::ThreadLocalCache<TLTest, Concurrency::SpinMutex, 2> tlc2(11);

			std::mutex mtx, mtx2;
			std::condition_variable cv1, cv2;
			std::atomic<int> stage1{ 0 }, stage2{ 0 };

			auto thread1 = std::thread([&]()
			{
				auto lock = std::unique_lock<std::mutex>(mtx);

				// Cache should not yet be updated for this thread
				Assert::AreEqual(0, tlc1.GetCache(false).Value);

				// Cache should be updated for this thread
				Assert::AreEqual(33, tlc1->Value);

				stage1 = 1;
				cv1.wait(lock);
				
				// Cache should not yet be updated for this thread
				Assert::AreEqual(33, tlc1.GetCache(false).Value);

				// Cache should be updated for this thread
				Assert::AreEqual(369, tlc1->Value);

				stage1 = 2;
				cv1.wait(lock);

				// Cache should not yet be updated for this thread
				Assert::AreEqual(369, tlc1.GetCache(false).Value);

				// Cache should be updated for this thread
				Assert::AreEqual(369369, tlc1->Value);
			});

			auto thread2 = std::thread([&]()
			{
				auto lock = std::unique_lock<std::mutex>(mtx2);

				// Cache should not yet be updated for this thread
				Assert::AreEqual(0, tlc1.GetCache(false).Value);

				// Cache should be updated for this thread
				Assert::AreEqual(33, tlc1->Value);

				stage2 = 1;
				cv2.wait(lock);

				// Cache should not yet be updated for this thread
				Assert::AreEqual(33, tlc1.GetCache(false).Value);

				// Cache should be updated for this thread
				Assert::AreEqual(369, tlc1->Value);

				stage2 = 2;
				cv2.wait(lock);

				// Cache should not yet be updated for this thread
				Assert::AreEqual(369, tlc1.GetCache(false).Value);

				// Cache should be updated for this thread
				Assert::AreEqual(369369, tlc1->Value);

				// Cache should not yet be updated for this thread
				Assert::AreEqual(0, tlc2.GetCache(false).Value);

				// Cache should be updated for this thread
				Assert::AreEqual(22, tlc2->Value);
			});

			{
				auto locka = std::unique_lock<std::mutex>(mtx);
				while (stage1 != 1)
				{
					cv1.wait_for(locka, 100ms, [&]() { return (stage1 == 1); });
				}

				std::unique_lock<std::mutex> lock2a(mtx2);
				while (stage2 != 1)
				{
					cv2.wait_for(lock2a, 100ms, [&]() { return (stage2 == 1); });
				}
			}

			tlc1.UpdateValue([](TLTest& tlt)
			{
				tlt.Value = 369;
			});

			// Cache should be updated for main thread
			Assert::AreEqual(369, tlc1->Value);

			// Should have initial value
			Assert::AreEqual(11, tlc2->Value);

			cv1.notify_one();
			cv2.notify_one();
		
			{
				auto lock = std::unique_lock<std::mutex>(mtx);
				while (stage1 != 2)
				{
					cv1.wait_for(lock, 100ms, [&]() { return (stage1 == 2); });
				}

				auto lock2 = std::unique_lock<std::mutex>(mtx2);
				while (stage2 != 2)
				{
					cv2.wait_for(lock2, 100ms, [&]() { return (stage2 == 2); });
				}
			}

			{
				tlc1.UpdateValue([](TLTest& tlt)
				{
					tlt.Value = 369369;
				});

				tlc2.UpdateValue([](TLTest& tlt)
				{
					tlt.Value = 22;
				});
			}

			cv1.notify_one();
			cv2.notify_one();

			if (thread1.joinable()) thread1.join();
			if (thread2.joinable()) thread2.join();

			// Cache should not yet be updated for main thread
			Assert::AreEqual(11, tlc2.GetCache(false).Value);

			// Cache should be updated for main thread
			Assert::AreEqual(22, tlc2->Value);
		}
	};
}