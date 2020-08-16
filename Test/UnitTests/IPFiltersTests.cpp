// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
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

			auto result3 = ipfilters.AddFilter(L"fe80:c11a:3a9c:ef10:e795::",
											   L"ffff:ffff:ffff:ffff:ffff::", IPFilterType::Blocked);
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

			Assert::AreEqual(true, ipfilters.GetAllowed(L"") == ResultCode::AddressInvalid);
			Assert::AreEqual(true, ipfilters.GetAllowed(L"192.abc.0.1") == ResultCode::AddressInvalid);

			{
				// Not allowed by default
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.168.0.1").GetValue());
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.168.0.10").GetValue());
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.168.0.200").GetValue());

				// Allow range
				auto result = ipfilters.AddFilter(L"192.168.0.1", L"255.255.255.0", IPFilterType::Allowed);
				Assert::AreEqual(true, result.Succeeded());

				// Should now be allowed
				Assert::AreEqual(true, ipfilters.GetAllowed(L"192.168.0.1").GetValue());
				Assert::AreEqual(true, ipfilters.GetAllowed(L"192.168.0.10").GetValue());
				Assert::AreEqual(true, ipfilters.GetAllowed(L"192.168.0.200").GetValue());

				// Not allowed
				Assert::AreEqual(false, ipfilters.GetAllowed(L"fe80::c11a:3a9c:ef10:e795").GetValue());
				
			}

			{
				// Not allowed by default
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.168.1.2").GetValue());

				// Allow range
				auto result2 = ipfilters.AddFilter(L"192.168.1.2", L"255.255.255.255", IPFilterType::Allowed);
				Assert::AreEqual(true, result2.Succeeded());

				// Should now be allowed
				Assert::AreEqual(true, ipfilters.GetAllowed(L"192.168.1.2").GetValue());

				// Not allowed by default
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.168.1.1").GetValue());
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.168.1.100").GetValue());
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.200.1.100").GetValue());

				Assert::AreEqual(true,
								 ipfilters.RemoveFilter(*result2, IPFilterType::Allowed).Succeeded());

				// Not allowed anymore after removal of filter range above
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.168.1.2").GetValue());
			}

			{
				// Not allowed by default
				Assert::AreEqual(false, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ef10:e795::").GetValue());

				// Allow range
				auto result3 = ipfilters.AddFilter(L"fe80:c11a:3a9c:ef10:e795::",
												   L"ffff:ffff:ffff:ffff:ffff::", IPFilterType::Allowed);
				Assert::AreEqual(true, result3.Succeeded());

				// Should now be allowed
				Assert::AreEqual(true, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ef10:e795::").GetValue());

				Assert::AreEqual(true,
								 ipfilters.RemoveFilter(*result3, IPFilterType::Allowed).Succeeded());

				// Not allowed anymore after removal of filter range above
				Assert::AreEqual(false, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ef10:e795::").GetValue());
			}

			{
				Assert::AreEqual(true,
								 ipfilters.AddFilter(L"fe80:c11a:3a9c:ef11:e795::",
													 L"ffff:ffff:ffff:ff00::", IPFilterType::Allowed).Succeeded());

				Assert::AreEqual(true, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ef80:e795::").GetValue());
				Assert::AreEqual(true, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ef81:e795::").GetValue());
				Assert::AreEqual(true, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ef91:e795::").GetValue());

				Assert::AreEqual(false, ipfilters.GetAllowed(L"fe80:c11a:3a9c:df11::").GetValue());
				Assert::AreEqual(false, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ff11::").GetValue());
			}

			// Remove all filters
			ipfilters.Clear();

			// Should be empty after clear
			Assert::AreEqual(static_cast<size_t>(0), ipfilters.GetFilters().GetValue().size());
		}

		TEST_METHOD(BlockOverride)
		{
			IPFilters ipfilters;

			{
				// Blocked by default
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.168.0.100").GetValue());

				// Allow IPv4 range
				Assert::AreEqual(true,
								 ipfilters.AddFilter(L"192.168.0.1", L"255.255.255.0", IPFilterType::Allowed).Succeeded());

				// This address should be allowed now because of above filter
				Assert::AreEqual(true, ipfilters.GetAllowed(L"192.168.0.100").GetValue());

				// Specifically block an address in the allowed range
				Assert::AreEqual(true,
								 ipfilters.AddFilter(L"192.168.0.100", L"255.255.255.255", IPFilterType::Blocked).Succeeded());

				// This address should now be blocked
				Assert::AreEqual(false, ipfilters.GetAllowed(L"192.168.0.100").GetValue());

				// These should be allowed
				Assert::AreEqual(true, ipfilters.GetAllowed(L"192.168.0.99").GetValue());
				Assert::AreEqual(true, ipfilters.GetAllowed(L"192.168.0.101").GetValue());
			}

			{
				// Blocked by default
				Assert::AreEqual(false, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ef11:e795::f000").GetValue());

				// Allow IPv6 range
				Assert::AreEqual(true, ipfilters.AddFilter(L"fe80:c11a:3a9c:ef11:e795::",
														   L"/80",
														   IPFilterType::Allowed).Succeeded());

				// This address should be allowed now because of above filter
				Assert::AreEqual(true, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ef11:e795::f000").GetValue());

				// Specifically block an address in the allowed range
				Assert::AreEqual(true, ipfilters.AddFilter(L"fe80:c11a:3a9c:ef11:e795::f000",
														   L"/128",
														   IPFilterType::Blocked).Succeeded());

				// This address should now be blocked
				Assert::AreEqual(false, ipfilters.GetAllowed(L"fe80:c11a:3a9c:ef11:e795::f000").GetValue());
			}
		}
	};
}