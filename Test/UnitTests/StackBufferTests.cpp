// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "..\..\QuantumGateLib\Memory\StackBuffer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Memory;

namespace UnitTests
{
	TEST_CLASS(StackStackBufferTests)
	{
	public:
		TEST_METHOD(General)
		{
			String txt = L"There is no such a thing in America as an independent press, [...]. "
				L"You are all slaves. You know it, and I know it. There is not one of you who dares "
				L"to express an honest opinion. If you expressed it, you would know beforehand that it "
				L"would never appear in print. [...] If I should allow honest opinions to be printed in "
				L"one issue of my paper, I would be like Othello before twenty-four hours: my occupation "
				L"would be gone. [...] The business of a New York journalist is to distort the truth, to "
				L"lie outright, to pervert, to vilify, to fawn at the feet of Mammon [...]. You know this, "
				L"and I know it; and what foolery to be toasting an 'Independent Press'! We are the tools "
				L"and vassals of rich men behind the scenes. [...] They pull the string and we dance. Our "
				L"time, our talents, our lives, our possibilities, are all the property of other men. We "
				L"are intellectual prostitutes. - John Swinton, journalist for The New York Times, 1880";

			// Default constructor
			StackBuffer2048 b1;
			Assert::AreEqual(true, b1.IsEmpty());
			Assert::AreEqual(false, b1.operator bool());
			Assert::AreEqual(true, b1.GetSize() == 0);
			Assert::AreEqual(true, b1.GetMaxSize() == 2048);

			// Allocation
			b1.Allocate(10);
			Assert::AreEqual(false, b1.IsEmpty());
			Assert::AreEqual(true, b1.operator bool());
			Assert::AreEqual(true, b1.GetSize() == 10);

			StackBuffer128 b1b;
			Assert::ExpectException<StackBufferOverflowException>([&]() { b1b.Allocate(129); });
			Assert::AreEqual(true, b1b.GetMaxSize() == 128);

			// Copy constructor for Byte*
			StackBuffer2048 b2(reinterpret_cast<Byte*>(txt.data()), txt.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b2.GetSize() == txt.size() * sizeof(String::value_type));
			Assert::AreEqual(true, memcmp(b2.GetBytes(), txt.data(), b2.GetSize()) == 0);
			Assert::AreEqual(true, b1 != b2);
			Assert::AreEqual(true, b2.operator bool());

			// Alloc constructor
			StackBuffer2048 b3(txt.size() * sizeof(String::value_type));
			memcpy(b3.GetBytes(), reinterpret_cast<Byte*>(txt.data()), txt.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b2 == b3);
			Assert::ExpectException<StackBufferOverflowException>([]() { StackBuffer128 b3b(129); });

			// Copy constructor for StackBuffer
			StackBuffer2048 b4(b3);
			Assert::AreEqual(true, b4 == b3);
			Assert::AreEqual(true, b4.GetSize() == b3.GetSize());

			// Move constructor
			StackBuffer2048 b5(std::move(b4));
			Assert::AreEqual(true, b5 == b3);
			Assert::AreEqual(true, b5.GetSize() == b3.GetSize());
			Assert::AreEqual(true, b4.IsEmpty());
			Assert::AreEqual(true, b4.GetSize() == 0);

			// Copy assignment
			b1 = b2;
			Assert::AreEqual(true, b1 == b2);
			Assert::AreEqual(true, b1.GetSize() == b2.GetSize());
			Assert::ExpectException<StackBufferOverflowException>([&]() { b1b = b2; });

			// Move assignment for StackBuffer
			b4 = std::move(b5);
			Assert::AreEqual(true, b4 == b3);
			Assert::AreEqual(true, b4.GetSize() == b3.GetSize());
			Assert::AreEqual(true, b5.IsEmpty());
			Assert::AreEqual(true, b5.GetSize() == 0);

			// Clear
			b4.Clear();
			Assert::AreEqual(true, b4.IsEmpty());
			Assert::AreEqual(true, b4.GetSize() == 0);

			// StackBuffer swap
			b4.Swap(b2);
			Assert::AreEqual(true, b4 == b3);
			Assert::AreEqual(true, b2.IsEmpty());
			Assert::AreEqual(true, b2.GetSize() == 0);

			// Resize
			b2.Resize(128);
			Assert::AreEqual(false, b2.IsEmpty());
			Assert::AreEqual(true, b2.GetSize() == 128);
			std::memcpy(b2.GetBytes(), b4.GetBytes(), b2.GetSize());
			Assert::AreEqual(true, b2 == BufferView(b4).GetFirst(128));

			// Adding
			b2 += BufferView(b4).GetSub(128, 32);
			Assert::AreEqual(true, b2.GetSize() == 160);
			Assert::AreEqual(true, b2 == BufferView(b4).GetFirst(160));
			b2 = b4;
			Assert::ExpectException<StackBufferOverflowException>([&]() { b2 += b4; });

			// Equality
			Assert::AreEqual(true, b2 == b4);
			b4.RemoveLast(1);
			Assert::AreEqual(true, b2 != b4);
			b2 = b4;
			Assert::AreEqual(true, b2 == b4);
			b4[0] = Byte{ 80 };
			Assert::AreEqual(true, b2 != b4);
		}

		TEST_METHOD(StackBufferAndBufferView)
		{
			std::string txt = "Be a loner. That gives you time to wonder, to search for the truth. "
				"Have holy curiosity. Make your life worth living. - Albert Einstein";

			StackBuffer256 b1(reinterpret_cast<Byte*>(txt.data()), txt.size());

			BufferView bview(b1);

			// Copy construction/assignment
			StackBuffer256 b2 = bview;

			Assert::AreEqual(true, b1 == b2);
			Assert::AreEqual(true, b1[6] == b2[6]);
			Assert::AreEqual(true, b1[6] == bview[6]);

			// Remove left and right
			b2.RemoveFirst(12);
			bview.RemoveFirst(12);
			Assert::AreEqual(true, bview == b2);

			b2.RemoveLast(18);
			bview.RemoveLast(18);
			Assert::AreEqual(true, bview == b2);

			Assert::AreEqual(true, b1 != b2);

			// Resize
			b2.Resize(4);
			Assert::AreEqual(true,
							 b2[0] == Byte{ 'T' } &&
							 b2[1] == Byte{ 'h' } &&
							 b2[2] == Byte{ 'a' } &&
							 b2[3] == Byte{ 't' });

			// Empty BufferView
			BufferView bview2;
			StackBuffer256 b3(bview2);
			Assert::AreEqual(true, b3.IsEmpty());
			Assert::AreEqual(true, b3.GetSize() == 0);

			b3 += bview2;
			Assert::AreEqual(true, b3.IsEmpty());
			Assert::AreEqual(true, b3.GetSize() == 0);

			StackBuffer256 b4(b3);
			Assert::AreEqual(true, b4.IsEmpty());
			Assert::AreEqual(true, b4.GetSize() == 0);

			BufferView bview3(b2);
			// Copy assignment
			b4 = bview3;

			Assert::AreEqual(true, b2 == b4);
		}

		TEST_METHOD(StackBufferConstexpr)
		{
			constexpr StackBuffer32 b1;
			constexpr StackBuffer32 b2;
			static_assert(b1 == b2, "Should be equal.");
			static_assert(b1.IsEmpty(), "Should be empty.");
			static_assert(b1.GetSize() == 0, "Should be 0.");
			static_assert(b1.GetMaxSize() == 32, "Should be 32.");

			constexpr StackBuffer32 b3{ 10 };
			constexpr StackBuffer32 b4{ 20 };
			static_assert(b3 != b4, "Should not be equal.");
			static_assert(!b3.IsEmpty(), "Should be empty.");
			static_assert(b3.GetSize() == 10, "Should be 10.");

			constexpr std::array<Byte, 5> txt{ Byte{ 'a' }, Byte{ 'b' }, Byte{ 'c' }, Byte{ 'd' }, Byte{ 'e' } };
			constexpr BufferView txtb{ txt.data(), txt.size() };
			constexpr StackBuffer32 b5{ txtb };
			static_assert(!b5.IsEmpty(), "Should not be empty.");
			static_assert(b5.GetSize() == 5, "Should be 5.");
			static_assert(b5[0] == Byte{ 'a' }, "Should be equal.");
			static_assert(b5[1] == Byte{ 'b' }, "Should be equal.");
			static_assert(b5[2] == Byte{ 'c' }, "Should be equal.");
			static_assert(b5[3] == Byte{ 'd' }, "Should be equal.");
			static_assert(b5[4] == Byte{ 'e' }, "Should be equal.");

			constexpr StackBuffer32 b6 = std::move(b5);
			static_assert(!b6.IsEmpty(), "Should not be empty.");
			static_assert(b6.GetSize() == 5, "Should be 5.");
			static_assert(b6[0] == Byte{ 'a' }, "Should be equal.");
			static_assert(b6[1] == Byte{ 'b' }, "Should be equal.");
			static_assert(b6[2] == Byte{ 'c' }, "Should be equal.");
			static_assert(b6[3] == Byte{ 'd' }, "Should be equal.");
			static_assert(b6[4] == Byte{ 'e' }, "Should be equal.");

			constexpr std::array<Byte, 3> txt2{ Byte{ 'f' }, Byte{ 'g' }, Byte{ 'h' } };
			constexpr BufferView txt2b{ txt2.data(), txt2.size() };
			constexpr StackBuffer32 b7 = txt2b;
			static_assert(!b7.IsEmpty(), "Should not be empty.");
			static_assert(b7.GetSize() == 3, "Should be 3.");
			static_assert(b7[0] == Byte{ 'f' }, "Should be equal.");
			static_assert(b7[1] == Byte{ 'g' }, "Should be equal.");
			static_assert(b7[2] == Byte{ 'h' }, "Should be equal.");
			static_assert(b6 != b7, "Should not be equal.");

			constexpr StackBuffer32 b8 = b6 + b7;
			static_assert(!b8.IsEmpty(), "Should not be empty.");
			static_assert(b8.GetSize() == 8, "Should be 8.");
			static_assert(b8[0] == Byte{ 'a' }, "Should be equal.");
			static_assert(b8[1] == Byte{ 'b' }, "Should be equal.");
			static_assert(b8[2] == Byte{ 'c' }, "Should be equal.");
			static_assert(b8[3] == Byte{ 'd' }, "Should be equal.");
			static_assert(b8[4] == Byte{ 'e' }, "Should be equal.");
			static_assert(b8[5] == Byte{ 'f' }, "Should be equal.");
			static_assert(b8[6] == Byte{ 'g' }, "Should be equal.");
			static_assert(b8[7] == Byte{ 'h' }, "Should be equal.");
		}
	};
}