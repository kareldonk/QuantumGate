// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Core\Access\IPSubnetLimits.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Core::Access;

namespace UnitTests
{
	TEST_CLASS(IPSubnetLimitsTests)
	{
	public:
		TEST_METHOD(AddRemoveLimits)
		{
			IPSubnetLimits limits;

			//Adding
			{
				// IPv4
				{
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 0, 0).Succeeded());
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, L"/8", 0).Succeeded());
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 16, 0).Succeeded());
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, L"/32", 0).Succeeded());

					// Duplicate should fail
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 32, 0).Failed());

					// CIDR leading bits too large
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 33, 0) == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, L"/34", 0) == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 50, 0) == ResultCode::InvalidArgument);

					// Bad CIDR
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, L"34", 0) == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, L"/1 2", 0) == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, L"/aw12", 0) == ResultCode::InvalidArgument);
				}

				// IPv6
				{
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 0, 0).Succeeded());
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 8, 0).Succeeded());
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, L"/16", 0).Succeeded());
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 32, 0).Succeeded());
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 48, 0).Succeeded());
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, L"/128", 0).Succeeded());

					// Duplicate should fail
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 32, 0).Failed());

					// CIDR leading bits too large
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 129, 0) == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, L"/134", 0) == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 200, 0) == ResultCode::InvalidArgument);

					// Bad CIDR
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, L"3 4", 0) == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, L"/1 2", 0) == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, L"/ 12", 0) == ResultCode::InvalidArgument);
				}
			}

			// Removing
			{
				// IPv4
				{
					Assert::AreEqual(true, limits.HasLimit(IPAddress::Family::IPv4, 0));
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv4, 0).Succeeded());
					Assert::AreEqual(false, limits.HasLimit(IPAddress::Family::IPv4, 0));

					Assert::AreEqual(true, limits.HasLimit(IPAddress::Family::IPv4, 16));
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv4, L"/16").Succeeded());
					Assert::AreEqual(false, limits.HasLimit(IPAddress::Family::IPv4, 16));

					// Removing again should fail
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv4, 0).Failed());
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv4, 16).Failed());

					// Bad CIDR
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv4, L"34") == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv4, L"/1 2") == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv4, L"/aw12") == ResultCode::InvalidArgument);
				}

				// IPv6
				{
					Assert::AreEqual(true, limits.HasLimit(IPAddress::Family::IPv6, 0));
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv6, 0).Succeeded());
					Assert::AreEqual(false, limits.HasLimit(IPAddress::Family::IPv6, 0));

					Assert::AreEqual(true, limits.HasLimit(IPAddress::Family::IPv6, 16));
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv6, 16).Succeeded());
					Assert::AreEqual(false, limits.HasLimit(IPAddress::Family::IPv6, 16));

					// Removing again should fail
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv6, 0).Failed());
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv6, 16).Failed());

					// Bad CIDR
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv6, L"3 4") == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv6, L"/1 2") == ResultCode::InvalidArgument);
					Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv6, L"/ 12") == ResultCode::InvalidArgument);
				}
			}
		}

		TEST_METHOD(AddRemoveConnections)
		{
			IPSubnetLimits limits;
			Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 8, 6).Succeeded());
			Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 16, 2).Succeeded());
			Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 16, 6).Succeeded());
			Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 64, 2).Succeeded());

			// IPv4
			{
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"192.168.10.10")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"192.168.10.20")));
				// Subnet /16 full (192.168.*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"192.168.10.30")));

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"192.169.10.10")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"192.169.10.20")));
				// Subnet /16 full (192.169.*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"192.169.10.30")));

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"193.169.10.10")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"193.169.10.20")));
				// Subnet /16 full (193.169.*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"193.169.10.30")));

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"192.159.10.10")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"192.159.10.20")));
				// Subnet /8 full (192.*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"192.159.10.10")));

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.119.10.10")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.129.10.20")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.139.10.30")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.149.10.40")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.159.10.50")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.169.10.60")));
				// Subnet /8 full (194.*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"194.179.10.70")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.169.10.60")));
				// Subnet /8 not full anymore (194.*)
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"194.179.10.70")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"193.169.10.20")));
				// Subnet /16 not full anymore (193.169.*)
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"193.169.10.30")));

				// Does not exist in any subnet
				Assert::AreEqual(false, limits.RemoveConnection(IPAddress(L"200.169.10.20")));
				// Does not exist in subnet /16
				Assert::AreEqual(false, limits.RemoveConnection(IPAddress(L"194.200.10.30")));

				//..

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"192.168.10.10")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"192.168.10.20")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"192.169.10.10")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"192.169.10.20")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"193.169.10.10")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"192.159.10.10")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"192.159.10.20")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.119.10.10")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.129.10.20")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.139.10.30")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.149.10.40")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.159.10.50")));
			}

			// IPv6
			{
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e795::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e794::")));
				// Subnet /64 full (fe80:c11a:3a9c:ef10:*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e793::")));

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3b9c:ef11:e794::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3b9c:ef11:e795::")));
				// Subnet /64 full (fe80:c11a:3b9c:ef11:*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"fe80:c11a:3b9c:ef11:e796::")));

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe81:c11a:3b9c:ef11:e794::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe81:c11a:3b9c:ef11:e795::")));
				// Subnet /64 full (fe81:c11a:3b9c:ef11*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"fe81:c11a:3b9c:ef11:e796::")));

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c12a:3b9c:ef11:e785::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c12a:3b9c:ef11:e786::")));
				// Subnet /16 full (fe80:*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"fe80:c12a:3b9c:ef11:e787::")));

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe85:c10a:3b9c:ef11:e787::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe85:c10a:3b9c:ef12:e788::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe85:c10a:3b9c:ef13:e789::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe85:c10a:3b9c:ef14:e790::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe85:c10a:3b9c:ef15:e791::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe85:c10a:3b9c:ef16:e792::")));
				// Subnet /16 full (fe85:*)
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"fe85:c10a:3b9c:ef11:e793::")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe85:c10a:3b9c:ef11:e787::")));
				// Subnet /16 not full anymore (fe85:*)
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"fe85:c10a:3b9c:ef11:e793::")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe81:c11a:3b9c:ef11:e795::")));
				// Subnet /64 not full anymore (fe81:c11a:3b9c:ef11:*)
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"fe81:c11a:3b9c:ef11:e796::")));

				// Does not exist in any subnet
				Assert::AreEqual(false, limits.RemoveConnection(IPAddress(L"fa81:c11a:4b9c:ef11:e796::")));
				// Does not exist in subnet /64
				Assert::AreEqual(false, limits.RemoveConnection(IPAddress(L"fe81:c11a:3c9c:ef11:e795::")));

				//..

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e795::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e794::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3b9c:ef11:e794::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3b9c:ef11:e795::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe81:c11a:3b9c:ef11:e794::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c12a:3b9c:ef11:e785::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c12a:3b9c:ef11:e786::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe85:c10a:3b9c:ef12:e788::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe85:c10a:3b9c:ef13:e789::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe85:c10a:3b9c:ef14:e790::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe85:c10a:3b9c:ef15:e791::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe85:c10a:3b9c:ef16:e792::")));
			}
		}

		TEST_METHOD(AddLimitAfterExistingConnections)
		{
			IPSubnetLimits limits;

			// IPv4
			{
				Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 0, 10).Succeeded());

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.10")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.20")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.50")));

				Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 24, 2).Succeeded());

				// We should now have a /24 limit with 3 connections too much
				Assert::AreEqual(true, limits.HasConnectionOverflow(IPAddress(L"194.120.10.50")));

				// These should fail
				Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"194.120.10.60")));
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"194.120.10.60")));
				
				// Remove connections that are too much
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.20")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.30")));

				Assert::AreEqual(false, limits.HasConnectionOverflow(IPAddress(L"194.120.10.50")));

				// Remove one extra
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.50")));

				// These should now succeed
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"194.120.10.60")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.60")));

				// These should fail
				Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"194.120.10.70")));
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"194.120.10.70")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.10")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.60")));
			}

			// IPv6
			{
				Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 0, 10).Succeeded());

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e795::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e796::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e799::")));

				Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 64, 2).Succeeded());

				// We should now have a /64 limit with 3 connections too much
				Assert::AreEqual(true, limits.HasConnectionOverflow(IPAddress(L"fe80:c11a:3a9c:ef10:e799::")));

				// These should fail
				Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e800::")));
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e800::")));

				// Remove connections that are too much
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e799::")));

				Assert::AreEqual(false, limits.HasConnectionOverflow(IPAddress(L"fe80:c11a:3a9c:ef10:e799::")));

				// Remove one extra
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e796::")));

				// These should now succeed
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e800::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e800::")));

				// These should fail
				Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e801::")));
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e801::")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e795::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e800::")));
			}
		}

		TEST_METHOD(RemoveLimitAfterExistingConnections)
		{
			IPSubnetLimits limits;

			// IPv4
			{
				Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 0, 3).Succeeded());

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.10")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.20")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.20")));

				Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 24, 1).Succeeded());

				// We should now have a /24 limit with 3 connections too much
				Assert::AreEqual(true, limits.HasConnectionOverflow(IPAddress(L"194.120.10.30")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.10")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.20")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.20")));

				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.30")));
				
				Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"194.120.10.30")));

				Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv4, 24).Succeeded());

				// Can accept two more now
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"194.120.10.30")));

				// The /0 limit is now full
				Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"194.120.10.30")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.30")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"194.120.10.30")));

				// No more connections
				Assert::AreEqual(false, limits.RemoveConnection(IPAddress(L"194.120.10.30")));
			}

			// IPv6
			{
				Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 0, 3).Succeeded());

				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e795::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e796::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e796::")));

				Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv6, 24, 1).Succeeded());

				// We should now have a /24 limit with 3 connections too much
				Assert::AreEqual(true, limits.HasConnectionOverflow(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e795::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e796::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e796::")));

				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));

				Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));

				Assert::AreEqual(true, limits.RemoveLimit(IPAddress::Family::IPv6, 24).Succeeded());

				// Can accept two more now
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));

				// The /0 limit is now full
				Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(false, limits.AddConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));

				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));
				Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"fe80:c11a:3a9c:ef10:e797::")));

				// No more connections
				Assert::AreEqual(false, limits.RemoveConnection(IPAddress(L"194.120.10.30")));
			}
		}

		TEST_METHOD(CanAccept)
		{
			IPSubnetLimits limits;
			Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 0, 3).Succeeded());
			Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 8, 2).Succeeded());
			Assert::AreEqual(true, limits.AddLimit(IPAddress::Family::IPv4, 16, 1).Succeeded());

			Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"192.168.10.10")));
			Assert::AreEqual(true, limits.AddConnection(IPAddress(L"192.168.10.10")));

			// /16 full
			Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"192.168.10.20")));

			Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"192.167.10.20")));

			Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"193.168.10.20")));
			Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"193.162.10.20")));

			Assert::AreEqual(true, limits.AddConnection(IPAddress(L"192.165.10.10")));
			// /8 full
			Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"192.167.10.20")));

			Assert::AreEqual(true, limits.AddConnection(IPAddress(L"193.165.10.10")));
			// /0 full
			Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"194.167.10.20")));

			Assert::AreEqual(true, limits.RemoveConnection(IPAddress(L"192.165.10.10")));
			// /8 not full
			Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"192.167.10.20")));
			// /0 not full
			Assert::AreEqual(true, limits.CanAcceptConnection(IPAddress(L"194.167.10.20")));
			// /16 still full
			Assert::AreEqual(false, limits.CanAcceptConnection(IPAddress(L"192.168.10.20")));
		}
	};
}