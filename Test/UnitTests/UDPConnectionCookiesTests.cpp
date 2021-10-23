// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Settings.h"
#include "Common\Util.h"

// Undefine conflicting macro
#ifdef max
#undef max
#endif

#include "Core\UDP\UDPConnectionCookies.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Core::UDP::Listener;

namespace UnitTests
{
	TEST_CLASS(UDPConnectionCookiesTests)
	{
	public:
		TEST_METHOD(CookiesCheck)
		{
			const auto expiration = 4s;

			ConnectionCookies cc;
			const auto init_result = cc.Initialize(Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(true, init_result);

			const auto endpoint1 = IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"3.30.120.5"), 2000);
			const auto endpoint2 = IPEndpoint(IPEndpoint::Protocol::UDP, IPAddress(L"3.30.120.5"), 2000);
			const auto endpoint3 = IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"3.50.120.5"), 2000);
			const auto endpoint4 = IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"3.30.120.5"), 3000);
			const auto connid1 = 123;
			const auto connid2 = 456;

			const auto cookiedata1 = cc.GetCookie(connid1, endpoint1, Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(true, cookiedata1.has_value());

			// Same data within expiration should give same cookie
			{
				const auto cookiedata2 = cc.GetCookie(connid1, endpoint1, Util::GetCurrentSteadyTime(), expiration);
				Assert::AreEqual(true, cookiedata2.has_value());

				Assert::AreEqual(true, cookiedata1->CookieID == cookiedata2->CookieID);
			}

			// Different protocol should give different cookie
			{
				const auto cookiedata2 = cc.GetCookie(connid1, endpoint2, Util::GetCurrentSteadyTime(), expiration);
				Assert::AreEqual(true, cookiedata2.has_value());

				Assert::AreEqual(true, cookiedata1->CookieID != cookiedata2->CookieID);
			}

			// Different IP should give different cookie
			{
				const auto cookiedata2 = cc.GetCookie(connid1, endpoint3, Util::GetCurrentSteadyTime(), expiration);
				Assert::AreEqual(true, cookiedata2.has_value());

				Assert::AreEqual(true, cookiedata1->CookieID != cookiedata2->CookieID);
			}

			// Different port should give different cookie
			{
				const auto cookiedata2 = cc.GetCookie(connid1, endpoint4, Util::GetCurrentSteadyTime(), expiration);
				Assert::AreEqual(true, cookiedata2.has_value());

				Assert::AreEqual(true, cookiedata1->CookieID != cookiedata2->CookieID);
			}

			// Different connection ID should give different cookie
			{
				const auto cookiedata2 = cc.GetCookie(connid2, endpoint1, Util::GetCurrentSteadyTime(), expiration);
				Assert::AreEqual(true, cookiedata2.has_value());

				Assert::AreEqual(true, cookiedata1->CookieID != cookiedata2->CookieID);
			}

			std::this_thread::sleep_for(2200ms);

			// Same data after half of expiration interval should give different cookie
			{
				const auto cookiedata2 = cc.GetCookie(connid1, endpoint1, Util::GetCurrentSteadyTime(), expiration);
				Assert::AreEqual(true, cookiedata2.has_value());

				Assert::AreEqual(true, cookiedata1->CookieID != cookiedata2->CookieID);

				// But original cookie still valid
				const auto veri_result1 = cc.VerifyCookie(*cookiedata1, connid1, endpoint1, Util::GetCurrentSteadyTime(), expiration);
				Assert::AreEqual(true, veri_result1);
			}
		}

		TEST_METHOD(CookiesExpirationTests)
		{
			const auto expiration = 4s;

			ConnectionCookies cc;
			const auto init_result = cc.Initialize(Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(true, init_result);

			const auto endpoint = IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(L"3.30.120.5"), 2000);
			const auto connid = 123;

			auto cookiedata = cc.GetCookie(connid, endpoint, Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(true, cookiedata.has_value());

			std::this_thread::sleep_for(1200ms);

			const auto veri_result1 = cc.VerifyCookie(*cookiedata, connid, endpoint, Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(true, veri_result1);

			std::this_thread::sleep_for(1000ms);

			const auto veri_result2 = cc.VerifyCookie(*cookiedata, connid, endpoint, Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(true, veri_result2);

			// After half of expiration time, same data results in a different cookie because another key is used
			auto cookiedata2 = cc.GetCookie(connid, endpoint, Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(true, cookiedata2.has_value());

			Assert::AreEqual(true, cookiedata->CookieID != cookiedata2->CookieID);

			std::this_thread::sleep_for(2000ms);

			// First cookie is now expired
			const auto veri_result3 = cc.VerifyCookie(*cookiedata, connid, endpoint, Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(false, veri_result3);

			// Second cookie is still valid
			const auto veri_result4 = cc.VerifyCookie(*cookiedata2, connid, endpoint, Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(true, veri_result4);

			std::this_thread::sleep_for(2200ms);

			// Second cookie has now also expired
			const auto veri_result5 = cc.VerifyCookie(*cookiedata2, connid, endpoint, Util::GetCurrentSteadyTime(), expiration);
			Assert::AreEqual(false, veri_result5);
		}
	};
}