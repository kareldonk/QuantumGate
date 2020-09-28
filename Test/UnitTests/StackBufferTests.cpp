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

			// Allocation
			b1.Allocate(10);
			Assert::AreEqual(false, b1.IsEmpty());
			Assert::AreEqual(true, b1.operator bool());
			Assert::AreEqual(true, b1.GetSize() == 10);

			StackBuffer128 b1b;
			Assert::ExpectException<BadAllocException>([&]() { b1b.Allocate(129); });

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
			Assert::ExpectException<BadAllocException>([]() { StackBuffer128 b3b(129); });

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
			Assert::ExpectException<BadAllocException>([&]() { b1b = b2; });

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
			Assert::ExpectException<BadAllocException>([&]() { b2 += b4; });

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
	};
}