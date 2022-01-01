// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\BTHEndpoint.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	TEST_CLASS(BTHEndpointTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Default construction
			BTHEndpoint bth1;
			Assert::AreEqual(true, bth1.GetProtocol() == BTHEndpoint::Protocol::Unspecified);
			Assert::AreEqual(true, bth1.GetBTHAddress() == BTHAddress::AnyBTH());
			Assert::AreEqual(true, bth1.GetPort() == 0);
			Assert::AreEqual(true, bth1.GetServiceClassID() == BTHEndpoint::GetNullServiceClassID());
			Assert::AreEqual(true, bth1.GetRelayHop() == 0);
			Assert::AreEqual(true, bth1.GetRelayPort() == 0);

			// Construction
			BTHEndpoint bth2(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 4,
							 BTHEndpoint::GetNullServiceClassID(), 1, 1);
			Assert::AreEqual(true, bth2.GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, bth2.GetBTHAddress() == BTHAddress(L"(92:5F:D3:5B:93:B2)"));
			Assert::AreEqual(true, bth2.GetPort() == 4);
			Assert::AreEqual(true, bth2.GetServiceClassID() == BTHEndpoint::GetNullServiceClassID());
			Assert::AreEqual(true, bth2.GetRelayHop() == 1);
			Assert::AreEqual(true, bth2.GetRelayPort() == 1);

			BTHEndpoint bth2a(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 0,
							 BTHEndpoint::GetQuantumGateServiceClassID(), 1, 1);
			Assert::AreEqual(true, bth2a.GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, bth2a.GetBTHAddress() == BTHAddress(L"(92:5F:D3:5B:93:B2)"));
			Assert::AreEqual(true, bth2a.GetPort() == 0);
			Assert::AreEqual(true, bth2a.GetServiceClassID() == BTHEndpoint::GetQuantumGateServiceClassID());
			Assert::AreEqual(true, bth2a.GetRelayHop() == 1);
			Assert::AreEqual(true, bth2a.GetRelayPort() == 1);

			// Copy construction
			BTHEndpoint bth3(bth2);
			Assert::AreEqual(true, bth3.GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, bth3.GetBTHAddress() == BTHAddress(L"(92:5F:D3:5B:93:B2)"));
			Assert::AreEqual(true, bth3.GetPort() == 4);
			Assert::AreEqual(true, bth3.GetServiceClassID() == BTHEndpoint::GetNullServiceClassID());
			Assert::AreEqual(true, bth3.GetRelayHop() == 1);
			Assert::AreEqual(true, bth3.GetRelayPort() == 1);

			// Equal and not equal
			{
				Assert::AreEqual(true, bth2 == bth3);
				Assert::AreEqual(false, bth2 != bth3);
				Assert::AreEqual(true, bth1 != bth2);

				BTHEndpoint bth2a(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 0,
								  BTHEndpoint::GetQuantumGateServiceClassID(), 1, 1);
				Assert::AreEqual(true, bth2 != bth2a);
				Assert::AreEqual(false, bth2 == bth2a);

				BTHEndpoint bth2b(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B5)"), 4,
								  BTHEndpoint::GetNullServiceClassID(), 1, 1);
				Assert::AreEqual(true, bth2 != bth2b);
				Assert::AreEqual(false, bth2 == bth2b);

				BTHEndpoint bth2c(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 5,
								  BTHEndpoint::GetNullServiceClassID(), 1, 1);
				Assert::AreEqual(true, bth2 != bth2c);
				Assert::AreEqual(false, bth2 == bth2c);

				BTHEndpoint bth2d(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 4,
								  BTHEndpoint::GetNullServiceClassID(), 2, 1);
				Assert::AreEqual(true, bth2 != bth2d);
				Assert::AreEqual(false, bth2 == bth2d);

				BTHEndpoint bth2e(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 4,
								  BTHEndpoint::GetNullServiceClassID(), 1, 2);
				Assert::AreEqual(true, bth2d != bth2e);
				Assert::AreEqual(false, bth2d == bth2e);
			}

			// Move construction
			BTHEndpoint bth4(std::move(bth2));
			Assert::AreEqual(true, bth3 == bth4);

			// Copy assignment
			bth1 = bth3;
			Assert::AreEqual(true, bth3 == bth1);

			BTHEndpoint bth5(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9);
			Assert::AreEqual(true, bth5.GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, bth5.GetBTHAddress() == BTHAddress(L"(92:5F:D3:5B:93:B2)"));
			Assert::AreEqual(true, bth5.GetPort() == 9);
			Assert::AreEqual(true, bth5.GetServiceClassID() == BTHEndpoint::GetNullServiceClassID());
			Assert::AreEqual(true, bth5.GetRelayHop() == 0);
			Assert::AreEqual(true, bth5.GetRelayPort() == 0);

			// Move assignment
			bth1 = std::move(bth5);
			Assert::AreEqual(false, bth3 == bth1);
			Assert::AreEqual(true, bth1.GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, bth1.GetBTHAddress() == BTHAddress(L"(92:5F:D3:5B:93:B2)"));
			Assert::AreEqual(true, bth1.GetPort() == 9);
			Assert::AreEqual(true, bth1.GetServiceClassID() == BTHEndpoint::GetNullServiceClassID());
			Assert::AreEqual(true, bth1.GetRelayHop() == 0);
			Assert::AreEqual(true, bth1.GetRelayPort() == 0);

			// GetString
			Assert::AreEqual(true, bth1.GetString() == L"RFCOMM:(92:5F:D3:5B:93:B2):9");
		}

		TEST_METHOD(Input)
		{
			// Test invalid addresses
			Assert::ExpectException<std::invalid_argument>([] { BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L""), 9); });
			Assert::ExpectException<std::invalid_argument>([] { BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"abcd"), 9); });
			Assert::ExpectException<std::invalid_argument>([] { BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5Z:D3:5B:93:B2)"), 9); });
			Assert::ExpectException<std::invalid_argument>([] { BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9, BTHEndpoint::GetQuantumGateServiceClassID()); });
			Assert::ExpectException<std::invalid_argument>([] { BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9, BTHEndpoint::GetQuantumGateServiceClassID(), 1000, 2); });

			// Test invalid protocol
			Assert::ExpectException<std::invalid_argument>([] { BTHEndpoint(BTHEndpoint::Protocol::Unspecified, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9); });
			Assert::ExpectException<std::invalid_argument>([] { BTHEndpoint(static_cast<BTHEndpoint::Protocol>(200), BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9); });

			// Test valid addresses
			try
			{
				BTHEndpoint bth1(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(00:00:00:00:00:00)"), 9);
				BTHEndpoint bth2(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 0);
				BTHEndpoint bth3(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 0, BTHEndpoint::GetQuantumGateServiceClassID());
				BTHEndpoint bth4(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 0, BTHEndpoint::GetQuantumGateServiceClassID(), 1000, 1);
				BTHEndpoint bth5(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9, BTHEndpoint::GetNullServiceClassID());
				BTHEndpoint bth6(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9, BTHEndpoint::GetNullServiceClassID(), 1000, 1);
			}
			catch (...)
			{
				Assert::Fail(L"Exception thrown while creating BTHEndpoints");
			}
		}

		TEST_METHOD(Constexpr)
		{
			// Default construction
			constexpr BTHEndpoint bth_ep0;
			static_assert(bth_ep0.GetProtocol() == BTHEndpoint::Protocol::Unspecified, "Should be equal");
			static_assert(bth_ep0.GetBTHAddress() == BTHAddress::AnyBTH(), "Should be equal");
			static_assert(bth_ep0.GetPort() == 0, "Should be equal");
			static_assert(BTHEndpoint::AreGUIDsEqual(bth_ep0.GetServiceClassID(), BTHEndpoint::GetNullServiceClassID()), "Should be equal");
			static_assert(bth_ep0.GetRelayPort() == 0, "Should be equal");
			static_assert(bth_ep0.GetRelayHop() == 0, "Should be equal");

			// Construction
			constexpr auto bth = BinaryBTHAddress(BinaryBTHAddress::Family::BTH, 0x925FD35B93B2);
			constexpr BTHEndpoint bth_ep1(BTHEndpoint::Protocol::RFCOMM, BTHAddress(bth), 9);
			constexpr BTHAddress btha = bth_ep1.GetBTHAddress();
			constexpr auto protocol = bth_ep1.GetProtocol();
			constexpr auto port = bth_ep1.GetPort();
			constexpr auto rport = bth_ep1.GetRelayPort();
			constexpr auto rhop = bth_ep1.GetRelayHop();

			static_assert(protocol == BTHEndpoint::Protocol::RFCOMM, "Should be equal");
			static_assert(port == 9, "Should be equal");
			static_assert(rport == 0, "Should be equal");
			static_assert(rhop == 0, "Should be equal");

			Assert::AreEqual(true, bth == btha.GetBinary());
			Assert::AreEqual(true, port == 9);
			Assert::AreEqual(true, rport == 0);
			Assert::AreEqual(true, rhop == 0);

			constexpr BTHEndpoint bth_ep2(BTHEndpoint::Protocol::RFCOMM, btha, 0, BTHEndpoint::GetQuantumGateServiceClassID(), 3000, 3);

			constexpr BTHEndpoint bth_ep4(std::move(bth_ep2));
			static_assert(bth_ep4.GetProtocol() == BTHEndpoint::Protocol::RFCOMM, "Should be equal");
			static_assert(bth_ep4.GetBTHAddress() == btha, "Should be equal");
			static_assert(bth_ep4.GetPort() == 0, "Should be equal");
			static_assert(BTHEndpoint::AreGUIDsEqual(bth_ep4.GetServiceClassID(), BTHEndpoint::GetQuantumGateServiceClassID()), "Should be equal");
			static_assert(bth_ep4.GetRelayPort() == 3000, "Should be equal");
			static_assert(bth_ep4.GetRelayHop() == 3, "Should be equal");
			Assert::AreEqual(true, bth_ep4.GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, bth_ep4.GetBTHAddress() == btha);
			Assert::AreEqual(true, bth_ep4.GetServiceClassID() == BTHEndpoint::GetQuantumGateServiceClassID());
			Assert::AreEqual(true, bth_ep4.GetPort() == 0);
			Assert::AreEqual(true, bth_ep4.GetRelayPort() == 3000);
			Assert::AreEqual(true, bth_ep4.GetRelayHop() == 3);

			constexpr BTHEndpoint bth_ep5 = std::move(bth_ep1);
			static_assert(bth_ep5.GetProtocol() == BTHEndpoint::Protocol::RFCOMM, "Should be equal");
			static_assert(bth_ep5.GetBTHAddress() == btha, "Should be equal");
			static_assert(bth_ep5.GetPort() == 9, "Should be equal");
			static_assert(BTHEndpoint::AreGUIDsEqual(bth_ep5.GetServiceClassID(), BTHEndpoint::GetNullServiceClassID()), "Should be equal");
			static_assert(bth_ep5.GetRelayPort() == 0, "Should be equal");
			static_assert(bth_ep5.GetRelayHop() == 0, "Should be equal");
			Assert::AreEqual(true, bth_ep5.GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, bth_ep5.GetBTHAddress() == btha);
			Assert::AreEqual(true, bth_ep5.GetPort() == 9);
			Assert::AreEqual(true, bth_ep5.GetServiceClassID() == BTHEndpoint::GetNullServiceClassID());
			Assert::AreEqual(true, bth_ep5.GetRelayPort() == 0);
			Assert::AreEqual(true, bth_ep5.GetRelayHop() == 0);

			constexpr auto bth_ep6 = bth_ep5;
			Assert::AreEqual(true, bth_ep6.GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, bth_ep6.GetBTHAddress() == btha);
			Assert::AreEqual(true, bth_ep6.GetPort() == 9);
			Assert::AreEqual(true, bth_ep6.GetRelayPort() == 0);
			Assert::AreEqual(true, bth_ep6.GetRelayHop() == 0);

			constexpr BTHEndpoint bth_ep7(BTHEndpoint::Protocol::RFCOMM, BTHAddress::AnyBTH(), 0);
			static_assert(bth_ep7.GetProtocol() == BTHEndpoint::Protocol::RFCOMM, "Should be equal");
			static_assert(bth_ep7.GetBTHAddress() == BTHAddress::AnyBTH(), "Should be equal");
			static_assert(bth_ep7.GetPort() == 0, "Should be equal");
			static_assert(BTHEndpoint::AreGUIDsEqual(bth_ep7.GetServiceClassID(), BTHEndpoint::GetNullServiceClassID()), "Should be equal");
			static_assert(bth_ep7.GetRelayPort() == 0, "Should be equal");
			static_assert(bth_ep7.GetRelayHop() == 0, "Should be equal");

			constexpr BTHEndpoint bth_ep8(BTHEndpoint::Protocol::RFCOMM, BTHAddress::AnyBTH(), 4, BTHEndpoint::GetNullServiceClassID());

			constexpr BTHEndpoint bth_ep9(BTHEndpoint::Protocol::RFCOMM, btha, 0, BTHEndpoint::GetQuantumGateServiceClassID());
			static_assert(bth_ep9.GetProtocol() == BTHEndpoint::Protocol::RFCOMM, "Should be equal");
			static_assert(bth_ep9.GetBTHAddress() == btha, "Should be equal");
			static_assert(bth_ep9.GetPort() == 0, "Should be equal");
			static_assert(BTHEndpoint::AreGUIDsEqual(bth_ep9.GetServiceClassID(), BTHEndpoint::GetQuantumGateServiceClassID()), "Should be equal");
			static_assert(bth_ep9.GetRelayPort() == 0, "Should be equal");
			static_assert(bth_ep9.GetRelayHop() == 0, "Should be equal");

			constexpr BTHEndpoint bth_ep10(BTHEndpoint::Protocol::RFCOMM, BTHAddress::AnyBTH(), 0, BTHEndpoint::GetQuantumGateServiceClassID(), 2000, 2);

			constexpr BTHEndpoint bth_ep11(BTHEndpoint::Protocol::RFCOMM, btha, 9, BTHEndpoint::GetNullServiceClassID(), 2000, 2);
			static_assert(bth_ep11.GetProtocol() == BTHEndpoint::Protocol::RFCOMM, "Should be equal");
			static_assert(bth_ep11.GetBTHAddress() == btha, "Should be equal");
			static_assert(bth_ep11.GetPort() == 9, "Should be equal");
			static_assert(BTHEndpoint::AreGUIDsEqual(bth_ep11.GetServiceClassID(), BTHEndpoint::GetNullServiceClassID()), "Should be equal");
			static_assert(bth_ep11.GetRelayPort() == 2000, "Should be equal");
			static_assert(bth_ep11.GetRelayHop() == 2, "Should be equal");
		}
	};
}