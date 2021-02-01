// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

int FreeTestFunctionNoexcept(int n) noexcept
{
	if (n > 1) return n * FreeTestFunctionNoexcept(n - 1);
	else return 1;
}

bool FreeTestFunctionExecuted{ false };

void FreeTestFunction()
{
	FreeTestFunctionExecuted = true;
	return;
}

bool MemberTestFunctionConstExecuted{ false };
bool MemberTestFunctionConstNoexceptExecuted{ false };
bool MemberTestStaticFunctionExecuted{ false };

void ResetExecuteState() noexcept
{
	FreeTestFunctionExecuted = false;
	MemberTestFunctionConstExecuted = false;
	MemberTestFunctionConstNoexceptExecuted = false;
	MemberTestStaticFunctionExecuted = false;
}

class CbTestClass
{
public:
	CbTestClass() = default;

	CbTestClass(const CbTestClass& other) noexcept :
		m_TestVar(other.m_TestVar + 3)
	{}

	CbTestClass(CbTestClass&& other) noexcept :
		m_TestVar(other.m_TestVar + 6)
	{
		other.m_TestVar = 0;
	}

	~CbTestClass() = default;

	CbTestClass& operator=(const CbTestClass&) = delete;
	CbTestClass& operator=(CbTestClass&&) noexcept = delete;

	int MemberTestFunction(int n)
	{
		return FreeTestFunctionNoexcept(n);
	}

	int MemberTestFunctionNoexcept(int n) noexcept
	{
		return FreeTestFunctionNoexcept(n);
	}

	bool MemberTestFunctionConst() const
	{
		MemberTestFunctionConstExecuted = true;
		return true;
	}

	void MemberTestFunctionConstNoexcept() const noexcept
	{
		MemberTestFunctionConstNoexceptExecuted = true;
		return;
	}

	int MemberTestFunctionRef(int& n)
	{
		return FreeTestFunctionNoexcept(n);
	}

	int MemberTestFunctionRef(int&& n)
	{
		return FreeTestFunctionNoexcept(n) + 1;
	}

	int operator()()
	{
		return 11;
	}
	
	int operator()() const
	{
		return 22;
	}

	int operator()(int x)
	{
		return x + 1;
	}

	int operator()(int x) const noexcept
	{
		return x + 2;
	}

	bool operator()(std::optional<bool> flag) const
	{
		if (flag.has_value()) return *flag;

		return false;
	}

	static bool MemberTestStaticFunction() noexcept
	{
		MemberTestStaticFunctionExecuted = true;
		return true;
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
			ResetExecuteState();

			// Empty and null tests
			auto cbn1 = Callback<int(int)>();
			auto cbn2 = Callback<int(int)>(nullptr);
			Assert::AreEqual(false, cbn1.operator bool());
			Assert::AreEqual(false, cbn2.operator bool());

			// Clear() test
			auto cb1 = Callback<int(int) noexcept>(&FreeTestFunctionNoexcept);
			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3628800, cb1(10));
			cb1.Clear();
			Assert::AreEqual(false, cb1.operator bool());
			
