// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Memory\BufferReader.h"
#include "Memory\BufferWriter.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Memory;
using namespace QuantumGate::Implementation::Network;

enum BufferReadWriteTest { One, Two, Three };

namespace UnitTests
{
	TEST_CLASS(BufferReaderWriterTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Write various types of data to a buffer and then try to read it back
			// again in both little and big endian formats

			std::array<bool, 2> endian{ false, true };

			for (auto nbo : endian)
			{
				auto uint8t = UInt8{ 9 };
				auto uint16t = UInt16{ 99 };
				auto uint32t = UInt32{ 999 };
				auto uint64t = UInt64{ 999999 };
				auto enumt = BufferReadWriteTest::Two;
				auto uuidt = SerializedUUID{ QuantumGate::UUID(L"b51ba1b5-c6c5-89a0-cb70-6b8d93da06df") };
				auto ipt = SerializedBinaryIPAddress{ IPAddress(L"192.168.1.1").GetBinary() };
				auto strte = String{ L"" };
				auto strt = String{ L"A free people [claim] their rights as derived from the laws of nature, and \
									not as the gift of their chief magistrate. - Thomas Jefferson" };
				auto numvect = Vector<UInt32>{ 11, 22, 33, 369 };
				auto numvecte = Vector<UInt32>{};
				auto enumvect = Vector<BufferReadWriteTest>{ BufferReadWriteTest::One, BufferReadWriteTest::Three };
				auto uuidvect = Vector<SerializedUUID>{
					SerializedUUID{ QuantumGate::UUID(L"7a954ed4-ce2e-19e8-cb74-eae90dbdaac1") },
					SerializedUUID{ QuantumGate::UUID(L"aaccc955-e4ac-2966-5e74-871fd705739a") },
					SerializedUUID{ QuantumGate::UUID(L"8e7f4795-fe9b-f9b1-8bb3-9be6c1b305bc") }
				};

				auto len = strt.size() * sizeof(String::value_type);
				Buffer buf(len);
				memcpy(buf.GetBytes(), strt.data(), len);

				Buffer bufe;

				BufferWriter wrt(nbo);
				auto ret = wrt.WriteWithPreallocation(uint8t, uint16t, uint32t, uint64t, enumt, uuidt, ipt,
													  WithSize(strte, MaxSize::UInt8), WithSize(strt, MaxSize::UInt16),
													  WithSize(numvect, MaxSize::UInt8), WithSize(numvecte, MaxSize::UInt8),
													  WithSize(enumvect, MaxSize::UInt8), WithSize(uuidvect, MaxSize::UInt8),
													  WithSize(buf, MaxSize::UInt16), WithSize(bufe, MaxSize::UInt8));

				Assert::AreEqual(true, ret);

				Buffer data(wrt.MoveWrittenBytes());

				UInt8 uint8tr{ 0 };
				UInt16 uint16tr{ 0 };
				UInt32 uint32tr{ 0 };
				UInt64 uint64tr{ 0 };
				BufferReadWriteTest enumtr;
				SerializedUUID uuidtr;
				SerializedBinaryIPAddress iptr;
				String strter;
				String strtr;
				Vector<UInt32> numvectr;
				Vector<UInt32> numvecter;
				Vector<BufferReadWriteTest> enumvectr;
				Vector<SerializedUUID> uuidvectr;
				Buffer bufr;
				Buffer bufer;

				BufferReader rdr(data, nbo);
				ret = rdr.Read(uint8tr, uint16tr, uint32tr, uint64tr, enumtr, uuidtr, iptr,
							   WithSize(strter, MaxSize::UInt8), WithSize(strtr, MaxSize::UInt16),
							   WithSize(numvectr, MaxSize::UInt8), WithSize(numvecter, MaxSize::UInt8),
							   WithSize(enumvectr, MaxSize::UInt8), WithSize(uuidvectr, MaxSize::UInt8),
							   WithSize(bufr, MaxSize::UInt16), WithSize(bufe, MaxSize::UInt8));

				Assert::AreEqual(true, ret);

				Assert::AreEqual(true, (uint8tr == uint8t));
				Assert::AreEqual(true, (uint16tr == uint16t));
				Assert::AreEqual(true, (uint32tr == uint32t));
				Assert::AreEqual(true, (uint64tr == uint64t));
				Assert::AreEqual(true, (enumtr == enumt));
				Assert::AreEqual(true, (uuidtr == uuidt));
				Assert::AreEqual(true, (iptr == ipt));
				Assert::AreEqual(true, (strter == strte));
				Assert::AreEqual(true, (strtr == strt));
				Assert::AreEqual(true, (numvectr == numvect));
				Assert::AreEqual(true, (numvecter == numvecte));
				Assert::AreEqual(true, (enumvectr == enumvect));
				Assert::AreEqual(true, (uuidvectr == uuidvect));
				Assert::AreEqual(true, (bufr == buf));
				Assert::AreEqual(true, (bufer == bufe));
			}
		}

		TEST_METHOD(BadData)
		{
			auto uint64t = UInt64{ 999999 };

			BufferWriter wrt;
			Assert::AreEqual(true, wrt.WriteWithPreallocation(uint64t));

			Buffer data(wrt.MoveWrittenBytes());

			UInt64 uint64tr{ 0 };
			BufferReader rdr(data);
			Assert::AreEqual(true, rdr.Read(uint64tr));
			Assert::AreEqual(true, (uint64tr == uint64t));

			// Try to read more bytes than exist in the buffer; should fail
			BufferReader rdr2(data);
			Buffer tbuf(20);
			Assert::AreEqual(false, rdr2.Read(tbuf));
			// Again
			Vector<UInt32> tvec(10);
			Assert::AreEqual(false, rdr2.Read(tvec));
			// And again
			String tstr;
			tstr.resize(10);
			Assert::AreEqual(false, rdr2.Read(tstr));
			// And once again
			BufferView datav(data);
			datav.RemoveFirst(2);
			BufferReader rdr3(datav);
			Assert::AreEqual(false, rdr3.Read(uint64tr));

			auto strt = String{ L"The abrogation of natural laws from human societies and their replacement \
								by conventional laws is the fundamental danger that threatens freedom. Any \
								ruling system must be made subservient to natural laws, not the reverse. \
								- Muammar al-Qaddafi" };

			BufferWriter wrt2;
			Assert::AreEqual(true, wrt2.WriteWithPreallocation(WithSize(strt, MaxSize::UInt16)));
			Buffer data2(wrt2.MoveWrittenBytes());

			String strt2;
			BufferReader rdr4(data2);

			// Try to read data that's bigger than the max expected size
			Assert::AreEqual(false, rdr4.Read(WithSize(strt2, MaxSize::UInt8)));
			// Bigger expected size should work
			Assert::AreEqual(true, rdr4.Read(WithSize(strt2, MaxSize::UInt16)));

			BufferView datav2(data2);
			datav2.RemoveLast(1);

			// Data is smaller than the saved size encoded at the beginning; should fail
			BufferReader rdr5(datav2);
			Assert::AreEqual(false, rdr5.Read(WithSize(strt2, MaxSize::UInt16)));
		}
	};
}