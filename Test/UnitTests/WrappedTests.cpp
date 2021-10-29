// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"

#include "Common\Wrapped.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

struct WrappedTestValue final
{
	WrappedTestValue() noexcept : WrappedTestValue(0)
	{}

	WrappedTestValue(const int v) noexcept :
		Val(v)
	{
		++ConstructCount;
	}

	WrappedTestValue(const WrappedTestValue& other) noexcept :
		Val(other.Val)
	{
		++ConstructCount;
	}
	
	WrappedTestValue(WrappedTestValue&& other) noexcept :
		Val(other.Val)
	{
		++MoveCount;
		other.Moved = true;
	}

	~WrappedTestValue()
	{
		if (!Moved) ++DestructCount;
	}

	WrappedTestValue& operator=(const WrappedTestValue&) noexcept = default;

	WrappedTestValue& operator=(WrappedTestValue&& other) noexcept
	{
		Val = other.Val;
		other.Moved = true;
		return *this;
	}

	static void ResetCounts() noexcept
	{
		ConstructCount = 0;
		MoveCount = 0;
		DestructCount = 0;
	}

	bool Moved{ false };
	int Val{ 0 };
	inline static int ConstructCount{ 0 };
	inline static int MoveCount{ 0 };
	inline static int DestructCount{ 0 };
};

struct WrappedTestValueThrow final
{
	WrappedTestValueThrow() noexcept(false) {}

	WrappedTestValueThrow(const int v) noexcept(false) {}

	WrappedTestValueThrow(const WrappedTestValueThrow& other) noexcept(false) {}

	WrappedTestValueThrow(WrappedTestValueThrow&& other) noexcept(false) {}

	~WrappedTestValueThrow() {}

	WrappedTestValueThrow& operator=(const WrappedTestValueThrow&) noexcept(false) { return *this; }

	WrappedTestValueThrow& operator=(WrappedTestValueThrow&& other) noexcept(false) { return *this; }
};

