// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\Socket.h"
#include "Common\Util.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

struct ConditionalAcceptData
{
	bool Checked{ false };
	bool Accept{ false };
};

int CALLBACK ConditionalAcceptFunction(LPWSABUF lpCallerId, LPWSABUF lpCallerData, LPQOS lpSQOS,
									   LPQOS lpGQOS, LPWSABUF lpCalleeId, LPWSABUF lpCalleeData,
									   GROUP FAR* g, DWORD_PTR dwCallbackData) noexcept
{
	auto data = reinterpret_cast<ConditionalAcceptData*>(dwCallbackData);
	data->Checked = true;
	if (data->Accept)
	{
		return CF_ACCEPT;
	}
	return CF_REJECT;
}

namespace UnitTests
{
	TEST_CLASS(SocketTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Initialize Winsock
			WSADATA wsaData{ 0 };
			const auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);
			Assert::AreEqual(true, result == 0);

			// Default constructor
			{
				Socket socket;
				Assert::AreEqual(false, socket.GetIOStatus().IsOpen());
				Assert::AreEqual(false, socket.GetIOStatus().IsConnecting());
				Assert::AreEqual(false, socket.GetIOStatus().IsConnected());
				Assert::AreEqual(false, socket.GetIOStatus().IsListening());
				Assert::AreEqual(false, socket.GetIOStatus().CanRead());
				Assert::AreEqual(false, socket.GetIOStatus().CanWrite());
				Assert::AreEqual(false, socket.GetIOStatus().HasException());
				Assert::AreEqual(-1, socket.GetIOStatus().GetErrorCode());
				Assert::AreEqual(true, socket.GetBytesReceived() == 0);
				Assert::AreEqual(true, socket.GetBytesSent() == 0);
				Assert::AreEqual(true, socket.GetAddressFamily() == IP::AddressFamily::Unspecified);
				Assert::AreEqual(true, socket.GetType() == Socket::Type::Unspecified);
				Assert::AreEqual(true, socket.GetProtocol() == IP::Protocol::Unspecified);
			}

			// SOCKET constructor
			{
				const auto handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				Assert::AreEqual(true, handle != INVALID_SOCKET);

				Socket socket(handle);
				Assert::AreEqual(true, socket.GetIOStatus().IsOpen());
				Assert::AreEqual(false, socket.GetIOStatus().IsConnecting());
				Assert::AreEqual(false, socket.GetIOStatus().IsConnected());
				Assert::AreEqual(false, socket.GetIOStatus().IsListening());
				Assert::AreEqual(false, socket.GetIOStatus().CanRead());
				Assert::AreEqual(false, socket.GetIOStatus().CanWrite());
				Assert::AreEqual(false, socket.GetIOStatus().HasException());
				Assert::AreEqual(-1, socket.GetIOStatus().GetErrorCode());
				Assert::AreEqual(true, socket.GetBytesReceived() == 0);
				Assert::AreEqual(true, socket.GetBytesSent() == 0);
				Assert::AreEqual(true, socket.GetAddressFamily() == IP::AddressFamily::IPv4);
				Assert::AreEqual(true, socket.GetType() == Socket::Type::Stream);
				Assert::AreEqual(true, socket.GetProtocol() == IP::Protocol::TCP);

				socket.Close();
				Assert::AreEqual(false, socket.GetIOStatus().IsOpen());
			}

			// Constructor
			{
				std::array<IP::AddressFamily, 2> afs{ IP::AddressFamily::IPv4 ,IP::AddressFamily::IPv6 };

				struct TestCases
				{
					Socket::Type Type{ Socket::Type::Unspecified };
					IP::Protocol Protocol{ IP::Protocol::Unspecified };
				};

				const std::vector<TestCases> tests
				{
					{ Socket::Type::RAW, IP::Protocol::ICMP },
					{ Socket::Type::Datagram, IP::Protocol::UDP },
					{ Socket::Type::Stream, IP::Protocol::TCP }
				};

				for (const auto& test : tests)
				{
					for (const auto af : afs)
					{
						Socket socket(af, test.Type, test.Protocol);
						Assert::AreEqual(true, socket.GetIOStatus().IsOpen());
						Assert::AreEqual(false, socket.GetIOStatus().IsConnecting());
						Assert::AreEqual(false, socket.GetIOStatus().IsConnected());
						Assert::AreEqual(false, socket.GetIOStatus().IsListening());
						Assert::AreEqual(false, socket.GetIOStatus().CanRead());
						Assert::AreEqual(false, socket.GetIOStatus().CanWrite());
						Assert::AreEqual(false, socket.GetIOStatus().HasException());
						Assert::AreEqual(-1, socket.GetIOStatus().GetErrorCode());
						Assert::AreEqual(true, socket.GetBytesReceived() == 0);
						Assert::AreEqual(true, socket.GetBytesSent() == 0);
						Assert::AreEqual(true, socket.GetAddressFamily() == af);
						Assert::AreEqual(true, socket.GetType() == test.Type);

						// Windows returns Unspecified for some reason in the case of ICMP
						if (test.Protocol != IP::Protocol::ICMP)
						{
							Assert::AreEqual(true, socket.GetProtocol() == test.Protocol);
						}

						socket.Close();
						Assert::AreEqual(false, socket.GetIOStatus().IsOpen());
					}
				}
			}

