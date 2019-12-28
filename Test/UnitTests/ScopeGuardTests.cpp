// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Common\ScopeGuard.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

int ScopeGuardTestNum{ 0 };
void ScopeGuardTestFunc()
{
	++ScopeGuardTestNum;
}

namespace UnitTests
{
	TEST_CLASS(ScopeGuardTests)
	{
	public:
		TEST_METHOD(General)
		{
			auto num = 0u;

			{
				// MakeScopeGuard function
				auto sg1 = MakeScopeGuard([&] { ++num; });
				Assert::AreEqual(true, sg1.IsActive());

				// Move constructor
				auto sg2 = std::move(sg1);

				Assert::AreEqual(false, sg1.IsActive());
				Assert::AreEqual(true, sg2.IsActive());

				// Constructor
				ScopeGuard<void(*)()> sg3(&ScopeGuardTestFunc);
				Assert::AreEqual(true, sg3.IsActive());

				// Move constructor
				ScopeGuard<void(*)()> sg4(std::move(sg3));
				Assert::AreEqual(false, sg3.IsActive());
				Assert::AreEqual(true, sg4.IsActive());
			}

			// sg2 and sg4 should have executed
			Assert::AreEqual(true, num == 1);
			Assert::AreEqual(true, ScopeGuardTestNum == 1);

			{
				// MakeScopeGuard function
				auto sg1 = MakeScopeGuard(&ScopeGuardTestFunc);
				Assert::AreEqual(true, sg1.IsActive());

				auto sg2 = MakeScopeGuard([&] { ++num; });
				Assert::AreEqual(true, sg2.IsActive());

				sg2.Deactivate();
				Assert::AreEqual(false, sg2.IsActive());
			}

			// sg1 only should have executed
			Assert::AreEqual(true, num == 1);
			Assert::AreEqual(true, ScopeGuardTestNum == 2);

			{
				// MakeScopeGuard function
				auto sg1 = MakeScopeGuard(&ScopeGuardTestFunc);
				auto sg2 = MakeScopeGuard(&ScopeGuardTestFunc);
				
				Assert::AreEqual(true, sg1.IsActive());

				sg2.Deactivate();
				Assert::AreEqual(false, sg2.IsActive());

				// Move assignment
				sg1 = std::move(sg2);
				Assert::AreEqual(false, sg1.IsActive());

				sg1.Activate();
				Assert::AreEqual(true, sg1.IsActive());
			}

			// sg1 only should have executed
			Assert::AreEqual(true, ScopeGuardTestNum == 3);
		}
	};
}