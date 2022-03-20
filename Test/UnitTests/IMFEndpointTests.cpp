// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\IMFEndpoint.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	constexpr bool CheckIMFEndpointConstexpr() noexcept
	{
		// Default construction
		IMFEndpoint imf_ep0;
		auto success = (imf_ep0.GetProtocol() == IMFEndpoint::Protocol::Unspecified);
		success &= (imf_ep0.GetIMFAddress() == IMFAddress());
		success &= (imf_ep0.GetPort() == 0);
		success &= (imf_ep0.GetRelayPort() == 0);
		success &= (imf_ep0.GetRelayHop() == 0);
		
		// Construction
		auto imf = BinaryIMFAddress(BinaryIMFAddress::Family::IMF, L"info@example.com");
		IMFEndpoint imf_ep1(IMFEndpoint::Protocol::IMF, IMFAddress(imf), 9);
		auto imfa = imf_ep1.GetIMFAddress();
		auto protocol = imf_ep1.GetProtocol();
		auto port = imf_ep1.GetPort();
		auto rport = imf_ep1.GetRelayPort();
		auto rhop = imf_ep1.GetRelayHop();

		success &= (protocol == IMFEndpoint::Protocol::IMF);
		success &= (imf == imfa.GetBinary());
		success &= (port == 9);
		success &= (rport == 0);
		success &= (rhop == 0);

		IMFEndpoint imf_ep2(IMFEndpoint::Protocol::IMF, imfa, 9, 3000, 3);
		success &= (imf_ep2.GetProtocol() == IMFEndpoint::Protocol::IMF);
		success &= (imf_ep2.GetIMFAddress() == imfa);
		success &= (imf_ep2.GetPort() == 9);
		success &= (imf_ep2.GetRelayPort() == 3000);
		success &= (imf_ep2.GetRelayHop() == 3);

		// Copy construction
		IMFEndpoint imf_ep3(imf_ep2);
		success &= (imf_ep3.GetProtocol() == IMFEndpoint::Protocol::IMF);
		success &= (imf_ep3.GetIMFAddress() == imfa);
		success &= (imf_ep3.GetPort() == 9);
		success &= (imf_ep3.GetRelayPort() == 3000);
		success &= (imf_ep3.GetRelayHop() == 3);

		// Move construction
		IMFEndpoint imf_ep4(std::move(imf_ep2));
		success &= (imf_ep4.GetProtocol() == IMFEndpoint::Protocol::IMF);
		success &= (imf_ep4.GetIMFAddress() == imfa);
		success &= (imf_ep4.GetPort() == 9);
		success &= (imf_ep4.GetRelayPort() == 3000);
		success &= (imf_ep4.GetRelayHop() == 3);

		// Move assignment
		IMFEndpoint imf_ep5 = std::move(imf_ep4);
		success &= (imf_ep5.GetProtocol() == IMFEndpoint::Protocol::IMF);
		success &= (imf_ep5.GetIMFAddress() == imfa);
		success &= (imf_ep5.GetPort() == 9);
		success &= (imf_ep5.GetRelayPort() == 3000);
		success &= (imf_ep5.GetRelayHop() == 3);

		// Copy assignment
		auto imf_ep6 = imf_ep5;
		success &= (imf_ep6.GetProtocol() == IMFEndpoint::Protocol::IMF);
		success &= (imf_ep6.GetIMFAddress() == imfa);
		success &= (imf_ep6.GetPort() == 9);
		success &= (imf_ep6.GetRelayPort() == 3000);
		success &= (imf_ep6.GetRelayHop() == 3);

		// Equal and not equal
		success &= (imf_ep6 == imf_ep5);
		success &= (imf_ep6 != imf_ep0);
		
		// Construction
		IMFEndpoint imf_ep7(IMFEndpoint::Protocol::IMF, IMFAddress(), 0);
		success &= (imf_ep7.GetProtocol() == IMFEndpoint::Protocol::IMF);
		success &= (imf_ep7.GetIMFAddress() == IMFAddress());
		success &= (imf_ep7.GetPort() == 0);
		success &= (imf_ep7.GetRelayPort() == 0);
		success &= (imf_ep7.GetRelayHop() == 0);

		IMFEndpoint imf_ep8(IMFEndpoint::Protocol::IMF, IMFAddress(), 4);

		IMFEndpoint imf_ep9(IMFEndpoint::Protocol::IMF, imfa, 0);
		success &= (imf_ep9.GetProtocol() == IMFEndpoint::Protocol::IMF);
		success &= (imf_ep9.GetIMFAddress() == imfa);
		success &= (imf_ep9.GetPort() == 0);
		success &= (imf_ep9.GetRelayPort() == 0);
		success &= (imf_ep9.GetRelayHop() == 0);

		IMFEndpoint imf_ep10(IMFEndpoint::Protocol::IMF, IMFAddress(), 0, 2000, 2);

		IMFEndpoint imf_ep11(IMFEndpoint::Protocol::IMF, imfa, 9, 2000, 2);
		success &= (imf_ep11.GetProtocol() == IMFEndpoint::Protocol::IMF);
		success &= (imf_ep11.GetIMFAddress() == imfa);
		success &= (imf_ep11.GetPort() == 9);
		success &= (imf_ep11.GetRelayPort() == 2000);
		success &= (imf_ep11.GetRelayHop() == 2);

		return success;
	}

	TEST_CLASS(IMFEndpointTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Default construction
			IMFEndpoint imf1;
			Assert::AreEqual(true, imf1.GetProtocol() == IMFEndpoint::Protocol::Unspecified);
			Assert::AreEqual(true, imf1.GetIMFAddress() == IMFAddress());
			Assert::AreEqual(true, imf1.GetPort() == 0);
			Assert::AreEqual(true, imf1.GetRelayHop() == 0);
			Assert::AreEqual(true, imf1.GetRelayPort() == 0);

			// Construction
			IMFEndpoint imf2(IMFEndpoint::Protocol::IMF, IMFAddress(L"info@example.com"), 999, 1, 1);
			Assert::AreEqual(true, imf2.GetProtocol() == IMFEndpoint::Protocol::IMF);
			Assert::AreEqual(true, imf2.GetIMFAddress() == IMFAddress(L"info@example.com"));
			Assert::AreEqual(true, imf2.GetPort() == 999);
			Assert::AreEqual(true, imf2.GetRelayHop() == 1);
			Assert::AreEqual(true, imf2.GetRelayPort() == 1);

			IMFEndpoint imf2a(IMFEndpoint::Protocol::IMF, IMFAddress(L"info@example.com"), 999, 1, 1);
			Assert::AreEqual(true, imf2a.GetProtocol() == IMFEndpoint::Protocol::IMF);
			Assert::AreEqual(true, imf2a.GetIMFAddress() == IMFAddress(L"info@example.com"));
			Assert::AreEqual(true, imf2a.GetPort() == 999);
			Assert::AreEqual(true, imf2a.GetRelayHop() == 1);
			Assert::AreEqual(true, imf2a.GetRelayPort() == 1);

			// Copy construction
			IMFEndpoint imf3(imf2);
			Assert::AreEqual(true, imf3.GetProtocol() == IMFEndpoint::Protocol::IMF);
			Assert::AreEqual(true, imf3.GetIMFAddress() == IMFAddress(L"info@example.com"));
			Assert::AreEqual(true, imf3.GetPort() == 999);
			Assert::AreEqual(true, imf3.GetRelayHop() == 1);
			Assert::AreEqual(true, imf3.GetRelayPort() == 1);

			// Equal and not equal
			{
				Assert::AreEqual(true, imf2 == imf3);
				Assert::AreEqual(false, imf2 != imf3);
				Assert::AreEqual(true, imf1 != imf2);

				IMFEndpoint imf2a(IMFEndpoint::Protocol::IMF, IMFAddress(L"info@example.com"), 999, 2, 1);
				Assert::AreEqual(true, imf2 != imf2a);
				Assert::AreEqual(false, imf2 == imf2a);

				IMFEndpoint imf2b(IMFEndpoint::Protocol::IMF, IMFAddress(L"info2@example.com"), 999, 1, 1);
				Assert::AreEqual(true, imf2 != imf2b);
				Assert::AreEqual(false, imf2 == imf2b);

				IMFEndpoint imf2c(IMFEndpoint::Protocol::IMF, IMFAddress(L"info@example.com"), 999, 1, 2);
				Assert::AreEqual(true, imf2 != imf2c);
				Assert::AreEqual(false, imf2 == imf2c);

				IMFEndpoint imf2d(IMFEndpoint::Protocol::IMF, IMFAddress(L"info@example.com"), 9999, 1, 2);
				Assert::AreEqual(true, imf2 != imf2d);
				Assert::AreEqual(false, imf2 == imf2d);
			}

			// Move construction
			IMFEndpoint imf4(std::move(imf2));
			Assert::AreEqual(true, imf3 == imf4);

			// Copy assignment
			imf1 = imf3;
			Assert::AreEqual(true, imf3 == imf1);

			IMFEndpoint imf5(IMFEndpoint::Protocol::IMF, IMFAddress(L"info@example.com"), 999);
			Assert::AreEqual(true, imf5.GetProtocol() == IMFEndpoint::Protocol::IMF);
			Assert::AreEqual(true, imf5.GetIMFAddress() == IMFAddress(L"info@example.com"));
			Assert::AreEqual(true, imf5.GetPort() == 999);
			Assert::AreEqual(true, imf5.GetRelayHop() == 0);
			Assert::AreEqual(true, imf5.GetRelayPort() == 0);

			// Move assignment
			imf1 = std::move(imf5);
			Assert::AreEqual(false, imf3 == imf1);
			Assert::AreEqual(true, imf1.GetProtocol() == IMFEndpoint::Protocol::IMF);
			Assert::AreEqual(true, imf1.GetIMFAddress() == IMFAddress(L"info@example.com"));
			Assert::AreEqual(true, imf1.GetPort() == 999);
			Assert::AreEqual(true, imf1.GetRelayHop() == 0);
			Assert::AreEqual(true, imf1.GetRelayPort() == 0);

			// GetString
			auto s = imf1.GetString();
			Assert::AreEqual(true, imf1.GetString() == L"IMF:info@example.com:999");
			Assert::AreEqual(true, imf4.GetString() == L"IMF:info@example.com:999:1:1");
		}

		TEST_METHOD(Input)
		{
			// Test invalid addresses
			Assert::ExpectException<std::invalid_argument>([] { IMFEndpoint(IMFEndpoint::Protocol::IMF, IMFAddress(L""), 9); });
			Assert::ExpectException<std::invalid_argument>([] { IMFEndpoint(IMFEndpoint::Protocol::IMF, IMFAddress(L"abcd"), 9); });
			Assert::ExpectException<std::invalid_argument>([] { IMFEndpoint(IMFEndpoint::Protocol::IMF, IMFAddress(L"(92:5Z:D3:5B:93:B2)"), 9); });
			Assert::ExpectException<std::invalid_argument>([] { IMFEndpoint(IMFEndpoint::Protocol::IMF, IMFAddress(L"192.168.0.1"), 9); });
			Assert::ExpectException<std::invalid_argument>([] { IMFEndpoint(IMFEndpoint::Protocol::IMF, IMFAddress(L"fd12:3456:789a:1::1"), 9, 1000, 2); });

			// Test invalid protocol
			Assert::ExpectException<std::invalid_argument>([] { IMFEndpoint(IMFEndpoint::Protocol::Unspecified, IMFAddress(L"info@example.com"), 9); });
			Assert::ExpectException<std::invalid_argument>([] { IMFEndpoint(static_cast<IMFEndpoint::Protocol>(200), IMFAddress(L"info@example.com"), 9); });

			// Test valid addresses
			try
			{
				IMFEndpoint imf1(IMFEndpoint::Protocol::IMF, IMFAddress(L"info@example.com"), 9);
				IMFEndpoint imf2(IMFEndpoint::Protocol::IMF, IMFAddress(L"info@example.com"), 0);
				IMFEndpoint imf3(IMFEndpoint::Protocol::IMF, IMFAddress(L"info@example.com"), 999, 1000, 1);
			}
			catch (...)
			{
				Assert::Fail(L"Exception thrown while creating IMFEndpoints");
			}
		}

		TEST_METHOD(Constexpr)
		{
			static_assert(CheckIMFEndpointConstexpr() == true);
		}
	};
}