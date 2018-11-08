// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(BufferTests)
	{
	public:
		TEST_METHOD(General)
		{
			String txt = L"All discussions on the question of whether man is good or evil, a social or \
				antisocial being, are philosophic game-playing. Whether man is a social being or a mass \
				of protoplasm reacting in a peculiar and irrational way depends on whether his basic \
				biological needs are in harmony or at variance with the institutions he has created for \
				himself. - Wilhelm Reich";

			// Default constructor
			Buffer b1;
			Assert::AreEqual(true, b1.IsEmpty());
			Assert::AreEqual(false, b1.operator bool());
			Assert::AreEqual(true, b1.GetSize() == 0);

			// Allocation
			b1.Allocate(10);
			Assert::AreEqual(false, b1.IsEmpty());
			Assert::AreEqual(true, b1.operator bool());
			Assert::AreEqual(true, b1.GetSize() == 10);

			// Copy constructor for Byte*
			Buffer b2(reinterpret_cast<Byte*>(txt.data()), txt.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b2.GetSize() == txt.size() * sizeof(String::value_type));
			Assert::AreEqual(true, memcmp(b2.GetBytes(), txt.data(), b2.GetSize()) == 0);
			Assert::AreEqual(true, b1 != b2);
			Assert::AreEqual(true, b2.operator bool());

			// Alloc constructor
			Buffer b3(txt.size() * sizeof(String::value_type));
			memcpy(b3.GetBytes(), reinterpret_cast<Byte*>(txt.data()), txt.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b2 == b3);

			// Copy constructor for Buffer
			Buffer b4(b3);
			Assert::AreEqual(true, b4 == b3);
			Assert::AreEqual(true, b4.GetSize() == b3.GetSize());

			// Move constructor
			Buffer b5(std::move(b4));
			Assert::AreEqual(true, b5 == b3);
			Assert::AreEqual(true, b5.GetSize() == b3.GetSize());
			Assert::AreEqual(true, b4.IsEmpty());
			Assert::AreEqual(true, b4.GetSize() == 0);

			Buffer::VectorType vb(txt.size() * sizeof(String::value_type));
			memcpy(vb.data(), reinterpret_cast<Byte*>(txt.data()), txt.size() * sizeof(String::value_type));

			// Move constructor for Buffer::VectorType
			Buffer b6(std::move(vb));
			Assert::AreEqual(true, b6 == b3);
			Assert::AreEqual(true, b6.GetSize() == b3.GetSize());
			Assert::AreEqual(true, vb.empty());
			Assert::AreEqual(true, vb.size() == 0);

			// Copy assignment
			b1 = b2;
			Assert::AreEqual(true, b1 == b2);
			Assert::AreEqual(true, b1.GetSize() == b2.GetSize());

			// Move assignment for Buffer
			b4 = std::move(b5);
			Assert::AreEqual(true, b4 == b3);
			Assert::AreEqual(true, b4.GetSize() == b3.GetSize());
			Assert::AreEqual(true, b5.IsEmpty());
			Assert::AreEqual(true, b5.GetSize() == 0);

			// Buffer::VectorType copy
			Buffer::VectorType vb2 = b3.GetVector();

			// Move assignment for Buffer::VectorType
			b5 = std::move(vb2);
			Assert::AreEqual(true, b5 == b3);
			Assert::AreEqual(true, b5.GetSize() == b3.GetSize());
			Assert::AreEqual(true, vb2.empty());
			Assert::AreEqual(true, vb2.size() == 0);

			b4.Clear();
			Assert::AreEqual(true, b4.IsEmpty());
			Assert::AreEqual(true, b4.GetSize() == 0);

			// Buffer::VectorType move
			vb2 = std::move(b2.GetVector());
			Assert::AreEqual(true, vb2 == b3.GetVector());
			Assert::AreEqual(true, b2.IsEmpty());
			Assert::AreEqual(true, b2.GetSize() == 0);

			// Buffer::VectorType swap
			b2.Swap(vb2);
			Assert::AreEqual(true, b2 == b3);
			Assert::AreEqual(true, vb2.empty());
			Assert::AreEqual(true, vb2.size() == 0);

			// Buffer swap
			b4.Swap(b2);
			Assert::AreEqual(true, b4 == b3);
			Assert::AreEqual(true, b2.IsEmpty());
			Assert::AreEqual(true, b2.GetSize() == 0);
		}

		TEST_METHOD(BufferAndBufferView)
		{
			std::string txt = "Be a loner. That gives you time to wonder, to search for the truth. \
								Have holy curiosity. Make your life worth living. - Albert Einstein";

			Buffer b1(reinterpret_cast<Byte*>(txt.data()), txt.size());

			BufferView bview(b1);

			// Copy construction/assignment
			Buffer b2 = bview;

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
			Buffer b3(bview2);
			Assert::AreEqual(true, b3.IsEmpty());
			Assert::AreEqual(true, b3.GetSize() == 0);

			b3 += bview2;
			Assert::AreEqual(true, b3.IsEmpty());
			Assert::AreEqual(true, b3.GetSize() == 0);

			Buffer b4(b3);
			Assert::AreEqual(true, b4.IsEmpty());
			Assert::AreEqual(true, b4.GetSize() == 0);

			BufferView bview3(b2);
			// Copy assignment
			b4 = bview3;

			Assert::AreEqual(true, b2 == b4);
		}
	};
}