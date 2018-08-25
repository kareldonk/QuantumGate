// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

int FreeTestFunction(int n) noexcept
{
	if (n > 1) return n * FreeTestFunction(n - 1);
	else return 1;
}

void FreeTestFunction2()
{
	return;
}

class CbTestClass
{
public:
	int MemberTestFunction(int n)
	{
		return FreeTestFunction(n);
	}

	int MemberTestFunctionNe(int n) noexcept
	{
		return FreeTestFunction(n);
	}

	bool MemberTestFunction2() noexcept
	{
		return false;
	}

	void MemberTestFunction3() const noexcept
	{
		return;
	}

	bool MemberTestFunction4() const
	{
		return false;
	}

	static bool MemberTestStaticFunction() noexcept
	{
		return false;
	}

	int m_TestVar{ 0 };
};

namespace UnitTests
{
	TEST_CLASS(CallbackTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Empty and null tests
			auto cbn1 = Callback<int(int)>();
			auto cbn2 = Callback<int(int)>(nullptr);
			Assert::AreEqual(false, cbn1.operator bool());
			Assert::AreEqual(false, cbn2.operator bool());

			// Clear() test
			auto cb1 = Callback<int(int) noexcept>(&FreeTestFunction);
			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3628800, cb1(10));
			cb1.Clear();
			Assert::AreEqual(false, cb1.operator bool());
			
