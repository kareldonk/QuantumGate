// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(RingBufferTests)
	{
	public:
		TEST_METHOD(General)
		{
			const String txt{ L"Man is born free; and everywhere he is in chains. One thinks himself the master of others, "
				"and still remains a greater slave than they. How did this change come about? I do not know. "
				"- Jean Jacques Rousseau" };

			const BufferView txt_buffer(reinterpret_cast<const Byte*>(txt.data()), txt.size() * sizeof(String::value_type));

			// Alloc constructor
			RingBuffer b1(64);
			Assert::AreEqual(true, b1.GetReadSize() == 0);
			Assert::AreEqual(true, b1.GetWriteSize() == b1.GetSize());
			Assert::AreEqual(true, b1.GetSize() == 64);

			// Copy constructor for Byte*
			RingBuffer b2(reinterpret_cast<const Byte*>(txt.data()), txt.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b2.GetSize() == txt.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b2.GetReadSize() == b2.GetSize());
			Assert::AreEqual(true, b2.GetWriteSize() == 0);

			// Copy constructor for BufferView
			RingBuffer b3(txt_buffer);
			Assert::AreEqual(true, b3.GetSize() == txt_buffer.GetSize());
			Assert::AreEqual(true, b3.GetReadSize() == b3.GetSize());
			Assert::AreEqual(true, b3.GetWriteSize() == 0);

			// Move constructor
			RingBuffer b5(std::move(b3));
			Assert::AreEqual(true, b5.GetSize() == txt_buffer.GetSize());
			Assert::AreEqual(true, b5.GetReadSize() == b5.GetSize());
			Assert::AreEqual(true, b5.GetWriteSize() == 0);
			Assert::AreEqual(true, b3.GetSize() == 0);
			Assert::AreEqual(true, b3.GetReadSize() == 0);
			Assert::AreEqual(true, b3.GetWriteSize() == 0);

			// Clear()
			b5.Clear();
			Assert::AreEqual(true, b5.GetReadSize() == 0);
			Assert::AreEqual(true, b5.GetWriteSize() == b5.GetSize());
			Assert::AreEqual(true, b5.GetSize() == txt_buffer.GetSize());

			// Buffer swap
			b5.Swap(b1);
			Assert::AreEqual(true, b5.GetReadSize() == 0);
			Assert::AreEqual(true, b5.GetWriteSize() == b5.GetSize());
			Assert::AreEqual(true, b5.GetSize() == 64);
			Assert::AreEqual(true, b1.GetReadSize() == 0);
			Assert::AreEqual(true, b1.GetWriteSize() == b1.GetSize());
			Assert::AreEqual(true, b1.GetSize() == txt_buffer.GetSize());
		}

		TEST_METHOD(ReadWrite)
		{
			const String txt{ L"Man is born free; and everywhere he is in chains." };

			const BufferView txt_buffer(reinterpret_cast<const Byte*>(txt.data()), txt.size() * sizeof(String::value_type));

			RingBuffer b1(txt_buffer);
			Assert::AreEqual(true, b1.GetSize() == txt_buffer.GetSize());
			Assert::AreEqual(true, b1.GetReadSize() == b1.GetSize());
			Assert::AreEqual(true, b1.GetWriteSize() == 0);

			String s1;
			s1.resize(17);
			auto old_read_size = b1.GetReadSize();
			const auto numread = b1.Read(reinterpret_cast<Byte*>(s1.data()), s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread == s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread);
			Assert::AreEqual(true, b1.GetWriteSize() == numread);
			Assert::AreEqual(true, s1 == L"Man is born free;");

			String s2;
			s2.resize(18);
			old_read_size = b1.GetReadSize();
			const auto numread2 = b1.Read(reinterpret_cast<Byte*>(s2.data()), s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread2 == s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread2);
			Assert::AreEqual(true, b1.GetWriteSize() == numread + numread2);
			Assert::AreEqual(true, s2 == L" and everywhere he");

			// Reading more than exists in the buffer
			String s3;
			s3.resize(20);
			old_read_size = b1.GetReadSize();
			const auto numread3 = b1.Read(reinterpret_cast<Byte*>(s3.data()), s3.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread3 == 14 * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == 0);
			Assert::AreEqual(true, b1.GetWriteSize() == b1.GetSize());

			auto old_write_size = b1.GetWriteSize();
			const auto numwritten = b1.Write(reinterpret_cast<Byte*>(s1.data()), s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numwritten == s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size - numwritten);
			Assert::AreEqual(true, b1.GetReadSize() == numwritten);

			old_write_size = b1.GetWriteSize();
			const auto numwritten2 = b1.Write(reinterpret_cast<Byte*>(s2.data()), s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numwritten2 == s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size - numwritten2);
			Assert::AreEqual(true, b1.GetReadSize() == numwritten + numwritten2);

			// Writing more than the buffer can take
			old_write_size = b1.GetWriteSize();
			const auto numwritten3 = b1.Write(reinterpret_cast<Byte*>(s3.data()), s3.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numwritten3 == 14 * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetWriteSize() == 0);
			Assert::AreEqual(true, b1.GetReadSize() == b1.GetSize());

			// Now read everything written to the ringbuffer;
			// this should match the original text
			String txt2;
			txt2.resize(txt.size());
			const auto numread4 = b1.Read(reinterpret_cast<Byte*>(txt2.data()), txt2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread4 == txt2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == 0);
			Assert::AreEqual(true, b1.GetWriteSize() == b1.GetSize());
			Assert::AreEqual(true, txt == txt2);
		}

		TEST_METHOD(ReadWrap)
		{
			const String txt{ L"Man is born free; and everywhere he is in chains." };

			const BufferView txt_buffer(reinterpret_cast<const Byte*>(txt.data()), txt.size() * sizeof(String::value_type));

			RingBuffer b1(txt_buffer);
			Assert::AreEqual(true, b1.GetSize() == txt_buffer.GetSize());
			Assert::AreEqual(true, b1.GetReadSize() == b1.GetSize());
			Assert::AreEqual(true, b1.GetWriteSize() == 0);

			String s1;
			s1.resize(17);
			auto old_read_size = b1.GetReadSize();
			const auto numread = b1.Read(reinterpret_cast<Byte*>(s1.data()), s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread == s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread);
			Assert::AreEqual(true, b1.GetWriteSize() == numread);
			Assert::AreEqual(true, s1 == L"Man is born free;");

			String s2;
			s2.resize(18);
			old_read_size = b1.GetReadSize();
			const auto numread2 = b1.Read(reinterpret_cast<Byte*>(s2.data()), s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread2 == s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread2);
			Assert::AreEqual(true, b1.GetWriteSize() == numread + numread2);
			Assert::AreEqual(true, s2 == L" and everywhere he");

			auto old_write_size = b1.GetWriteSize();
			const auto numwritten = b1.Write(reinterpret_cast<Byte*>(s1.data()), s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numwritten == s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size - numwritten);
			Assert::AreEqual(true, b1.GetReadSize() == (old_read_size - numread2) + numwritten);

			// Reading wrapping around to beginning
			String s3;
			s3.resize(40);
			old_read_size = b1.GetReadSize();
			old_write_size = b1.GetWriteSize();
			const auto numread3 = b1.Read(reinterpret_cast<Byte*>(s3.data()), s3.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread3 == 31 * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread3);
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size + numread3);
			Assert::AreEqual(true, s3.substr(0, 31) == L" is in chains.Man is born free;");
		}

		TEST_METHOD(WriteWrap)
		{
			const String txt{ L"Man is born free; and everywhere he is in chains." };

			const BufferView txt_buffer(reinterpret_cast<const Byte*>(txt.data()), txt.size() * sizeof(String::value_type));

			RingBuffer b1(txt_buffer.GetSize());
			Assert::AreEqual(true, b1.GetSize() == txt_buffer.GetSize());
			Assert::AreEqual(true, b1.GetReadSize() == 0);
			Assert::AreEqual(true, b1.GetWriteSize() == txt_buffer.GetSize());

			const WChar txt1[]{ L"Man is born free;" };
			const auto txt1_len{ sizeof(txt1) - sizeof(WChar) }; // Not including last '\0' character

			auto old_write_size = b1.GetWriteSize();
			const auto numwritten = b1.Write(reinterpret_cast<const Byte*>(txt1), txt1_len);
			Assert::AreEqual(true, numwritten == txt1_len);
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size - txt1_len);
			Assert::AreEqual(true, b1.GetReadSize() == txt1_len);

			const WChar txt2[]{ L" and everywhere he" };
			const auto txt2_len{ sizeof(txt2) - sizeof(WChar) }; // Not including last '\0' character

			old_write_size = b1.GetWriteSize();
			const auto numwritten2 = b1.Write(reinterpret_cast<const Byte*>(txt2), txt2_len);
			Assert::AreEqual(true, numwritten2 == txt2_len);
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size - txt2_len);
			Assert::AreEqual(true, b1.GetReadSize() == txt1_len + txt2_len);

			String s1;
			s1.resize(17);
			old_write_size = b1.GetWriteSize();
			auto old_read_size = b1.GetReadSize();
			const auto numread = b1.Read(reinterpret_cast<Byte*>(s1.data()), s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread == s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread);
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size + numread);
			Assert::AreEqual(true, s1 == L"Man is born free;");

			// Writing wrapping around to the beginning
			const WChar txt3[]{ L" is in chains.Man is born free;" };
			const auto txt3_len{ sizeof(txt3) - sizeof(WChar) }; // Not including last '\0' character

			old_write_size = b1.GetWriteSize();
			old_read_size = b1.GetReadSize();
			const auto numwritten3 = b1.Write(reinterpret_cast<const Byte*>(txt3), txt3_len);
			Assert::AreEqual(true, numwritten3 == txt3_len);
			Assert::AreEqual(true, b1.GetWriteSize() == 0);
			Assert::AreEqual(true, b1.GetReadSize() == b1.GetSize());
		}

		TEST_METHOD(ReadWriteToBuffer)
		{
			const String txt{ L"Man is born free; and everywhere he is in chains." };

			const BufferView txt_buffer(reinterpret_cast<const Byte*>(txt.data()), txt.size() * sizeof(String::value_type));

			RingBuffer b1(txt_buffer);
			Assert::AreEqual(true, b1.GetSize() == txt_buffer.GetSize());
			Assert::AreEqual(true, b1.GetReadSize() == b1.GetSize());
			Assert::AreEqual(true, b1.GetWriteSize() == 0);

			Buffer rb1(17);
			const auto old_read_size = b1.GetReadSize();
			const auto numread = b1.Read(rb1);
			Assert::AreEqual(true, numread == rb1.GetSize());
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - 17);
			Assert::AreEqual(true, b1.GetWriteSize() == 17);
			Assert::AreEqual(true, rb1 == txt_buffer.GetFirst(17));

			const auto old_write_size = b1.GetWriteSize();
			const auto numwritten = b1.Write(rb1);
			Assert::AreEqual(true, numwritten == 17);
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size - 17);
			Assert::AreEqual(true, b1.GetReadSize() == b1.GetSize());
		}

		TEST_METHOD(ResizeBigger)
		{
			const String txt{ L"Man is born free; and everywhere he is in chains." };

			const BufferView txt_buffer(reinterpret_cast<const Byte*>(txt.data()), txt.size() * sizeof(String::value_type));

			RingBuffer b1(txt_buffer);
			Assert::AreEqual(true, b1.GetSize() == txt_buffer.GetSize());
			Assert::AreEqual(true, b1.GetReadSize() == b1.GetSize());
			Assert::AreEqual(true, b1.GetWriteSize() == 0);

			String s1;
			s1.resize(17);
			auto old_read_size = b1.GetReadSize();
			const auto numread = b1.Read(reinterpret_cast<Byte*>(s1.data()), s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread == s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread);
			Assert::AreEqual(true, b1.GetWriteSize() == numread);
			Assert::AreEqual(true, s1 == L"Man is born free;");

			String s2;
			s2.resize(18);
			old_read_size = b1.GetReadSize();
			const auto numread2 = b1.Read(reinterpret_cast<Byte*>(s2.data()), s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread2 == s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread2);
			Assert::AreEqual(true, b1.GetWriteSize() == numread + numread2);
			Assert::AreEqual(true, s2 == L" and everywhere he");

			auto old_write_size = b1.GetWriteSize();
			const auto numwritten = b1.Write(reinterpret_cast<Byte*>(s1.data()), s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numwritten == s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size - numwritten);
			Assert::AreEqual(true, b1.GetReadSize() == (old_read_size - numread2) + numwritten);

			const auto old_size = b1.GetSize();
			old_read_size = b1.GetReadSize();
			old_write_size = b1.GetWriteSize();
			b1.Resize(txt_buffer.GetSize() + 20);
			Assert::AreEqual(true, b1.GetSize() == old_size + 20);
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size + 20);
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size);

			String s3;
			s3.resize(40);
			old_read_size = b1.GetReadSize();
			old_write_size = b1.GetWriteSize();
			const auto numread3 = b1.Read(reinterpret_cast<Byte*>(s3.data()), s3.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread3 == 31 * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread3);
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size + numread3);
			Assert::AreEqual(true, s3.substr(0, 31) == L" is in chains.Man is born free;");
		}

		TEST_METHOD(ResizeSmaller)
		{
			const String txt{ L"Man is born free; and everywhere he is in chains." };

			const BufferView txt_buffer(reinterpret_cast<const Byte*>(txt.data()), txt.size() * sizeof(String::value_type));

			RingBuffer b1(txt_buffer);
			Assert::AreEqual(true, b1.GetSize() == txt_buffer.GetSize());
			Assert::AreEqual(true, b1.GetReadSize() == b1.GetSize());
			Assert::AreEqual(true, b1.GetWriteSize() == 0);

			String s1;
			s1.resize(17);
			auto old_read_size = b1.GetReadSize();
			const auto numread = b1.Read(reinterpret_cast<Byte*>(s1.data()), s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread == s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread);
			Assert::AreEqual(true, b1.GetWriteSize() == numread);
			Assert::AreEqual(true, s1 == L"Man is born free;");

			String s2;
			s2.resize(18);
			old_read_size = b1.GetReadSize();
			const auto numread2 = b1.Read(reinterpret_cast<Byte*>(s2.data()), s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread2 == s2.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread2);
			Assert::AreEqual(true, b1.GetWriteSize() == numread + numread2);
			Assert::AreEqual(true, s2 == L" and everywhere he");

			auto old_write_size = b1.GetWriteSize();
			const auto numwritten = b1.Write(reinterpret_cast<Byte*>(s1.data()), s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numwritten == s1.size() * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size - numwritten);
			Assert::AreEqual(true, b1.GetReadSize() == (old_read_size - numread2) + numwritten);

			const auto old_size = b1.GetSize();
			old_read_size = b1.GetReadSize();
			old_write_size = b1.GetWriteSize();
			b1.Resize(txt_buffer.GetSize() - 40);
			Assert::AreEqual(true, b1.GetSize() == old_size - 40);
			Assert::AreEqual(true, b1.GetWriteSize() == 0);
			Assert::AreEqual(true, b1.GetReadSize() == 58);

			String s3;
			s3.resize(40);
			old_read_size = b1.GetReadSize();
			old_write_size = b1.GetWriteSize();
			const auto numread3 = b1.Read(reinterpret_cast<Byte*>(s3.data()), s3.size() * sizeof(String::value_type));
			Assert::AreEqual(true, numread3 == 29 * sizeof(String::value_type));
			Assert::AreEqual(true, b1.GetReadSize() == old_read_size - numread3);
			Assert::AreEqual(true, b1.GetWriteSize() == old_write_size + numread3);
			Assert::AreEqual(true, s3.substr(0, 29) == L" is in chains.Man is born fre");
		}
	};
}