// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Common\Util.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation;

namespace UnitTests
{
	TEST_CLASS(UtilTests)
	{
	public:
		TEST_METHOD(FormatString)
		{
			const auto str1 = Util::FormatString(L"Testing 1 2 3");
			Assert::AreEqual(true, str1 == L"Testing 1 2 3");

			const auto str2 = Util::FormatString(L"Testing 1 2 3 %s", L"4");
			Assert::AreEqual(true, str2 == L"Testing 1 2 3 4");

			const String test{ L"world" };
			const auto str3 = Util::FormatString(L"Hello %s", test.c_str());
			Assert::AreEqual(true, str3 == L"Hello world");

			const int int1{ -1 };
			const unsigned int uint{ 20 };
			const unsigned long long ull{ 30 };
			const double d{ 3.384 };
			const auto str4 = Util::FormatString(L"Test %d %u %llu %.3f", int1, uint, ull, d);
			Assert::AreEqual(true, str4 == L"Test -1 20 30 3.384");
		}

		TEST_METHOD(ToString)
		{
			{
				const std::string str{ "A drop of ink may make a million think." };
				const auto wstr = Util::ToStringW(str);
				Assert::AreEqual(true, wstr == L"A drop of ink may make a million think.");
				const auto str2 = Util::ToStringA(wstr);
				Assert::AreEqual(true, str == str2);
			}

			{
				const auto chars = u8"ÜüΩωЙ你月曜日a🐕èéøÞǽлљΣæča🐕🐕";
				const std::string strt{ reinterpret_cast<const char*>(chars) };
				const String wstrt{ L"ÜüΩωЙ你月曜日a🐕èéøÞǽлљΣæča🐕🐕" };
				const auto wstr = Util::ToStringW(strt);
				Assert::AreEqual(true, wstr == wstrt);
				const auto str = Util::ToStringA(wstr);
				Assert::AreEqual(true, str == strt);
			}

			{
				const ProtectedStringA str{ "A drop of ink may make a million think." };
				const auto wstr = Util::ToProtectedStringW(str);
				Assert::AreEqual(true, wstr == L"A drop of ink may make a million think.");
				const auto str2 = Util::ToProtectedStringA(wstr);
				Assert::AreEqual(true, str == str2);
			}

			{
				const auto chars = u8"ÜüΩωЙ你月曜日a🐕èéøÞǽлљΣæča🐕🐕";
				const ProtectedStringA strt{ reinterpret_cast<const char*>(chars) };
				const ProtectedString wstrt{ L"ÜüΩωЙ你月曜日a🐕èéøÞǽлљΣæča🐕🐕" };
				const auto wstr = Util::ToProtectedStringW(strt);
				Assert::AreEqual(true, wstr == wstrt);
				const auto str = Util::ToProtectedStringA(wstr);
				Assert::AreEqual(true, str == strt);
			}
		}

		template<typename T>
		struct BinTest
		{
			T num{ 0 };
			String str;
		};

		TEST_METHOD(ToBinaryString)
		{
			{
				const std::vector<BinTest<UInt8>> bintests
				{
					{ 0, L"00000000" },
					{ 1, L"00000001" },
					{ 11, L"00001011" },
					{ 96, L"01100000" },
					{ 128, L"10000000" },
					{ 255, L"11111111" }
				};

				for (const auto& test : bintests)
				{
					Assert::AreEqual(true, Util::ToBinaryString(test.num).data() == test.str);
				}
			}

			{
				const std::vector<BinTest<Int8>> bintests
				{
					{ 0, L"00000000" },
					{ 1, L"00000001" },
					{ -2, L"11111110" },
					{ -1, L"11111111" }
				};

				for (const auto& test : bintests)
				{
					Assert::AreEqual(true, Util::ToBinaryString(test.num).data() == test.str);
				}
			}

			{
				const std::vector<BinTest<UInt16>> bintests
				{
					{ 0, L"00000000'00000000" },
					{ 1, L"00000000'00000001" },
					{ 11, L"00000000'00001011" },
					{ 96, L"00000000'01100000" },
					{ 128, L"00000000'10000000" },
					{ 255, L"00000000'11111111" },
					{ 500, L"00000001'11110100" },
					{ 60000, L"11101010'01100000" },
					{ std::numeric_limits<UInt16>::max(), L"11111111'11111111" }
				};

				for (const auto& test : bintests)
				{
					Assert::AreEqual(true, Util::ToBinaryString(test.num).data() == test.str);
				}
			}

			{
				const std::vector<BinTest<UInt64>> bintests
				{
					{ 0, L"00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000000" },
					{ 1, L"00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000001" },
					{ 11, L"00000000'00000000'00000000'00000000'00000000'00000000'00000000'00001011" },
					{ 96, L"00000000'00000000'00000000'00000000'00000000'00000000'00000000'01100000" },
					{ 128, L"00000000'00000000'00000000'00000000'00000000'00000000'00000000'10000000" },
					{ 255, L"00000000'00000000'00000000'00000000'00000000'00000000'00000000'11111111" },
					{ 500, L"00000000'00000000'00000000'00000000'00000000'00000000'00000001'11110100" },
					{ 60000, L"00000000'00000000'00000000'00000000'00000000'00000000'11101010'01100000" },
					{ 4918988518979594848, L"01000100'01000011'11000001'11111111'00000000'00000000'11101010'01100000" },
					{ std::numeric_limits<UInt64>::max(), L"11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111" }
				};

				for (const auto& test : bintests)
				{
					Assert::AreEqual(true, Util::ToBinaryString(test.num).data() == test.str);
				}
			}
		}

		template<typename S, typename B>
		void Base64Impl()
		{
			{
				const S str{ L"To disagree with three-fourths of the public is one of the first requisites of sanity." };
				const S strb64t{
					L"VABvACAAZABpAHMAYQBnAHIAZQBlACAAdwBpAHQAaAAgAHQAaAByAGUAZQAtAGYAbwB1AHIAdABoAHMAIABvAG"
					"YAIAB0AGgAZQAgAHAAdQBiAGwAaQBjACAAaQBzACAAbwBuAGUAIABvAGYAIAB0AGgAZQAgAGYAaQByAHMAdAA"
					"gAHIAZQBxAHUAaQBzAGkAdABlAHMAIABvAGYAIABzAGEAbgBpAHQAeQAuAA=="
				};

				const B buffer(reinterpret_cast<const Byte*>(str.data()), str.size() * sizeof(S::value_type));
				const auto strb64 = Util::ToBase64(buffer);
				Assert::AreEqual(true, strb64.has_value());
				Assert::AreEqual(true, strb64 == strb64t);

				const auto buffer2 = Util::FromBase64(*strb64);
				Assert::AreEqual(true, buffer2.has_value());
				Assert::AreEqual(true, buffer == buffer2);

				S str2;
				str2.resize(buffer2->GetSize() / sizeof(S::value_type));
				memcpy(str2.data(), buffer2->GetBytes(), buffer2->GetSize());

				Assert::AreEqual(true, str == str2);
			}

			{
				const std::string str{ "To disagree with three-fourths of the public is one of the first requisites of sanity." };
				const S strb64t{ L"VG8gZGlzYWdyZWUgd2l0aCB0aHJlZS1mb3VydGhzIG9mIHRoZSBwdWJsaWMgaXMgb25lIG9mIHRoZSBmaXJzdCByZXF1aXNpdGVzIG9mIHNhbml0eS4=" };

				const auto buffer = Util::FromBase64(strb64t);
				Assert::AreEqual(true, buffer.has_value());

				std::string str2;
				str2.resize(buffer->GetSize() / sizeof(std::string::value_type));
				memcpy(str2.data(), buffer->GetBytes(), buffer->GetSize());

				Assert::AreEqual(true, str == str2);

				const auto strb64 = Util::ToBase64(*buffer);
				Assert::AreEqual(true, strb64 == strb64t);
			}
		}

		TEST_METHOD(Base64)
		{
			Base64Impl<String, Buffer>();
			Base64Impl<ProtectedString, ProtectedBuffer>();
		}
	};
}