			// Assign different function with same signature to cleared callback
			CbTestClass t;
			cb1 = Callback<int(int) noexcept>(&t, &CbTestClass::MemberTestFunctionNe);
			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3628800, cb1(10));
			cb1.Clear();
			Assert::AreEqual(false, cb1.operator bool());
		}

		TEST_METHOD(FreeFunction)
		{
			auto cb1 = Callback<int(int) noexcept>(&FreeTestFunction);
			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3628800, cb1(10));

			// Move assignment test
			Callback<int(int) noexcept> cb2;
			cb2 = std::move(cb1);
			Assert::AreEqual(false, cb1.operator bool());
			Assert::AreEqual(true, cb2.operator bool());
			Assert::AreEqual(3628800, cb2(10));

			// Move construction test
			auto cb3(std::move(cb2));
			Assert::AreEqual(false, cb2.operator bool());
			Assert::AreEqual(true, cb3.operator bool());
			Assert::AreEqual(3628800, cb3(10));

			auto cb4 = Callback<void()>(&FreeTestFunction2);
			Assert::AreEqual(true, cb4.operator bool());
			cb4();

			// Assignment to null object
			cb4 = Callback<void()>(nullptr);
			Assert::AreEqual(false, cb4.operator bool());
		}

		TEST_METHOD(MemberFunction)
		{
			CbTestClass t;
			Callback<int(int)> cb1(&t, &CbTestClass::MemberTestFunction);
			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3628800, cb1(10));

			// Move assignment test
			Callback<int(int)> cb2;
			cb2 = std::move(cb1);
			Assert::AreEqual(false, cb1.operator bool());
			Assert::AreEqual(true, cb2.operator bool());
			Assert::AreEqual(3628800, cb2(10));

			// Move construction test
			auto cb3(std::move(cb2));
			Assert::AreEqual(false, cb2.operator bool());
			Assert::AreEqual(true, cb3.operator bool());
			Assert::AreEqual(3628800, cb3(10));

			auto cb4 = Callback<void() noexcept>(&t, &CbTestClass::MemberTestFunction3);
			Assert::AreEqual(true, cb4.operator bool());
			cb4();

			// Assignment to null object
			cb4 = Callback<void() noexcept>(nullptr);
			Assert::AreEqual(false, cb4.operator bool());
		}

		TEST_METHOD(LambdaFunction)
		{
			Callback<int(int)> cb1([&](int n) -> int
			{
				if (n > 1) return n * FreeTestFunction(n - 1);
				else return 1;
			});

			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3628800, cb1(10));

			// Move assignment test
			Callback<int(int)> cb2;
			cb2 = std::move(cb1);
			Assert::AreEqual(false, cb1.operator bool());
			Assert::AreEqual(true, cb2.operator bool());
			Assert::AreEqual(3628800, cb2(10));

			// Move construction test
			auto cb3(std::move(cb2));
			Assert::AreEqual(false, cb2.operator bool());
			Assert::AreEqual(true, cb3.operator bool());
			Assert::AreEqual(3628800, cb3(10));

			// Assignment to null object
			cb3 = Callback<int(int)>(nullptr);
			Assert::AreEqual(false, cb3.operator bool());
		}

		TEST_METHOD(BigLambdaFunction)
		{
			// Bring enough state into the lambda to make it
			// bigger than default Callback storage size
			UInt64 test1{ 200 };
			UInt64 test2{ 200 };
			UInt64 test3{ 200 };
			UInt64 test4{ 200 };

			Callback<int(int)> cb1([&](int n) -> int
			{
				UInt64 n2 = test1 * 2;
				if (test4 > 300) n2 = n2 + test3; // will never happen
				return static_cast<int>((n * FreeTestFunction(n - 1)) + n2 + test2 + test3);
			});

			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3629600, cb1(10));

			// Move assignment test
			Callback<int(int)> cb2;
			cb2 = std::move(cb1);
			Assert::AreEqual(false, cb1.operator bool());
			Assert::AreEqual(true, cb2.operator bool());
			Assert::AreEqual(3629600, cb2(10));

			// Move construction test
			auto cb3(std::move(cb2));
			Assert::AreEqual(false, cb2.operator bool());
			Assert::AreEqual(true, cb3.operator bool());
			Assert::AreEqual(3629600, cb3(10));

			// Assignment to null object
			cb3 = Callback<int(int)>(nullptr);
			Assert::AreEqual(false, cb3.operator bool());
		}

		TEST_METHOD(FunctionReferences)
		{
			CbTestClass t;
			t.m_TestVar = 10;

			UInt64 val{ 10 };
			UInt64 val2{ 10 };

			// Test callback which takes references
			auto cb1 = MakeCallback([](CbTestClass& tv, UInt64& v, UInt64* v2)
			{
				tv.m_TestVar += 400;
				v += 400;
				*v2 += 400;
			});

			// Variables should be changed after this call
			cb1(t, val, &val2);

			Assert::AreEqual(410, t.m_TestVar);
			Assert::AreEqual(static_cast<UInt64>(410), val);
			Assert::AreEqual(static_cast<UInt64>(410), val2);

			// Test callback which takes copies
			auto cb2 = MakeCallback([](CbTestClass tv, UInt64 v, UInt64 v2)
			{
				tv.m_TestVar += 400;
				v += 400;
				v2 += 400;
			});

			// Variables should not be changed after this call
			cb2(t, val, val2);

			Assert::AreEqual(410, t.m_TestVar);
			Assert::AreEqual(static_cast<UInt64>(410), val);
			Assert::AreEqual(static_cast<UInt64>(410), val2);
		}

		TEST_METHOD(FunctionMakeCallback)
		{
			auto lambda = [](CbTestClass& tv, UInt64& v, UInt64* v2) noexcept
			{
				tv.m_TestVar += 400;
				v += 400;
				*v2 += 400;
			};

			auto cb1 = MakeCallback(lambda);
			auto cb1a = MakeCallback(std::move(lambda));

			auto cb2 = MakeCallback(&FreeTestFunction);
			Assert::AreEqual(true, cb2.operator bool());
			Assert::AreEqual(3628800, cb2(10));

			auto cb2b = MakeCallback(&FreeTestFunction2);
			Assert::AreEqual(true, cb2b.operator bool());
			cb2b();

			auto cb2c = MakeCallback(&CbTestClass::MemberTestStaticFunction);
			Assert::AreEqual(true, cb2c.operator bool());
			cb2c();

			CbTestClass t;
			auto cb3 = MakeCallback(&t, &CbTestClass::MemberTestFunction);
			Assert::AreEqual(true, cb3.operator bool());
			Assert::AreEqual(3628800, cb3(10));

			auto cb4 = MakeCallback(&t, &CbTestClass::MemberTestFunction3);
			Assert::AreEqual(true, cb4.operator bool());
			cb4();

			auto cb5 = MakeCallback(&t, &CbTestClass::MemberTestFunction4);
			Assert::AreEqual(true, cb4.operator bool());
			cb5();
		}
	};
}