			// Assign different function with same signature to cleared callback
			CbTestClass t;
			cb1 = Callback<int(int) noexcept>(&t, &CbTestClass::MemberTestFunctionNoexcept);
			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3628800, cb1(10));
			cb1.Clear();
			Assert::AreEqual(false, cb1.operator bool());
		}

		TEST_METHOD(FreeFunction)
		{
			ResetExecuteState();

			auto cb1 = Callback<int(int) noexcept>(&FreeTestFunctionNoexcept);
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

			auto cb4 = Callback<void()>(&FreeTestFunction);
			Assert::AreEqual(true, cb4.operator bool());
			Assert::AreEqual(false, FreeTestFunctionExecuted);
			cb4();
			Assert::AreEqual(true, FreeTestFunctionExecuted);

			// Assignment to null object
			cb4 = Callback<void()>(nullptr);
			Assert::AreEqual(false, cb4.operator bool());
		}

		TEST_METHOD(ObjectMemberFunction)
		{
			ResetExecuteState();

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

			auto cb4 = Callback<void() const noexcept>(&t, &CbTestClass::MemberTestFunctionConstNoexcept);
			Assert::AreEqual(true, cb4.operator bool());
			Assert::AreEqual(false, MemberTestFunctionConstNoexceptExecuted);
			cb4();
			Assert::AreEqual(true, MemberTestFunctionConstNoexceptExecuted);

			// Assignment to null object
			cb4 = Callback<void() const noexcept>(nullptr);
			Assert::AreEqual(false, cb4.operator bool());

			// Operator()
			CbTestClass t2;
			Callback<int()> cb5(std::move(t2));
			Assert::AreEqual(11, cb5());
			
			Callback<int() const> cb6(CbTestClass{});
			Assert::AreEqual(22, cb6());

			Callback<int(int)> cb8(CbTestClass{});
			Assert::AreEqual(45, cb8(44));

			Callback<int(int) const noexcept> cb9(CbTestClass{});
			Assert::AreEqual(46, cb9(44));

			Callback<bool(std::optional<bool>) const> cb10(CbTestClass{});
			Assert::AreEqual(true, cb10(true));
		}

		TEST_METHOD(ConstObjectMemberFunction)
		{
			ResetExecuteState();

			const CbTestClass t;
			auto cb = Callback<void() const noexcept>(&t, &CbTestClass::MemberTestFunctionConstNoexcept);
			Assert::AreEqual(true, cb.operator bool());
			Assert::AreEqual(false, MemberTestFunctionConstNoexceptExecuted);
			cb();
			Assert::AreEqual(true, MemberTestFunctionConstNoexceptExecuted);
		}

		TEST_METHOD(MutableLambdaFunction)
		{
			Callback<int(int)> cb1([&](int n) mutable -> int
			{
				if (n > 1) return n * FreeTestFunctionNoexcept(n - 1);
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

		TEST_METHOD(ConstLambdaFunction)
		{
			Callback<int(int) const> cb1([&](int n) -> int
			{
				if (n > 1) return n * FreeTestFunctionNoexcept(n - 1);
				else return 1;
			});

			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3628800, cb1(10));

			// Move assignment test
			Callback<int(int) const> cb2;
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
			cb3 = Callback<int(int) const>(nullptr);
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

			Callback<int(int) const> cb1([&](int n) -> int
			{
				UInt64 n2 = test1 * 2;
				if (test4 > 300) n2 = n2 + test3; // will never happen
				return static_cast<int>((n * FreeTestFunctionNoexcept(n - 1)) + n2 + test2 + test3);
			});

			Assert::AreEqual(true, cb1.operator bool());
			Assert::AreEqual(3629600, cb1(10));

			// Move assignment test
			Callback<int(int) const> cb2;
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
			cb3 = Callback<int(int) const>(nullptr);
			Assert::AreEqual(false, cb3.operator bool());
		}

		TEST_METHOD(ReferenceParameters)
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

		TEST_METHOD(MoveParameters)
		{
			CbTestClass t;
			t.m_TestVar = 10;

			// Test callback which takes lvalue reference
			auto cb1 = MakeCallback([](CbTestClass& tv)
			{
				Assert::AreEqual(10, tv.m_TestVar);
			});

			cb1(t);

			// Test callback which takes rvalue reference
			auto cb2 = MakeCallback([](CbTestClass&& tv)
			{
				const auto tv2(std::move(tv));
				Assert::AreEqual(16, tv2.m_TestVar);
			});

			cb2(std::move(t));

			Assert::AreEqual(0, t.m_TestVar);

			// Test callback which takes lvalue reference
			auto cb3 = MakeCallback(&t, static_cast<int(CbTestClass::*)(int&)>(&CbTestClass::MemberTestFunctionRef));
			int a(10);
			auto resulta = cb3(a);

			Assert::AreEqual(resulta, 3628800);

			// Test callback which takes rvalue reference
			auto cb4 = MakeCallback(&t, static_cast<int(CbTestClass::*)(int&&)>(&CbTestClass::MemberTestFunctionRef));
			int b(10);
			auto resultb = cb4(std::move(b));

			Assert::AreEqual(resultb, 3628801);
		}

		TEST_METHOD(MakeCallbackFunctions)
		{
			ResetExecuteState();

			auto lambda = [](CbTestClass& tv) noexcept
			{
				tv.m_TestVar += 400;
			};

			auto cb1 = MakeCallback(std::move(lambda));

			CbTestClass tv;
			cb1(tv);
			Assert::AreEqual(400, tv.m_TestVar);

			auto cb2 = MakeCallback(&FreeTestFunctionNoexcept);
			Assert::AreEqual(true, cb2.operator bool());
			Assert::AreEqual(3628800, cb2(10));

			auto cb2b = MakeCallback(&FreeTestFunction);
			Assert::AreEqual(true, cb2b.operator bool());
			Assert::AreEqual(false, FreeTestFunctionExecuted);
			cb2b();
			Assert::AreEqual(true, FreeTestFunctionExecuted);

			auto cb2c = MakeCallback(&CbTestClass::MemberTestStaticFunction);
			Assert::AreEqual(true, cb2c.operator bool());
			Assert::AreEqual(false, MemberTestStaticFunctionExecuted);
			cb2c();
			Assert::AreEqual(true, MemberTestStaticFunctionExecuted);

			CbTestClass t;
			auto cb3 = MakeCallback(&t, &CbTestClass::MemberTestFunction);
			Assert::AreEqual(true, cb3.operator bool());
			Assert::AreEqual(3628800, cb3(10));

			auto cb4 = MakeCallback(&t, &CbTestClass::MemberTestFunctionConstNoexcept);
			Assert::AreEqual(true, cb4.operator bool());
			Assert::AreEqual(false, MemberTestFunctionConstNoexceptExecuted);
			cb4();
			Assert::AreEqual(true, MemberTestFunctionConstNoexceptExecuted);

			auto cb5 = MakeCallback(&t, &CbTestClass::MemberTestFunctionConst);
			Assert::AreEqual(true, cb4.operator bool());
			Assert::AreEqual(false, MemberTestFunctionConstExecuted);
			cb5();
			Assert::AreEqual(true, MemberTestFunctionConstExecuted);
		}
	};
}
