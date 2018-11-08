// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Common\Util.h"
#include "Common\Endian.h"
#include "Compression\Compression.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(CompressionTests)
	{
	public:
		TEST_METHOD(General)
		{
			std::vector<String> comprstr = {
				L"A",
				L"Small string",
				L"Taxation is theft and slavery.\r\n\"Money is a new form of slavery, and distinguishable from the old \
				simply by the fact that it is impersonal; there is no human relation between master and slave. \
				The essence of all slavery consists in taking the product of another's labor by force. It is immaterial \
				whether this force be founded upon ownership of the slave or ownership of the money that he must get to \
				live.\" - Leo Tolstoy\r\n\r\n \
				\"Whoever controls the volume of money in our country is absolute master of all industry and commerce [...] \
				when you realize that the entire system is very easily controlled, one way or another, by a few powerful \
				men at the top, you will not have to be told how periods of inflation and depression originate.\" - \
				James A.Garfield"
			};

			std::vector<Buffer> inputbufs;

			// Test empty buffer
			inputbufs.insert(inputbufs.end(), Buffer());

			// Test buffer with data
			for (const auto& str : comprstr)
			{
				auto len = str.size() * sizeof(String::value_type);
				Buffer inbuf(reinterpret_cast<const Byte*>(str.data()), len);
				inputbufs.insert(inputbufs.end(), std::move(inbuf));
			}

			for (const auto& input : inputbufs)
			{
				Buffer zloutbuf, zstdoutbuf;

				Assert::AreEqual(true, Compression::Compress(input, zloutbuf, Algorithm::Compression::DEFLATE));
				Assert::AreEqual(true, Compression::Compress(input, zstdoutbuf, Algorithm::Compression::ZSTANDARD));

				Buffer zloutbuf2, zstdoutbuf2;

				if (!input.IsEmpty())
				{
					// Max size is too low so should fail
					Assert::AreEqual(false, Compression::Decompress(zloutbuf, zloutbuf2, Algorithm::Compression::DEFLATE, input.GetSize() - 1));
					Assert::AreEqual(false, Compression::Decompress(zstdoutbuf, zstdoutbuf2, Algorithm::Compression::ZSTANDARD, input.GetSize() - 1));
				}

				Assert::AreEqual(true, Compression::Decompress(zloutbuf, zloutbuf2, Algorithm::Compression::DEFLATE, input.GetSize()));
				Assert::AreEqual(true, Compression::Decompress(zstdoutbuf, zstdoutbuf2, Algorithm::Compression::ZSTANDARD, input.GetSize()));

				// Decompressed data must match original input
				Assert::AreEqual(true, (zloutbuf2 == input));
				Assert::AreEqual(true, (zstdoutbuf2 == input));
			}
		}

		TEST_METHOD(BadData)
		{
			// Random garbage data
			Buffer comprdata = Util::GetPseudoRandomBytes(400);

			std::vector<UInt32> sizes = { 0, 1, 2, 100, 400, 64000 };

			for (auto size : sizes)
			{
				UInt32 suhdr = Endian::ToNetworkByteOrder(size);
				memcpy(comprdata.GetBytes(), &suhdr, sizeof(suhdr));
				
				Buffer outbuf;

				Assert::AreEqual(false, Compression::Decompress(comprdata, outbuf, Algorithm::Compression::DEFLATE));
				Assert::AreEqual(false, Compression::Decompress(comprdata, outbuf, Algorithm::Compression::ZSTANDARD));
			}
		}
	};
}