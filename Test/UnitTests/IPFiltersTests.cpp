// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"
#include "Core\Access\IPFilters.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Core::Access;

namespace UnitTests
{
	TEST_CLASS(IPFiltersTests)
	{
	public:
		TEST_METHOD(AddRemove)
		{
			IPFilters ipfilters;

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"192.168.abc.1", L"255.255.255.0",
												 IPFilterType::Blocked) == ResultCode::AddressInvalid);

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"", L"255.255.255.0",
												 IPFilterType::Blocked) == ResultCode::AddressInvalid);

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"abcz::c11a:3a9c:ef10:e795", L"255.255.255.0",
												 IPFilterType::Blocked) == ResultCode::AddressInvalid);

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"c11a:3a9c:ef10:e795::", L"/129",
												 IPFilterType::Blocked) == ResultCode::AddressMaskInvalid);

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"192.168.0.1", L"",
												 IPFilterType::Blocked) == ResultCode::AddressMaskInvalid);

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"192.168.0.1", L"255.255.255.abc",
												 IPFilterType::Blocked) == ResultCode::AddressMaskInvalid);

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"192.168.0.1", L"ffff::rgvb:ffff:ffff:ffff",
												 IPFilterType::Blocked) == ResultCode::AddressMaskInvalid);

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"192.168.0.1", L"/1s",
												 IPFilterType::Blocked) == ResultCode::AddressMaskInvalid);

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"192.168.0.1/33",
												 IPFilterType::Blocked) == ResultCode::AddressMaskInvalid);

			// No entries at this point
			Assert::AreEqual(static_cast<size_t>(0), ipfilters.GetFilters().GetValue().size());

			auto result = ipfilters.AddFilter(L"192.168.0.1/24", IPFilterType::Blocked);
			Assert::AreEqual(true, result.Succeeded());

			Assert::AreEqual(true, ipfilters.HasFilter(*result, IPFilterType::Blocked));

			// Adding again should fail
			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"192.168.0.1", L"255.255.255.0", IPFilterType::Blocked).Failed());

			auto result2 = ipfilters.AddFilter(L"192.168.0.1", L"255.255.255.0", IPFilterType::Allowed);
			Assert::AreEqual(true, result2.Succeeded());

			auto result3 = ipfilters.AddFilter(L"fe80::c11a:3a9c:ef10:e795",
											   L"ffff::ffff:ffff:ffff:ffff", IPFilterType::Blocked);
			Assert::AreEqual(true, result3.Succeeded());

			Assert::AreEqual(static_cast<size_t>(3), ipfilters.GetFilters().GetValue().size());

			Assert::AreEqual(true,
							 ipfilters.RemoveFilter(*result, IPFilterType::Blocked).Succeeded());

			// Removing again should fail
			Assert::AreEqual(true,
							 ipfilters.RemoveFilter(*result, IPFilterType::Blocked).Failed());

			Assert::AreEqual(false, ipfilters.HasFilter(*result, IPFilterType::Blocked));

			Assert::AreEqual(true,
							 ipfilters.RemoveFilter(*result2, IPFilterType::Allowed).Succeeded());

			// Removing again should fail
			Assert::AreEqual(true,
							 ipfilters.RemoveFilter(*result2, IPFilterType::Allowed).Failed());

			Assert::AreEqual(false, ipfilters.HasFilter(*result2, IPFilterType::Allowed));

			Assert::AreEqual(true,
							 ipfilters.RemoveFilter(*result3, IPFilterType::Blocked).Succeeded());

			Assert::AreEqual(false, ipfilters.HasFilter(*result3, IPFilterType::Blocked));

			// Removing again should fail
			Assert::AreEqual(true,
							 ipfilters.RemoveFilter(*result3, IPFilterType::Blocked).Failed());

			Assert::AreEqual(static_cast<size_t>(0), ipfilters.GetFilters().GetValue().size());
		}

		TEST_METHOD(General)
		{
			IPFilters ipfilters;

			auto result = ipfilters.AddFilter(L"192.168.0.1", L"255.255.255.0", IPFilterType::Blocked);
			Assert::AreEqual(true, result.Succeeded());

			Assert::AreEqual(true, ipfilters.IsAllowed(L"") == ResultCode::AddressInvalid);
			Assert::AreEqual(true, ipfilters.IsAllowed(L"192.abc.0.1") == ResultCode::AddressInvalid);

			Assert::AreEqual(false, ipfilters.IsAllowed(L"192.168.0.1").GetValue());
			Assert::AreEqual(false, ipfilters.IsAllowed(L"192.168.0.10").GetValue());
			Assert::AreEqual(false, ipfilters.IsAllowed(L"192.168.0.200").GetValue());

			Assert::AreEqual(true, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef10:e795").GetValue());
			Assert::AreEqual(true, ipfilters.IsAllowed(L"192.168.1.2").GetValue());

			auto result2 = ipfilters.AddFilter(L"192.168.1.2", L"255.255.255.255", IPFilterType::Blocked);
			Assert::AreEqual(true, result2.Succeeded());

			Assert::AreEqual(false, ipfilters.IsAllowed(L"192.168.1.2").GetValue());
			Assert::AreEqual(true, ipfilters.IsAllowed(L"192.168.1.1").GetValue());
			Assert::AreEqual(true, ipfilters.IsAllowed(L"192.168.1.100").GetValue());
			Assert::AreEqual(true, ipfilters.IsAllowed(L"192.200.1.100").GetValue());

			Assert::AreEqual(true,
							 ipfilters.RemoveFilter(*result2, IPFilterType::Blocked).Succeeded());

			Assert::AreEqual(true, ipfilters.IsAllowed(L"192.168.1.2").GetValue());

			auto result3 = ipfilters.AddFilter(L"fe80::c11a:3a9c:ef10:e795",
											   L"ffff::ffff:ffff:ffff:ffff", IPFilterType::Blocked);
			Assert::AreEqual(true, result3.Succeeded());

			Assert::AreEqual(false, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef10:e795").GetValue());

			Assert::AreEqual(true,
							 ipfilters.RemoveFilter(*result3, IPFilterType::Blocked).Succeeded());

			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"fe80::c11a:3a9c:ef11:e795",
												 L"ffff::ffff:ffff:ffdc:ffff", IPFilterType::Blocked).Succeeded());

			Assert::AreEqual(true, ipfilters.IsAllowed(L"fe80::c11a:3a9c:eeee:e795").GetValue());
			Assert::AreEqual(true, ipfilters.IsAllowed(L"fe80::c11a:3a9c:eeef:e795").GetValue());
			Assert::AreEqual(true, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef0f:e795").GetValue());
			Assert::AreEqual(true, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef09:e795").GetValue());

			Assert::AreEqual(false, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef10:e795").GetValue());

			Assert::AreEqual(false, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef11:e795").GetValue());
			Assert::AreEqual(false, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef12:e795").GetValue());
			Assert::AreEqual(false, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef20:e795").GetValue());
			Assert::AreEqual(false, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef33:e795").GetValue());
			Assert::AreEqual(true, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef34:e795").GetValue());

			ipfilters.Clear();

			Assert::AreEqual(static_cast<size_t>(0), ipfilters.GetFilters().GetValue().size());
		}

		TEST_METHOD(AllowOverride)
		{
			IPFilters ipfilters;

			// Block IPv4 range
			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"192.168.0.1", L"255.255.255.0", IPFilterType::Blocked).Succeeded());

			// This address should be blocked now because of above filter
			Assert::AreEqual(false, ipfilters.IsAllowed(L"192.168.0.100").GetValue());

			// Specifically allow an address in the blocked range
			Assert::AreEqual(true,
							 ipfilters.AddFilter(L"192.168.0.100", L"255.255.255.255", IPFilterType::Allowed).Succeeded());

			// This address should now be allowed
			Assert::AreEqual(true, ipfilters.IsAllowed(L"192.168.0.100").GetValue());

			// And now the same as above for IPv6
			Assert::AreEqual(true, ipfilters.AddFilter(L"fe80::c11a:3a9c:ef11:e795",
													   L"ffff::ffff:ffff:ffdc:ffff",
													   IPFilterType::Blocked).Succeeded());

			Assert::AreEqual(false, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef12:e795").GetValue());

			Assert::AreEqual(true, ipfilters.AddFilter(L"fe80::c11a:3a9c:ef12:e795",
													   L"ffff::ffff:ffff:ffff:ffff",
													   IPFilterType::Allowed).Succeeded());

			Assert::AreEqual(true, ipfilters.IsAllowed(L"fe80::c11a:3a9c:ef12:e795").GetValue());
		}
	};
}