// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Memory\BufferView.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(BufferViewTests)
	{
	public:
		TEST_METHOD(General)
		{
			char quote[] = "When you are presented with a choice between two evils, "
							" do not choose the lesser. Choose the exit. - Robert Higgs";

			// Default constructor
			{
				BufferView bview;
				Assert::AreEqual(false, bview.operator bool());
				Assert::AreEqual(true, bview.IsEmpty());
				Assert::AreEqual(true, bview.GetSize() == 0);
				Assert::AreEqual(true, bview.GetBytes() == nullptr);
			}

			// nullptr constructor
			{
				BufferView bview;
				Assert::AreEqual(false, bview.operator bool());
				Assert::AreEqual(true, bview.IsEmpty());
				Assert::AreEqual(true, bview.GetSize() == 0);
				Assert::AreEqual(true, bview.GetBytes() == nullptr);
			}

			// Copy constructor
			{
				BufferView bview1(reinterpret_cast<Byte*>(quote), sizeof(quote));

				BufferView bview2(bview1);
				Assert::AreEqual(true, bview2.operator bool());
				Assert::AreEqual(false, bview2.IsEmpty());
				Assert::AreEqual(true, bview2.GetSize() == 115);
				Assert::AreEqual(true, bview2.GetBytes() != nullptr);
			}

			// Copy assignment
			{
				BufferView bview1(reinterpret_cast<Byte*>(quote), sizeof(quote));

				BufferView bview2 = bview1;
				Assert::AreEqual(true, bview2.operator bool());
				Assert::AreEqual(false, bview2.IsEmpty());
				Assert::AreEqual(true, bview2.GetSize() == 115);
				Assert::AreEqual(true, bview2.GetBytes() != nullptr);
			}

			// Move constructor
			{
				BufferView bview1(reinterpret_cast<Byte*>(quote), sizeof(quote));

				BufferView bview2(std::move(bview1));
				Assert::AreEqual(true, bview2.operator bool());
				Assert::AreEqual(false, bview2.IsEmpty());
				Assert::AreEqual(true, bview2.GetSize() == 115);
				Assert::AreEqual(true, bview2.GetBytes() != nullptr);
			}

			// Move assignment
			{
				BufferView bview1(reinterpret_cast<Byte*>(quote), sizeof(quote));

				BufferView bview2 = std::move(bview1);
				Assert::AreEqual(true, bview2.operator bool());
				Assert::AreEqual(false, bview2.IsEmpty());
				Assert::AreEqual(true, bview2.GetSize() == 115);
				Assert::AreEqual(true, bview2.GetBytes() != nullptr);
			}
		}

		TEST_METHOD(Operations)
		{
			char quote[] = "So long as they don't get violent, I want to let everyone say what they wish, "
							"for I myself have always said exactly what pleased me. – Albert Einstein";
			
			Buffer buf(reinterpret_cast<Byte*>(quote), sizeof(quote));

			BufferView bview(buf);
			Assert::AreEqual(true, bview.operator bool());
			Assert::AreEqual(false, bview.IsEmpty());
			Assert::AreEqual(true, (bview.GetSize() == buf.GetSize()));

			bview.RemoveFirst(3);
			Assert::AreEqual(true, (bview.GetSize() == buf.GetSize() - 3));

			bview.RemoveLast(10);
			Assert::AreEqual(true, (bview.GetSize() == buf.GetSize() - 13));

			Assert::AreEqual(true, (bview[0] == Byte{ 'l' }));
			Assert::AreEqual(true, (bview[bview.GetSize() - 1] == Byte{ 't' }));

			auto bviewf = bview.GetFirst(4);
			Assert::AreEqual(true, (bviewf.GetSize() == 4));

			auto bviewsub = bview.GetSub(0, 4);
			Assert::AreEqual(true, (bviewsub.GetSize() == 4));
			Assert::AreEqual(true, std::memcmp(bviewsub.GetBytes(), "long", bviewsub.GetSize()) == 0);

			Assert::AreEqual(true, (bviewsub == bviewf));

			auto bviewl = bview.GetLast(6);
			Assert::AreEqual(true, (bviewl.GetSize() == 6));

			auto bviewsub2 = bview.GetSub(bview.GetSize()-6, 6);
			Assert::AreEqual(true, (bviewsub2.GetSize() == 6));
			Assert::AreEqual(true, std::memcmp(bviewsub2.GetBytes(), "Albert", bviewsub2.GetSize()) == 0);

			Assert::AreEqual(true, (bviewsub2 == bviewl));

			Assert::AreEqual(false, (bviewf == bviewl));
			Assert::AreEqual(true, (bviewf != bviewl));

			bviewsub.RemoveFirst(4);
			Assert::AreEqual(true, bviewsub.IsEmpty());
			Assert::AreEqual(false, bviewsub.operator bool());

			bviewsub2.RemoveLast(6);
			Assert::AreEqual(true, bviewsub2.IsEmpty());
			Assert::AreEqual(false, bviewsub2.operator bool());
		}
	};
}