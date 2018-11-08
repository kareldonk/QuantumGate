// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Common\Util.h"
#include "Common\Dispatcher.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

int DispatcherTestFunction(int n) noexcept
{
	if (n > 1) return n * DispatcherTestFunction(n - 1);
	else return 1;
}

class DispatcherTestClass
{
public:
	int MemberTestFunction(int n)
	{
		m_TestVar = DispatcherTestFunction(n);
		return m_TestVar;
	}

	static int StaticMemberTestFunction(int n)
	{
		m_TestVarStatic = DispatcherTestFunction(n);
		return m_TestVarStatic;
	}

	int m_TestVar{ 0 };
	inline static int m_TestVarStatic{ 0 };
};

namespace UnitTests
{
	TEST_CLASS(DispatcherTests)
	{
	public:
		TEST_METHOD(General)
		{
			Dispatcher<int(int)> disp;

			// Beginning empty
			Assert::AreEqual(false, disp.operator bool());

			// Add empty callbacks
			auto cbn1 = Callback<int(int)>();
			auto cbn2 = Callback<int(int)>(nullptr);

			auto hn1 = disp.Add(std::move(cbn1));
			auto hn2 = disp.Add(std::move(cbn2));

			// Empty callbacks don't get added
			Assert::AreEqual(false, hn1.operator bool());
			Assert::AreEqual(false, hn2.operator bool());

			// Should still be empty
			Assert::AreEqual(false, disp.operator bool());

			// Nothing should happen
			disp.Remove(hn1);

			auto cb1 = Callback<int(int)>(&DispatcherTestClass::StaticMemberTestFunction);
			auto h1 = disp.Add(std::move(cb1));

			// Has callbacks now
			Assert::AreEqual(true, disp.operator bool());

			DispatcherTestClass t;
			auto cb2 = Callback<int(int)>(&t, &DispatcherTestClass::MemberTestFunction);
			auto h2 = disp.Add(std::move(cb2));

			Assert::AreEqual(true, h1.operator bool());
			Assert::AreEqual(true, h2.operator bool());

			disp(10);

			// Both callbacks should have been executed
			Assert::AreEqual(3628800, t.m_TestVar);
			Assert::AreEqual(3628800, t.m_TestVarStatic);

			t.m_TestVar = 0;
			t.m_TestVarStatic = 0;

			disp.Remove(h1);

			disp(10);

			// One callback should have been executed
			Assert::AreEqual(3628800, t.m_TestVar);
			Assert::AreEqual(0, t.m_TestVarStatic);

			disp.Remove(h2);

			// Should be empty
			Assert::AreEqual(false, disp.operator bool());

			auto h3 = disp.Add(Callback<int(int)>(&DispatcherTestClass::StaticMemberTestFunction));
			
			Assert::AreEqual(true, h3.operator bool());
			
			disp(10);
			
			Assert::AreEqual(3628800, t.m_TestVarStatic);

			disp.Clear();

			// Should be empty
			Assert::AreEqual(false, disp.operator bool());
		}
	};
}