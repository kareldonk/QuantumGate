// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\Endpoint.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	TEST_CLASS(EndpointTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Default construction
			Endpoint ep;
			Assert::AreEqual(true, ep.GetString() == L"Unspecified");
			Assert::AreEqual(true, ep.GetType() == Endpoint::Type::Unspecified);
			Assert::AreEqual(true, ep.GetProtocol() == Endpoint::Protocol::Unspecified);
			Assert::AreEqual(true, ep.GetAddressFamily() == Endpoint::AddressFamily::Unspecified);
			Assert::AreEqual(true, ep.GetRelayPort() == 0);
			Assert::AreEqual(true, ep.GetRelayHop() == 0);

			// Construction
			Endpoint ep2(BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(L"(92:5F:D3:5B:93:B2)"), 9,
									 BTHEndpoint::GetNullServiceClassID(), 2000, 2));
			auto ss = ep2.GetString();
			Assert::AreEqual(true, ep2.GetString() == L"RFCOMM:(92:5F:D3:5B:93:B2):9:2000:2");
			Assert::AreEqual(true, ep2.GetType() == Endpoint::Type::BTH);
			Assert::AreEqual(true, ep2.GetProtocol() == Endpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, ep2.GetAddressFamily() == Endpoint::AddressFamily::BTH);
			Assert::AreEqual(true, ep2.GetBTHEndpoint().GetBTHAddress().GetBinary().UInt64s == 0x925FD35B93B2);
			Assert::AreEqual(true, ep2.GetRelayPort() == 2000);
			Assert::AreEqual(true, ep2.GetRelayHop() == 2);

			Endpoint ep3(IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"192.168.0.1"), 80, 3000, 3));
			Assert::AreEqual(true, ep3.GetString() == L"TCP:192.168.0.1:80:3000:3");
			Assert::AreEqual(true, ep3.GetType() == Endpoint::Type::IP);
			Assert::AreEqual(true, ep3.GetAddressFamily() == Endpoint::AddressFamily::IPv4);
			Assert::AreEqual(true, ep3.GetIPEndpoint().GetIPAddress().GetBinary().UInt32s[0] == 0x0100A8C0);
			Assert::AreEqual(true, ep3.GetRelayPort() == 3000);
			Assert::AreEqual(true, ep3.GetRelayHop() == 3);

			// Copy construction
			Endpoint ep4(ep2);
			Assert::AreEqual(true, ep4.GetString() == L"RFCOMM:(92:5F:D3:5B:93:B2):9:2000:2");
			Assert::AreEqual(true, ep4.GetType() == Endpoint::Type::BTH);
			Assert::AreEqual(true, ep4.GetProtocol() == Endpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, ep4.GetAddressFamily() == Endpoint::AddressFamily::BTH);
			Assert::AreEqual(true, ep4.GetBTHEndpoint().GetBTHAddress().GetBinary().UInt64s == 0x925FD35B93B2);
			Assert::AreEqual(true, ep4.GetRelayPort() == 2000);
			Assert::AreEqual(true, ep4.GetRelayHop() == 2);

			// Equal and not equal
			Assert::AreEqual(true, ep2 == ep4);
			Assert::AreEqual(false, ep2 != ep4);
			Assert::AreEqual(true, ep2 != ep3);
			Assert::AreEqual(true, ep != ep2);
			Assert::AreEqual(true, ep != ep3);

			// Move construction
			Endpoint ep5(std::move(ep2));
			Assert::AreEqual(true, ep5.GetString() == L"RFCOMM:(92:5F:D3:5B:93:B2):9:2000:2");
			Assert::AreEqual(true, ep5.GetType() == Endpoint::Type::BTH);
			Assert::AreEqual(true, ep5.GetProtocol() == Endpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, ep5.GetAddressFamily() == Endpoint::AddressFamily::BTH);
			Assert::AreEqual(true, ep5.GetBTHEndpoint().GetBTHAddress().GetBinary().UInt64s == 0x925FD35B93B2);
			Assert::AreEqual(true, ep5.GetRelayPort() == 2000);
			Assert::AreEqual(true, ep5.GetRelayHop() == 2);

			Assert::AreEqual(true, ep5 == ep4);

			// Copy assignment
			ep = ep5;
			Assert::AreEqual(true, ep.GetString() == L"RFCOMM:(92:5F:D3:5B:93:B2):9:2000:2");
			Assert::AreEqual(true, ep.GetType() == Endpoint::Type::BTH);
			Assert::AreEqual(true, ep.GetProtocol() == Endpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, ep.GetAddressFamily() == Endpoint::AddressFamily::BTH);
			Assert::AreEqual(true, ep.GetBTHEndpoint().GetBTHAddress().GetBinary().UInt64s == 0x925FD35B93B2);
			Assert::AreEqual(true, ep.GetRelayPort() == 2000);
			Assert::AreEqual(true, ep.GetRelayHop() == 2);

			Assert::AreEqual(true, ep5 == ep);

			// Move assignment
			const auto ep6 = std::move(ep5);
			Assert::AreEqual(true, ep6.GetString() == L"RFCOMM:(92:5F:D3:5B:93:B2):9:2000:2");
			Assert::AreEqual(true, ep6.GetType() == Endpoint::Type::BTH);
			Assert::AreEqual(true, ep6.GetProtocol() == Endpoint::Protocol::RFCOMM);
			Assert::AreEqual(true, ep6.GetAddressFamily() == Endpoint::AddressFamily::BTH);
			Assert::AreEqual(true, ep6.GetBTHEndpoint().GetBTHAddress().GetBinary().UInt64s == 0x925FD35B93B2);
			Assert::AreEqual(true, ep6.GetRelayPort() == 2000);
			Assert::AreEqual(true, ep6.GetRelayHop() == 2);

			Assert::AreEqual(true, ep6 == ep);

			// Move assignment of different Endpoint type
			ep = std::move(ep3);
			Assert::AreEqual(true, ep.GetString() == L"TCP:192.168.0.1:80:3000:3");
			Assert::AreEqual(true, ep.GetType() == Endpoint::Type::IP);
			Assert::AreEqual(true, ep.GetAddressFamily() == Endpoint::AddressFamily::IPv4);
			Assert::AreEqual(true, ep.GetIPEndpoint().GetIPAddress().GetBinary().UInt32s[0] == 0x0100A8C0);
			Assert::AreEqual(true, ep.GetRelayPort() == 3000);
			Assert::AreEqual(true, ep.GetRelayHop() == 3);
		}

		TEST_METHOD(Constexpr)
		{
			constexpr auto bin_bth = BinaryBTHAddress(BinaryBTHAddress::Family::BTH, 0x925FD35B93B2);
			constexpr auto bth_ep = BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, BTHAddress(bin_bth), 9);
			constexpr Endpoint ep(bth_ep);
			constexpr auto bin_bth2 = ep.GetBTHEndpoint().GetBTHAddress().GetBinary();
			constexpr auto type = ep.GetType();

			static_assert(type == Endpoint::Type::BTH, "Should be equal");
			static_assert(bin_bth2 == bin_bth, "Should be equal");

			Assert::AreEqual(true, type == Endpoint::Type::BTH);
			Assert::AreEqual(true, bin_bth2 == bin_bth);
		}
	};
}