			WSACleanup();
		}

		TEST_METHOD(UDPSendReceive)
		{
			// Initialize Winsock
			WSADATA wsaData{ 0 };
			const auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);
			Assert::AreEqual(true, result == 0);

			const std::array<IPAddress, 2> ips{ IPAddress::LoopbackIPv4(), IPAddress::LoopbackIPv6() };

			for (const auto& ip : ips)
			{
				// Create first socket
				const auto endp1 = IPEndpoint(ip, 9000);
				Socket socket1(endp1.GetIPAddress().GetFamily(), Socket::Type::Datagram, IP::Protocol::UDP);
				Assert::AreEqual(true, socket1.Bind(endp1, false));

				Assert::AreEqual(true, socket1.UpdateIOStatus(0ms));
				Assert::AreEqual(true, socket1.GetIOStatus().IsOpen());
				Assert::AreEqual(false, socket1.GetIOStatus().IsConnecting());
				Assert::AreEqual(false, socket1.GetIOStatus().IsConnected());
				Assert::AreEqual(false, socket1.GetIOStatus().IsListening());
				Assert::AreEqual(false, socket1.GetIOStatus().CanRead());
				Assert::AreEqual(true, socket1.GetIOStatus().CanWrite());
				Assert::AreEqual(false, socket1.GetIOStatus().HasException());
				Assert::AreEqual(-1, socket1.GetIOStatus().GetErrorCode());
				Assert::AreEqual(true, socket1.GetBytesReceived() == 0);
				Assert::AreEqual(true, socket1.GetBytesSent() == 0);

				// Create second socket
				const auto endp2 = IPEndpoint(ip, 9001);
				Socket socket2(endp2.GetIPAddress().GetFamily(), Socket::Type::Datagram, IP::Protocol::UDP);
				Assert::AreEqual(true, socket2.Bind(endp2, true));

				Assert::AreEqual(true, socket2.UpdateIOStatus(0ms));
				Assert::AreEqual(true, socket2.GetIOStatus().IsOpen());
				Assert::AreEqual(false, socket2.GetIOStatus().IsConnecting());
				Assert::AreEqual(false, socket2.GetIOStatus().IsConnected());
				Assert::AreEqual(false, socket2.GetIOStatus().IsListening());
				Assert::AreEqual(false, socket2.GetIOStatus().CanRead());
				Assert::AreEqual(true, socket2.GetIOStatus().CanWrite());
				Assert::AreEqual(false, socket2.GetIOStatus().HasException());
				Assert::AreEqual(-1, socket2.GetIOStatus().GetErrorCode());
				Assert::AreEqual(true, socket2.GetBytesReceived() == 0);
				Assert::AreEqual(true, socket2.GetBytesSent() == 0);

				// Send data from first socket to second socket
				const auto snd_buf_len = 32u;
				const auto snd_buf1a = Util::GetPseudoRandomBytes(snd_buf_len);
				Buffer snd_buf1b = snd_buf1a;
				Assert::AreEqual(true, socket1.SendTo(endp2, snd_buf1b));
				Assert::AreEqual(true, snd_buf1b.IsEmpty());
				Assert::AreEqual(true, socket1.GetBytesSent() == snd_buf_len);

				// Update IO status selectively
				Assert::AreEqual(true, socket2.UpdateIOStatus(0ms,
															  Socket::IOStatus::Update::Write |
															  Socket::IOStatus::Update::Exception));
				// No read yet
				Assert::AreEqual(false, socket2.GetIOStatus().CanRead());
				// Now update read
				Assert::AreEqual(true, socket2.UpdateIOStatus(5000ms, Socket::IOStatus::Update::Read));
				Assert::AreEqual(true, socket2.GetIOStatus().CanRead());

				// Receive data sent by first socket
				IPEndpoint endp_rcv;
				Buffer rcv_buf;
				Assert::AreEqual(true, socket2.ReceiveFrom(endp_rcv, rcv_buf));
				Assert::AreEqual(true, rcv_buf.GetSize() == snd_buf_len);
				Assert::AreEqual(true, socket2.GetBytesReceived() == snd_buf_len);
				Assert::AreEqual(true, endp_rcv == endp1);
				Assert::AreEqual(true, rcv_buf == snd_buf1a);

				// Move constructor test
				Socket socket3(std::move(socket2));
				Assert::AreEqual(true, socket3.UpdateIOStatus(0ms));
				Assert::AreEqual(true, socket3.GetIOStatus().IsOpen());
				Assert::AreEqual(false, socket3.GetIOStatus().CanRead());
				Assert::AreEqual(true, socket3.GetIOStatus().CanWrite());
				Assert::AreEqual(true, socket3.GetBytesReceived() == snd_buf_len);
				Assert::AreEqual(true, socket3.GetBytesSent() == 0);
				Assert::AreEqual(true, socket3.GetLocalEndpoint() == endp2);

				// Send data from second socket to first socket
				const auto snd_buf2a = Util::GetPseudoRandomBytes(snd_buf_len);
				Buffer snd_buf2b = snd_buf2a;
				Assert::AreEqual(true, socket3.SendTo(endp1, snd_buf2b));
				Assert::AreEqual(true, snd_buf2b.IsEmpty());
				Assert::AreEqual(true, socket3.GetBytesSent() == snd_buf_len);

				Assert::AreEqual(true, socket1.UpdateIOStatus(5000ms));
				Assert::AreEqual(true, socket1.GetIOStatus().CanRead());

				// Receive data on first socket
				IPEndpoint endp_rcv2;
				Buffer rcv_buf2;
				Assert::AreEqual(true, socket1.ReceiveFrom(endp_rcv2, rcv_buf2));
				Assert::AreEqual(true, rcv_buf2.GetSize() == snd_buf_len);
				Assert::AreEqual(true, socket1.GetBytesReceived() == snd_buf_len);
				Assert::AreEqual(true, endp_rcv2 == endp2);
				Assert::AreEqual(true, rcv_buf2 == snd_buf2a);

				socket1.Close();
				Assert::AreEqual(false, socket1.GetIOStatus().IsOpen());
				socket2.Close();
				Assert::AreEqual(false, socket2.GetIOStatus().IsOpen());
			}

			WSACleanup();
		}

		TEST_METHOD(TCPSendReceive)
		{
			// Initialize Winsock
			WSADATA wsaData{ 0 };
			const auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);
			Assert::AreEqual(true, result == 0);

			const std::array<IPAddress, 2> ips{ IPAddress::LoopbackIPv4(), IPAddress::LoopbackIPv6() };

			for (const auto& ip : ips)
			{
				// Create listener socket
				const auto listen_endp = IPEndpoint(ip, 9000);
				Socket listener(listen_endp.GetIPAddress().GetFamily(), Socket::Type::Stream, IP::Protocol::TCP);
				Assert::AreEqual(true, listener.Listen(listen_endp, false, false));

				Assert::AreEqual(true, listener.UpdateIOStatus(0ms));
				Assert::AreEqual(true, listener.GetIOStatus().IsOpen());
				Assert::AreEqual(false, listener.GetIOStatus().IsConnecting());
				Assert::AreEqual(false, listener.GetIOStatus().IsConnected());
				Assert::AreEqual(true, listener.GetIOStatus().IsListening());
				Assert::AreEqual(false, listener.GetIOStatus().CanRead());
				Assert::AreEqual(false, listener.GetIOStatus().CanWrite());
				Assert::AreEqual(false, listener.GetIOStatus().HasException());
				Assert::AreEqual(-1, listener.GetIOStatus().GetErrorCode());
				Assert::AreEqual(true, listener.GetBytesReceived() == 0);
				Assert::AreEqual(true, listener.GetBytesSent() == 0);

				// Create first socket
				Socket socket1(listen_endp.GetIPAddress().GetFamily(), Socket::Type::Stream, IP::Protocol::TCP);

				// Connect first socket to listener socket
				{
					Assert::AreEqual(true, socket1.BeginConnect(listen_endp));
					Assert::AreEqual(true, socket1.GetIOStatus().IsOpen());
					Assert::AreEqual(true, socket1.GetIOStatus().IsConnecting());
					Assert::AreEqual(false, socket1.GetIOStatus().IsConnected());
					Assert::AreEqual(false, socket1.GetIOStatus().CanWrite());

					Assert::AreEqual(true, socket1.UpdateIOStatus(0ms));

					// Becomes writable
					Assert::AreEqual(true, socket1.GetIOStatus().CanWrite());
					Assert::AreEqual(true, socket1.CompleteConnect());

					Assert::AreEqual(false, socket1.GetIOStatus().IsConnecting());
					Assert::AreEqual(true, socket1.GetIOStatus().IsConnected());
					Assert::AreEqual(false, socket1.GetIOStatus().IsListening());
					Assert::AreEqual(false, socket1.GetIOStatus().CanRead());
					Assert::AreEqual(false, socket1.GetIOStatus().HasException());
					Assert::AreEqual(-1, socket1.GetIOStatus().GetErrorCode());
					Assert::AreEqual(true, socket1.GetBytesReceived() == 0);
					Assert::AreEqual(true, socket1.GetBytesSent() == 0);
				}

				Socket socket2;

				// Accept incoming connection on listener socket to socket2
				{
					Assert::AreEqual(true, listener.UpdateIOStatus(5000ms));
					Assert::AreEqual(true, listener.GetIOStatus().CanRead());

					Assert::AreEqual(true, listener.Accept(socket2));

					Assert::AreEqual(true, listener.UpdateIOStatus(0ms));
					Assert::AreEqual(false, listener.GetIOStatus().CanRead());
				}

				{
					Assert::AreEqual(true, socket2.GetIOStatus().IsOpen());
					Assert::AreEqual(false, socket2.GetIOStatus().IsConnecting());
					Assert::AreEqual(true, socket2.GetIOStatus().IsConnected());
					Assert::AreEqual(false, socket2.GetIOStatus().IsListening());
					Assert::AreEqual(false, socket2.GetIOStatus().CanRead());
					Assert::AreEqual(false, socket2.GetIOStatus().CanWrite());
					Assert::AreEqual(false, socket2.GetIOStatus().HasException());
					Assert::AreEqual(-1, socket2.GetIOStatus().GetErrorCode());
					Assert::AreEqual(true, socket2.GetBytesReceived() == 0);
					Assert::AreEqual(true, socket2.GetBytesSent() == 0);

					// Becomes writable
					Assert::AreEqual(true, socket2.UpdateIOStatus(0ms));
					Assert::AreEqual(true, socket2.GetIOStatus().CanWrite());
				}

				// Endpoints should be what we expect
				Assert::AreEqual(true, socket1.GetPeerEndpoint() == listen_endp);
				Assert::AreEqual(true, socket2.GetLocalEndpoint() == listen_endp);

				// Send data from first socket to second socket
				const auto snd_buf_len = 32u;
				const auto snd_buf1a = Util::GetPseudoRandomBytes(snd_buf_len);
				Buffer snd_buf1b = snd_buf1a;
				Assert::AreEqual(true, socket1.Send(snd_buf1b));
				Assert::AreEqual(true, snd_buf1b.IsEmpty());
				Assert::AreEqual(true, socket1.GetBytesSent() == snd_buf_len);

				// Selective IO update check
				Assert::AreEqual(true, socket2.UpdateIOStatus(0ms,
															  Socket::IOStatus::Update::Write |
															  Socket::IOStatus::Update::Exception));
				Assert::AreEqual(false, socket2.GetIOStatus().CanRead());
				Assert::AreEqual(true, socket2.UpdateIOStatus(5000ms, Socket::IOStatus::Update::Read));
				Assert::AreEqual(true, socket2.GetIOStatus().CanRead());

				// Receive data on second socket
				Buffer rcv_buf;
				Assert::AreEqual(true, socket2.Receive(rcv_buf));
				Assert::AreEqual(true, rcv_buf.GetSize() == snd_buf_len);
				Assert::AreEqual(true, socket2.GetBytesReceived() == snd_buf_len);
				Assert::AreEqual(true, rcv_buf == snd_buf1a);

				listener.Close();
				Assert::AreEqual(false, listener.GetIOStatus().IsOpen());

				// Close connection on first socket
				socket1.Close();
				Assert::AreEqual(false, socket1.GetIOStatus().IsOpen());

				// Connection closed on second socket; read returns false
				Assert::AreEqual(true, socket2.UpdateIOStatus(5000ms,
															  Socket::IOStatus::Update::Read |
															  Socket::IOStatus::Update::Exception));
				Assert::AreEqual(false, socket2.Receive(rcv_buf));
				socket2.Close();
				Assert::AreEqual(false, socket2.GetIOStatus().IsOpen());
			}

			WSACleanup();
		}

		TEST_METHOD(TCPListenerConditionalAccept)
		{
			// Initialize Winsock
			WSADATA wsaData{ 0 };
			const auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);
			Assert::AreEqual(true, result == 0);

			const std::array<IPAddress, 2> ips{ IPAddress::LoopbackIPv4(), IPAddress::LoopbackIPv6() };

			for (const auto& ip : ips)
			{
				// Create listener socket
				const auto listen_endp = IPEndpoint(ip, 9000);
				Socket listener(listen_endp.GetIPAddress().GetFamily(), Socket::Type::Stream, IP::Protocol::TCP);
				Assert::AreEqual(true, listener.Listen(listen_endp, true, true));

				Assert::AreEqual(true, listener.UpdateIOStatus(0ms));
				Assert::AreEqual(true, listener.GetIOStatus().IsOpen());
				Assert::AreEqual(false, listener.GetIOStatus().IsConnecting());
				Assert::AreEqual(false, listener.GetIOStatus().IsConnected());
				Assert::AreEqual(true, listener.GetIOStatus().IsListening());
				Assert::AreEqual(false, listener.GetIOStatus().CanRead());
				Assert::AreEqual(false, listener.GetIOStatus().CanWrite());
				Assert::AreEqual(false, listener.GetIOStatus().HasException());
				Assert::AreEqual(-1, listener.GetIOStatus().GetErrorCode());
				Assert::AreEqual(true, listener.GetBytesReceived() == 0);
				Assert::AreEqual(true, listener.GetBytesSent() == 0);

				// Create first socket
				Socket socket1(listen_endp.GetIPAddress().GetFamily(), Socket::Type::Stream, IP::Protocol::TCP);

				// Connect first socket to listener socket
				{
					Assert::AreEqual(true, socket1.BeginConnect(listen_endp));
					Assert::AreEqual(true, socket1.GetIOStatus().IsOpen());
					Assert::AreEqual(true, socket1.GetIOStatus().IsConnecting());
					Assert::AreEqual(false, socket1.GetIOStatus().IsConnected());
					Assert::AreEqual(false, socket1.GetIOStatus().CanWrite());
				}

				Socket socket2;

				// Reject incoming connection on listener socket
				{
					Assert::AreEqual(true, listener.UpdateIOStatus(5000ms));
					Assert::AreEqual(true, listener.GetIOStatus().CanRead());

					ConditionalAcceptData cond_data;
					cond_data.Accept = false;
					cond_data.Checked = false;

					Assert::AreEqual(false, listener.Accept(socket2, true, &ConditionalAcceptFunction, &cond_data));

					Assert::AreEqual(true, cond_data.Checked);

					Assert::AreEqual(true, listener.UpdateIOStatus(0ms));
					Assert::AreEqual(false, listener.GetIOStatus().CanRead());
				}

				// There may be more connection attempts waiting in the queue for socket1;
				// we keep rejecting them
				{
					auto exception = false;
					auto reads = true;

					while (!exception || reads)
					{
						DiscardReturnValue(listener.UpdateIOStatus(100ms));
						reads = listener.GetIOStatus().CanRead();
						if (reads)
						{
							Socket temp;

							ConditionalAcceptData cond_data;
							cond_data.Accept = false;

							DiscardReturnValue(listener.Accept(temp, true, &ConditionalAcceptFunction, &cond_data));
						}

						if (!socket1.GetIOStatus().HasException())
						{
							DiscardReturnValue(socket1.UpdateIOStatus(100ms));
						}
						else exception = true;
					}
				}

				// Failure to connect on socket1 because of rejection
				{
					Assert::AreEqual(true, socket1.GetIOStatus().IsOpen());
					Assert::AreEqual(false, socket1.GetIOStatus().CanWrite());
					Assert::AreEqual(true, socket1.GetIOStatus().IsConnecting());
					Assert::AreEqual(false, socket1.GetIOStatus().IsConnected());
					Assert::AreEqual(false, socket1.GetIOStatus().IsListening());
					Assert::AreEqual(false, socket1.GetIOStatus().CanRead());
					Assert::AreEqual(true, socket1.GetIOStatus().HasException());
					Assert::AreEqual(true, socket1.GetIOStatus().GetErrorCode() != -1);
					Assert::AreEqual(true, socket1.GetBytesReceived() == 0);
					Assert::AreEqual(true, socket1.GetBytesSent() == 0);

					socket1.Close();
				}

				// Try again
				socket1 = Socket(listen_endp.GetIPAddress().GetFamily(), Socket::Type::Stream, IP::Protocol::TCP);

				// Connect first socket to listener socket
				{
					Assert::AreEqual(true, socket1.BeginConnect(listen_endp));
					Assert::AreEqual(true, socket1.GetIOStatus().IsOpen());
					Assert::AreEqual(true, socket1.GetIOStatus().IsConnecting());
					Assert::AreEqual(false, socket1.GetIOStatus().IsConnected());
					Assert::AreEqual(false, socket1.GetIOStatus().CanWrite());
				}

				// Accept incoming connection on listener socket to socket2
				{
					Assert::AreEqual(true, listener.UpdateIOStatus(5000ms));
					Assert::AreEqual(true, listener.GetIOStatus().CanRead());

					ConditionalAcceptData cond_data;
					cond_data.Accept = true;
					cond_data.Checked = false;

					Assert::AreEqual(true, listener.Accept(socket2, true, &ConditionalAcceptFunction, &cond_data));

					Assert::AreEqual(true, cond_data.Checked);

					Assert::AreEqual(true, listener.UpdateIOStatus(0ms));
					Assert::AreEqual(false, listener.GetIOStatus().CanRead());
				}

				// Connection succeeded on socket1
				{
					Assert::AreEqual(true, socket1.UpdateIOStatus(5000ms));

					// Becomes writable
					Assert::AreEqual(true, socket1.GetIOStatus().CanWrite());
					Assert::AreEqual(true, socket1.CompleteConnect());

					Assert::AreEqual(false, socket1.GetIOStatus().IsConnecting());
					Assert::AreEqual(true, socket1.GetIOStatus().IsConnected());
					Assert::AreEqual(false, socket1.GetIOStatus().IsListening());
					Assert::AreEqual(false, socket1.GetIOStatus().CanRead());
					Assert::AreEqual(false, socket1.GetIOStatus().HasException());
					Assert::AreEqual(-1, socket1.GetIOStatus().GetErrorCode());
					Assert::AreEqual(true, socket1.GetBytesReceived() == 0);
					Assert::AreEqual(true, socket1.GetBytesSent() == 0);
				}

				// Connection succeeded on socket2
				{
					Assert::AreEqual(true, socket2.GetIOStatus().IsOpen());
					Assert::AreEqual(false, socket2.GetIOStatus().IsConnecting());
					Assert::AreEqual(true, socket2.GetIOStatus().IsConnected());
					Assert::AreEqual(false, socket2.GetIOStatus().IsListening());
					Assert::AreEqual(false, socket2.GetIOStatus().CanRead());
					Assert::AreEqual(false, socket2.GetIOStatus().CanWrite());
					Assert::AreEqual(false, socket2.GetIOStatus().HasException());
					Assert::AreEqual(-1, socket2.GetIOStatus().GetErrorCode());
					Assert::AreEqual(true, socket2.GetBytesReceived() == 0);
					Assert::AreEqual(true, socket2.GetBytesSent() == 0);

					// Becomes writable
					Assert::AreEqual(true, socket2.UpdateIOStatus(0ms));
					Assert::AreEqual(true, socket2.GetIOStatus().CanWrite());
				}

				// Endpoints should be what we expect
				Assert::AreEqual(true, socket1.GetPeerEndpoint() == listen_endp);
				Assert::AreEqual(true, socket2.GetLocalEndpoint() == listen_endp);

				listener.Close();
				Assert::AreEqual(false, listener.GetIOStatus().IsOpen());

				// Close connection on first socket
				socket1.Close();
				Assert::AreEqual(false, socket1.GetIOStatus().IsOpen());

				// Close connection on second socket
				socket2.Close();
				Assert::AreEqual(false, socket2.GetIOStatus().IsOpen());
			}

			WSACleanup();
		}
	};
}