namespace UnitTests
{
	TEST_CLASS(WrappedTests)
	{
	public:
		TEST_METHOD(Constructors)
		{
			// Default constructor
			{
				Wrapped<UInt64> w;
				Assert::AreEqual(false, w.IsOwner());
				Assert::AreEqual(false, w.operator bool());
				w.Reset();
				Assert::AreEqual(false, w.IsOwner());
				Assert::AreEqual(false, w.operator bool());
			}

			// nullptr_t constructor
			{
				Wrapped<UInt64> w(nullptr);
				Assert::AreEqual(false, w.IsOwner());
				Assert::AreEqual(false, w.operator bool());
				w.Reset();
				Assert::AreEqual(false, w.IsOwner());
				Assert::AreEqual(false, w.operator bool());
			}

			// Pointer constructor
			{
				UInt64 i{ 456 };
				Wrapped<UInt64> w(&i);
				Assert::AreEqual(false, w.IsOwner());
				Assert::AreEqual(true, w.operator bool());
				Assert::AreEqual(true, 456 == *w);
				w.Reset();
				Assert::AreEqual(false, w.IsOwner());
				Assert::AreEqual(false, w.operator bool());
			}

			// Value copy constructor
			{
				Wrapped<UInt64> w(123);
				Assert::AreEqual(true, w.IsOwner());
				Assert::AreEqual(true, w.operator bool());
				Assert::AreEqual(true, 123 == *w);
				w.Reset();
				Assert::AreEqual(false, w.IsOwner());
				Assert::AreEqual(false, w.operator bool());

				WrappedTestValue::ResetCounts();
				
				{
					Wrapped<WrappedTestValue> w2;
					Assert::AreEqual(false, w2.IsOwner());
					Assert::AreEqual(false, w2.operator bool());
					Assert::AreEqual(true, WrappedTestValue::ConstructCount == 0);

					Wrapped<WrappedTestValue> w3(WrappedTestValue(456));
					Assert::AreEqual(true, WrappedTestValue::ConstructCount == 1);

					Assert::AreEqual(true, w3.IsOwner());
					Assert::AreEqual(true, w3.operator bool());
					Assert::AreEqual(true, 456 == w3->Val);
					w3.Reset();
					Assert::AreEqual(false, w3.IsOwner());
					Assert::AreEqual(false, w3.operator bool());
				}

				Assert::AreEqual(true, WrappedTestValue::DestructCount == 1);
			}

			// Value move constructor
			{
				WrappedTestValue::ResetCounts();

				{
					WrappedTestValue wtv(333);
					Assert::AreEqual(true, WrappedTestValue::ConstructCount == 1);

					Wrapped<WrappedTestValue> w(std::move(wtv));
					Assert::AreEqual(true, WrappedTestValue::ConstructCount == 1);

					Assert::AreEqual(true, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					Assert::AreEqual(true, 333 == w->Val);
					w.Reset();
					Assert::AreEqual(false, w.IsOwner());
					Assert::AreEqual(false, w.operator bool());
				}

				Assert::AreEqual(true, WrappedTestValue::DestructCount == 1);
			}


			// Copy constructor
			{
				{
					Wrapped<UInt64> w(444);
					Assert::AreEqual(true, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					Assert::AreEqual(true, 444 == *w);

					auto w2(w);
					Assert::AreEqual(true, w2.IsOwner());
					Assert::AreEqual(true, w2.operator bool());
					Assert::AreEqual(true, 444 == *w2);
					Assert::AreEqual(true, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					Assert::AreEqual(true, 444 == *w);

					w2.Reset();
					Assert::AreEqual(false, w2.IsOwner());
					Assert::AreEqual(false, w2.operator bool());
				}

				{
					UInt64 i{ 555 };
					Wrapped<UInt64> w(&i);
					Assert::AreEqual(false, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					Assert::AreEqual(true, 555 == *w);

					auto w2(w);
					Assert::AreEqual(false, w2.IsOwner());
					Assert::AreEqual(true, w2.operator bool());
					Assert::AreEqual(true, 555 == *w2);
					Assert::AreEqual(false, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					Assert::AreEqual(true, 555 == *w);

					w2.Reset();
					Assert::AreEqual(false, w2.IsOwner());
					Assert::AreEqual(false, w2.operator bool());
				}

				WrappedTestValue::ResetCounts();

				{
					Wrapped<WrappedTestValue> w(WrappedTestValue(456));
					Assert::AreEqual(true, WrappedTestValue::ConstructCount == 1);
					Assert::AreEqual(true, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					Assert::AreEqual(true, 456 == w->Val);

					auto w2(w);
					Assert::AreEqual(true, WrappedTestValue::ConstructCount == 2);
					Assert::AreEqual(true, w2.IsOwner());
					Assert::AreEqual(true, w2.operator bool());
					Assert::AreEqual(true, 456 == w2->Val);
					Assert::AreEqual(true, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					Assert::AreEqual(true, 456 == w->Val);

					w2.Reset();
					Assert::AreEqual(false, w2.IsOwner());
					Assert::AreEqual(false, w2.operator bool());
				}

				Assert::AreEqual(true, WrappedTestValue::DestructCount == 2);
			}

			// Move constructor
			{
				{
					Wrapped<UInt64> w(444);
					Assert::AreEqual(true, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					Assert::AreEqual(true, 444 == *w);

					auto w2(std::move(w));
					Assert::AreEqual(true, w2.IsOwner());
					Assert::AreEqual(true, w2.operator bool());
					Assert::AreEqual(true, 444 == *w2);
					Assert::AreEqual(true, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					w2.Reset();
					Assert::AreEqual(false, w2.IsOwner());
					Assert::AreEqual(false, w2.operator bool());
				}

				{
					UInt64 i{ 555 };
					Wrapped<UInt64> w3(&i);
					Assert::AreEqual(false, w3.IsOwner());
					Assert::AreEqual(true, w3.operator bool());
					Assert::AreEqual(true, 555 == *w3);

					auto w4(std::move(w3));
					Assert::AreEqual(false, w4.IsOwner());
					Assert::AreEqual(true, w4.operator bool());
					Assert::AreEqual(true, 555 == *w4);
					Assert::AreEqual(false, w3.IsOwner());
					Assert::AreEqual(true, w3.operator bool());
					w4.Reset();
					Assert::AreEqual(false, w4.IsOwner());
					Assert::AreEqual(false, w4.operator bool());
				}

				WrappedTestValue::ResetCounts();

				{
					Wrapped<WrappedTestValue> w;
					Assert::AreEqual(true, WrappedTestValue::ConstructCount == 0);
					w.Emplace(456);
					Assert::AreEqual(true, WrappedTestValue::ConstructCount == 1);
					Assert::AreEqual(true, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());
					Assert::AreEqual(true, 456 == w->Val);

					auto w2(std::move(w));
					Assert::AreEqual(true, WrappedTestValue::ConstructCount == 1);
					Assert::AreEqual(true, w2.IsOwner());
					Assert::AreEqual(true, w2.operator bool());
					Assert::AreEqual(true, 456 == w2->Val);
					Assert::AreEqual(true, w.IsOwner());
					Assert::AreEqual(true, w.operator bool());

					w2.Reset();
					Assert::AreEqual(false, w2.IsOwner());
					Assert::AreEqual(false, w2.operator bool());
				}

				Assert::AreEqual(true, WrappedTestValue::DestructCount == 1);
			}
		}

		TEST_METHOD(Emplace)
		{
			WrappedTestValue::ResetCounts();

			{
				Wrapped<WrappedTestValue> w;
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 0);
				w.Emplace(456);
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 1);
				Assert::AreEqual(true, w.IsOwner());
				Assert::AreEqual(true, w.operator bool());
				Assert::AreEqual(true, 456 == w->Val);
				w.Emplace(555);
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 2);
				Assert::AreEqual(true, w.IsOwner());
				Assert::AreEqual(true, w.operator bool());
				Assert::AreEqual(true, 555 == w->Val);
				w.Reset();
				Assert::AreEqual(false, w.IsOwner());
				Assert::AreEqual(false, w.operator bool());
				w.Emplace(666);
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 3);
				Assert::AreEqual(true, w.IsOwner());
				Assert::AreEqual(true, w.operator bool());
				Assert::AreEqual(true, 666 == w->Val);
			}

			Assert::AreEqual(true, WrappedTestValue::DestructCount == 3);

			{
				Wrapped<std::string> w;
				w.Emplace("Testing");
				Assert::AreEqual(true, w.IsOwner());
				Assert::AreEqual(true, w.operator bool());
				Assert::AreEqual(true, "Testing" == *w);

				std::string s{ "Second" };
				w = &s;
				Assert::AreEqual(false, w.IsOwner());
				Assert::AreEqual(true, w.operator bool());
				Assert::AreEqual(true, "Second" == *w);
				w->resize(3);
				Assert::AreEqual(true, "Sec" == *w);

				w.Emplace("Testing2");
				Assert::AreEqual(true, w.IsOwner());
				Assert::AreEqual(true, w.operator bool());
				Assert::AreEqual(true, "Testing2" == *w);

				Assert::AreEqual(true, "Sec" == s);
			}
		}

		TEST_METHOD(Assignment)
		{
			WrappedTestValue::ResetCounts();

			{
				Wrapped<WrappedTestValue> w;
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 0);

				WrappedTestValue wv(999);
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 1);
				w = wv;
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 2);
				Assert::AreEqual(true, w.IsOwner());
				Assert::AreEqual(true, w.operator bool());
				Assert::AreEqual(true, 999 == w->Val);
				w->Val = 777;
				Assert::AreEqual(true, 777 == w->Val);
				Assert::AreEqual(true, 999 == wv.Val);
				
				Wrapped<WrappedTestValue> w3;
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 2);
				w3 = &wv;
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 2);
				Assert::AreEqual(false, w3.IsOwner());
				Assert::AreEqual(true, w3.operator bool());
				Assert::AreEqual(true, 999 == w3->Val);
				w3->Val = 444;
				Assert::AreEqual(true, 444 == w3->Val);
				Assert::AreEqual(true, 444 == wv.Val);

				Wrapped<WrappedTestValue> w2;
				Assert::AreEqual(true, WrappedTestValue::ConstructCount == 2);
				w2 = std::move(wv);
				Assert::AreEqual(true, WrappedTestValue::MoveCount == 1);
				Assert::AreEqual(true, w2.IsOwner());
				Assert::AreEqual(true, w2.operator bool());
				Assert::AreEqual(true, 444 == w2->Val);
			}

			Assert::AreEqual(true, WrappedTestValue::DestructCount == 2);
		}

		TEST_METHOD(CompileTime)
		{
			{
				static_assert(std::is_nothrow_constructible_v<Wrapped<int>>, "Should be nothrow constructible.");
				static_assert(std::is_nothrow_copy_constructible_v<Wrapped<int>>, "Should be nothrow copy constructible.");
				static_assert(std::is_nothrow_move_constructible_v<Wrapped<int>>, "Should be nothrow move constructible.");
				static_assert(std::is_nothrow_copy_assignable_v<Wrapped<int>>, "Should be nothrow copy assignable.");
				static_assert(std::is_nothrow_move_assignable_v<Wrapped<int>>, "Should be nothrow move assignable.");
				static_assert(std::is_trivially_destructible_v<Wrapped<int>>, "Should be trivially destructible.");
				int i{ 0 };
				static_assert(noexcept(std::declval<Wrapped<int>>().operator=(i)), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<int>>().operator=(std::move(i))), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<int>>().operator=(&i)), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<int>>().IsOwner()), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<int>>().Reset()), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<int>>().Emplace(2)), "Should be noexcept.");

				struct test
				{
					int Val{ 0 };
				};

				static_assert(std::is_trivially_destructible_v<Wrapped<test>>, "Should be trivially destructible.");
			}

			{
				static_assert(std::is_nothrow_constructible_v<Wrapped<WrappedTestValue>>, "Should be nothrow constructible.");
				static_assert(std::is_nothrow_copy_constructible_v<Wrapped<WrappedTestValue>>, "Should be nothrow copy constructible.");
				static_assert(std::is_nothrow_move_constructible_v<Wrapped<WrappedTestValue>>, "Should be nothrow move constructible.");
				static_assert(std::is_nothrow_copy_assignable_v<Wrapped<WrappedTestValue>>, "Should be nothrow copy assignable.");
				static_assert(std::is_nothrow_move_assignable_v<Wrapped<WrappedTestValue>>, "Should be nothrow move assignable.");
				static_assert(!std::is_trivially_destructible_v<Wrapped<WrappedTestValue>>, "Should not be trivially destructible.");
				WrappedTestValue w{ 0 };
				static_assert(noexcept(std::declval<Wrapped<WrappedTestValue>>().operator=(w)), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<WrappedTestValue>>().operator=(std::move(w))), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<WrappedTestValue>>().operator=(&w)), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<WrappedTestValue>>().IsOwner()), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<WrappedTestValue>>().Reset()), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<WrappedTestValue>>().Emplace(2)), "Should be noexcept.");
			}

			{
				static_assert(std::is_nothrow_constructible_v<Wrapped<WrappedTestValueThrow>>, "Should be nothrow constructible.");
				static_assert(!std::is_nothrow_copy_constructible_v<Wrapped<WrappedTestValueThrow>>, "Should not be nothrow copy constructible.");
				static_assert(!std::is_nothrow_move_constructible_v<Wrapped<WrappedTestValueThrow>>, "Should not be nothrow move constructible.");
				static_assert(!std::is_nothrow_copy_assignable_v<Wrapped<WrappedTestValueThrow>>, "Should not be nothrow copy assignable.");
				static_assert(!std::is_nothrow_move_assignable_v<Wrapped<WrappedTestValueThrow>>, "Should not be nothrow move assignable.");
				static_assert(!std::is_trivially_destructible_v<Wrapped<WrappedTestValueThrow>>, "Should not be trivially destructible.");
				WrappedTestValueThrow w{ 0 };
				static_assert(!noexcept(std::declval<Wrapped<WrappedTestValueThrow>>().operator=(w)), "Should not be noexcept.");
				static_assert(!noexcept(std::declval<Wrapped<WrappedTestValueThrow>>().operator=(std::move(w))), "Should not be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<WrappedTestValueThrow>>().operator=(&w)), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<WrappedTestValueThrow>>().IsOwner()), "Should be noexcept.");
				static_assert(noexcept(std::declval<Wrapped<WrappedTestValueThrow>>().Reset()), "Should be noexcept.");
				static_assert(!noexcept(std::declval<Wrapped<WrappedTestValueThrow>>().Emplace(2)), "Should not be noexcept.");
			}
		}
	};
}