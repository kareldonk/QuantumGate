// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Network\Ping.h"
#include "Common\Util.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

std::optional<IPAddress> ResolveIP(const std::wstring domain)
{
	std::optional<IPAddress> ip;

	ADDRINFOW* result{ nullptr };

	const auto ret = GetAddrInfoW(domain.c_str(), L"", nullptr, &result);
	if (ret == 0)
	{
		for (auto ptr = result; ptr != nullptr; ptr = ptr->ai_next)
		{
			if (ptr->ai_family == AF_INET || ptr->ai_family == AF_INET6)
			{
				ip = IPAddress(ptr->ai_addr);
				break;
			}
		}

		FreeAddrInfoW(result);
	}

	return ip;
}

namespace UnitTests
{
	TEST_CLASS(PingTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Initialize Winsock
			WSADATA wsaData{ 0 };
			const auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);
			Assert::AreEqual(true, result == 0);

			struct TestCases
			{
				BinaryIPAddress DestinationIP;
				UInt16 BufferSize{ 0 };
				std::chrono::milliseconds Timeout{ 5 };
				std::chrono::seconds TTL{ 64 };

				Ping::Status Status{ Ping::Status::Unknown };
				std::optional<BinaryIPAddress> RespondingIP;
				std::optional<std::chrono::seconds> RTT;
				bool Success{ false };
			};

			const auto google_ip = ResolveIP(L"google.com");
			Assert::AreEqual(true, google_ip.has_value());

			const std::vector<TestCases> tests
			{
				// Bad IP
				{
					BinaryIPAddress(),
					32, 5000ms, 64s,
					Ping::Status::Failed, std::nullopt, std::nullopt,
					false
				},

				// Bad IP
				{
					IPAddress::AnyIPv4().GetBinary(),
					32, 5000ms, 64s,
					Ping::Status::Failed, std::nullopt, std::nullopt,
					false
				},

				// Local host IPv4
				{
					IPAddress::LoopbackIPv4().GetBinary(),
					32, 5000ms, 64s,
					Ping::Status::Succeeded, IPAddress::LoopbackIPv4().GetBinary(), std::nullopt,
					true
				},

				// Local host IPv6
				{
					IPAddress::LoopbackIPv6().GetBinary(),
					32, 5000ms, 64s,
					Ping::Status::Succeeded, IPAddress::LoopbackIPv6().GetBinary(), std::nullopt,
					true
				},

				// Zero buffer size
				{
					IPAddress::LoopbackIPv4().GetBinary(),
					0, 5000ms, 64s,
					Ping::Status::Succeeded, IPAddress::LoopbackIPv4().GetBinary(), std::nullopt,
					true
				},

				// Big buffer
				{
					IPAddress::LoopbackIPv4().GetBinary(),
					512, 5000ms, 64s,
					Ping::Status::Succeeded, IPAddress::LoopbackIPv4().GetBinary(), std::nullopt,
					true
				},

				{
					google_ip->GetBinary(),
					32, 5000ms, 64s,
					Ping::Status::Succeeded, google_ip->GetBinary(), std::nullopt,
					true
				},

				{
					google_ip->GetBinary(),
					0, 5000ms, 64s,
					Ping::Status::Succeeded, google_ip->GetBinary(), std::nullopt,
					true
				},
				
				// Low TTL
				{
					google_ip->GetBinary(),
					32, 5000ms, 5s,
					Ping::Status::TimeToLiveExceeded, std::nullopt, std::nullopt,
					true
				},

				// (Hopefully) non existent IP
				{
					IPAddress(L"192.168.111.111").GetBinary(),
					32, 5000ms, 64s,
					Ping::Status::Timedout, std::nullopt, std::nullopt,
					true
				}
			};

			for (const auto& test : tests)
			{
				Ping ping(test.DestinationIP, test.BufferSize, test.Timeout, test.TTL);
				Assert::AreEqual(test.Success, ping.Execute());

				auto status = ping.GetStatus();
				auto dest_ip = ping.GetDestinationIPAddress();
				auto bufsize = ping.GetBufferSize();
				auto ttl = ping.GetTTL();
				auto timeout = ping.GetTimeout();

				auto resp_ip = ping.GetRespondingIPAddress();
				auto rttl = ping.GetResponseTTL();
				auto rtt = ping.GetRoundTripTime();

				Assert::AreEqual(true, status == test.Status);
				Assert::AreEqual(true, dest_ip == test.DestinationIP);

				if (test.RespondingIP.has_value()) Assert::AreEqual(true, resp_ip == test.RespondingIP);
				if (test.RTT.has_value()) Assert::AreEqual(true, rtt == *test.RTT);

				// Move assignment
				auto ping2 = std::move(ping);
				Assert::AreEqual(true, ping2.GetStatus() == status);
				Assert::AreEqual(true, ping2.GetRespondingIPAddress() == resp_ip);
				Assert::AreEqual(true, ping2.GetDestinationIPAddress() == dest_ip);
				Assert::AreEqual(true, ping2.GetTTL() == ttl);
				Assert::AreEqual(true, ping2.GetTimeout() == timeout);
				Assert::AreEqual(true, ping2.GetRoundTripTime() == rtt);
				Assert::AreEqual(true, ping2.GetResponseTTL() == rttl);
				Assert::AreEqual(true, ping2.GetBufferSize() == bufsize);

				// Move constructor
				auto ping3(std::move(ping2));
				Assert::AreEqual(true, ping3.GetStatus() == status);
				Assert::AreEqual(true, ping3.GetRespondingIPAddress() == resp_ip);
				Assert::AreEqual(true, ping3.GetDestinationIPAddress() == dest_ip);
				Assert::AreEqual(true, ping3.GetTTL() == ttl);
				Assert::AreEqual(true, ping3.GetTimeout() == timeout);
				Assert::AreEqual(true, ping3.GetRoundTripTime() == rtt);
				Assert::AreEqual(true, ping3.GetResponseTTL() == rttl);
				Assert::AreEqual(true, ping3.GetBufferSize() == bufsize);
			}

			WSACleanup();
		}
	};
}