// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\IMFAddress.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	constexpr bool CheckIMFAddressConstexpr() noexcept
	{
		// Default construction
		IMFAddress addr1;
		auto success = (addr1.GetFamily() == IMFAddress::Family::Unspecified);
		success &= (addr1.GetBinary() == BinaryIMFAddress());

		// Construction
		auto bin_addr = BinaryIMFAddress(BinaryIMFAddress::Family::IMF, L"test@example.com");
		IMFAddress addr2(bin_addr);
		auto bin_addr2 = addr2.GetBinary();
		auto family = addr2.GetFamily();

		success &= (family == IMFAddress::Family::IMF);
		success &= (bin_addr2 == bin_addr);

		// Copy construction
		IMFAddress addr3(addr2);
		success &= (addr3.GetFamily() == IMFAddress::Family::IMF);
		success &= (addr3.GetBinary() == bin_addr2);
		success &= (StringView(addr3.GetBinary().GetChars()) == L"test@example.com");

		// Equal and not equal
		success &= (addr2 == addr3);
		success &= (!(addr2 != addr3));
		success &= (addr1 != addr2);

		// Move construction
		IMFAddress addr4(std::move(addr2));
		success &= (addr4 != addr2);
		success &= (addr4.GetFamily() == IMFAddress::Family::IMF);
		success &= (addr4.GetBinary() == bin_addr2);
		success &= (StringView(addr4.GetBinary().GetChars()) == L"test@example.com");

		// Copy assignment
		auto addr5 = addr3;
		success &= (addr5 == addr3);

		// Move assignment
		auto addr6 = std::move(addr3);
		success &= (addr6 != addr3);
		success &= (addr6.GetFamily() == IMFAddress::Family::IMF);
		success &= (addr6.GetBinary() == bin_addr2);
		success &= (StringView(addr6.GetBinary().GetChars()) == L"test@example.com");

		return success;
	}

	TEST_CLASS(IMFAddressTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Default construction
			IMFAddress addr1;
			Assert::IsTrue(addr1.GetString() == L"");
			Assert::IsTrue(addr1.GetFamily() == IMFAddress::Family::Unspecified);

			// Construction
			IMFAddress addr2(L"test@example.com");
			Assert::IsTrue(addr2.GetString() == L"test@example.com");
			Assert::IsTrue(addr2.GetFamily() == IMFAddress::Family::IMF);

			// Copy construction
			IMFAddress addr3(addr2);
			Assert::IsTrue(addr3.GetString() == L"test@example.com");
			Assert::IsTrue(addr3.GetFamily() == IMFAddress::Family::IMF);

			// Equal and not equal
			Assert::IsTrue(addr2 == addr3);
			Assert::IsFalse(addr2 != addr3);
			Assert::IsTrue(addr1 != addr2);

			// Move construction
			IMFAddress addr4(std::move(addr2));
			Assert::IsTrue(addr3 == addr4);

			// Copy assignment
			addr1 = addr3;
			Assert::IsTrue(addr3 == addr1);

			// Move assignment
			const auto addr5 = std::move(addr3);
			Assert::IsTrue(addr5 == addr1);

			// GetBinary
			Assert::IsTrue(addr1.GetBinary().AddressFamily == BinaryIMFAddress::Family::IMF);
			Assert::IsTrue(StringView(addr1.GetBinary().GetChars()) == StringView(L"test@example.com"));
			Assert::IsTrue(addr1.GetBinary().GetSize() == 17);

			// GetFamily
			Assert::IsTrue(addr1.GetFamily() == IMFAddress::Family::IMF);
		}

		TEST_METHOD(Input)
		{
			// Test invalid addresses
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L""); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"example"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"@"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"@example"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test.@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@example.."); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test:test@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@example:example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"te..st@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test\\@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test @example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@-example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@example-.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@example..com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@.example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"\"\"\"@iana.org"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghiklmn@example.com"); }); // local part too long
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test\"@example..com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"\"test@example..com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test\"text\"@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@255.255.255.255"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@a[255.255.255.255]"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@1111:2222:3333:4444:5555:6666:7777:8888"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@[1111:2222:3333:4444:5555:6666:7777:888G]"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@[abcz::c11a:3a9c:ef10:e795]"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"Abc.example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"A@b@c@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"a\"b(c)d, e:f; g<h>i[j\\k]l@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"just\"not\"right@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"this is\"not\\allowed@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"this\\ still\"not\\allowed@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"1234567890123456789012345678901234567890123456789012345678901234+x@example.com"); });

			IMFAddress address;
			Assert::IsFalse(IMFAddress::TryParse(L"", address));
			Assert::IsFalse(IMFAddress::TryParse(L"example.com", address));
			Assert::IsFalse(IMFAddress::TryParse(L"test\\@example.com", address));
			Assert::IsFalse(IMFAddress::TryParse(L"test@[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:aaaa]", address));

			// Test valid addresses
			Assert::IsTrue(IMFAddress::TryParse(L"test@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@example_under_score.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@example-hyphen-test.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"Test@ExamPle.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"John.Smith@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test+test@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"\"test test\"@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghiklm@example.com", address)); // local part within limits
			Assert::IsTrue(IMFAddress::TryParse(L"a@a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v", address));
			Assert::IsTrue(IMFAddress::TryParse(L"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghiklm@abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghikl.abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghikl.abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghi", address));
			Assert::IsTrue(IMFAddress::TryParse(L"\"\a\"@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"用户@例子.广告", address));
			Assert::IsTrue(IMFAddress::TryParse(L"☞@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"екзампл@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"ñoñó1234@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"武@メール.グーグル", address));
			Assert::IsTrue(IMFAddress::TryParse(L"Pelé@example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"δοκιμή@παράδειγμα.δοκιμή", address));
			Assert::IsTrue(IMFAddress::TryParse(L"我買@屋企.香港", address));
			Assert::IsTrue(IMFAddress::TryParse(L"二ノ宮@黒川.日本", address));
			Assert::IsTrue(IMFAddress::TryParse(L"медведь@с-балалайкой.рф", address));
			Assert::IsTrue(IMFAddress::TryParse(L"संपर्क@डाटामेल.भारत", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@[255.255.255.255]", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@[192.25.168.1]", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@[1111:2222:3333:4444:5555:6666:7777:8888]", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@[fe80::c11a:3a9c:ef10:e795]", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@[0000:0000:0000:0000:0000:ffff:192.168.100.228]", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test@[f0a0:f0a0:f0a0:f0a0:f0a0:ffff:c0a8:64e4]", address));
			Assert::IsTrue(IMFAddress::TryParse(L"test/test@test.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"admin@mailserver1", address));
			Assert::IsTrue(IMFAddress::TryParse(L"admin@mailserver1.", address));
			Assert::IsTrue(IMFAddress::TryParse(L"example@s.exampl", address));
			Assert::IsTrue(IMFAddress::TryParse(L"\" \"@example.org", address));
			Assert::IsTrue(IMFAddress::TryParse(L"\"john..doe\"@example.org", address));
			Assert::IsTrue(IMFAddress::TryParse(L"mailhost!username@example.org", address));
			Assert::IsTrue(IMFAddress::TryParse(L"\"very.(), :; <>[]\\\".VERY.\\\"very@\\ \\\"very\\\".unusual\"@strange.example.com", address));
			Assert::IsTrue(IMFAddress::TryParse(L"user%example.com@example.org", address));
			Assert::IsTrue(IMFAddress::TryParse(L"user-@example.org", address));
			Assert::IsTrue(IMFAddress::TryParse(L"postmaster@[IPv6:2001:0db8:85a3:0000:0000:8a2e:0370:7334]", address));
		}

		TEST_METHOD(Constexpr)
		{
			static_assert(CheckIMFAddressConstexpr() == true);
		}
	};
}