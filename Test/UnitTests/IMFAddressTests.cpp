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
			Assert::AreEqual(true, addr1.GetString() == L"");
			Assert::AreEqual(true, addr1.GetFamily() == IMFAddress::Family::Unspecified);

			// Construction
			IMFAddress addr2(L"test@example.com");
			Assert::AreEqual(true, addr2.GetString() == L"test@example.com");
			Assert::AreEqual(true, addr2.GetFamily() == IMFAddress::Family::IMF);

			// Copy construction
			IMFAddress addr3(addr2);
			Assert::AreEqual(true, addr3.GetString() == L"test@example.com");
			Assert::AreEqual(true, addr3.GetFamily() == IMFAddress::Family::IMF);

			// Equal and not equal
			Assert::AreEqual(true, addr2 == addr3);
			Assert::AreEqual(false, addr2 != addr3);
			Assert::AreEqual(true, addr1 != addr2);

			// Move construction
			IMFAddress addr4(std::move(addr2));
			Assert::AreEqual(true, addr3 == addr4);

			// Copy assignment
			addr1 = addr3;
			Assert::AreEqual(true, addr3 == addr1);

			// Move assignment
			const auto addr5 = std::move(addr3);
			Assert::AreEqual(true, addr5 == addr1);

			// GetBinary
			Assert::AreEqual(true, addr1.GetBinary().AddressFamily == BinaryIMFAddress::Family::IMF);
			Assert::AreEqual(true, StringView(addr1.GetBinary().GetChars()) == StringView(L"test@example.com"));
			Assert::AreEqual(true, addr1.GetBinary().GetSize() == 17);

			// GetFamily
			Assert::AreEqual(true, addr1.GetFamily() == IMFAddress::Family::IMF);
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
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test:test@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@example:example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"te..st@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test\\@example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test @example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@-example.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@example-.com"); });
			Assert::ExpectException<std::invalid_argument>([] { IMFAddress(L"test@example..com"); });
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

			IMFAddress address;
			Assert::AreEqual(false, IMFAddress::TryParse(L"", address));
			Assert::AreEqual(false, IMFAddress::TryParse(L"example.com", address));
			Assert::AreEqual(false, IMFAddress::TryParse(L"test\\@example.com", address));
			Assert::AreEqual(false, IMFAddress::TryParse(L"test@[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:aaaa]", address));

			// Test valid addresses
			Assert::AreEqual(true, IMFAddress::TryParse(L"test@example.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"Test@ExamPle.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"John.Smith@example.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"test+test@example.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"\"test test\"@example.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghiklm@example.com", address)); // local part within limits
			Assert::AreEqual(true, IMFAddress::TryParse(L"a@a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghiklm@abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghikl.abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghikl.abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghi", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"\"\a\"@example.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"test@example.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"用户@例子.广告", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"☞@example.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"екзампл@example.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"ñoñó1234@example.com", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"武@メール.グーグル", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"test@[255.255.255.255]", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"test@[192.25.168.1]", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"test@[1111:2222:3333:4444:5555:6666:7777:8888]", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"test@[fe80::c11a:3a9c:ef10:e795]", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"test@[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"test@[0000:0000:0000:0000:0000:ffff:192.168.100.228]", address));
			Assert::AreEqual(true, IMFAddress::TryParse(L"test@[f0a0:f0a0:f0a0:f0a0:f0a0:ffff:c0a8:64e4]", address));
		}

		TEST_METHOD(Constexpr)
		{
			static_assert(CheckIMFAddressConstexpr() == true);
		}
	};
}