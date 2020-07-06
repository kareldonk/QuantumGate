// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Socket.h"

#include <ws2tcpip.h>
#include <Mstcpip.h>

using namespace std::literals;

namespace QuantumGate::Implementation::Network
{
	Socket::Socket() noexcept
#ifdef USE_SOCKET_EVENT
		: m_Event(WSACreateEvent())
#endif
	{}

	Socket::Socket(const IP::AddressFamily af, const Type type, const IP::Protocol protocol) : Socket()
	{
		int saf{ 0 };
		int stype{ 0 };
		int sprotocol{ 0 };

		switch (af)
		{
			case IP::AddressFamily::Unspecified:
				saf = AF_UNSPEC;
				break;
			case IP::AddressFamily::IPv4:
				saf = AF_INET;
				break;
			case IP::AddressFamily::IPv6:
				saf = AF_INET6;
				break;
			default:
				throw SocketException("Unsupported address family");
				assert(false);
				return;
		}

		switch (type)
		{
			case Type::Stream:
				stype = SOCK_STREAM;
				break;
			case Type::Datagram:
				stype = SOCK_DGRAM;
				break;
			case Type::RAW:
				stype = SOCK_RAW;
				break;
			default:
				throw SocketException("Unsupported socket type");
				assert(false);
				return;
		}

		switch (protocol)
		{
			case IP::Protocol::Unspecified:
				sprotocol = IPPROTO_IP;
				break;
			case IP::Protocol::TCP:
				sprotocol = IPPROTO_TCP;
				break;
			case IP::Protocol::UDP:
				sprotocol = IPPROTO_UDP;
				break;
			case IP::Protocol::ICMP:
				sprotocol = IPPROTO_ICMP;
				break;
			default:
				throw SocketException("Unsupported IP protocol");
				assert(false);
				return;
		}

		const auto s = socket(saf, stype, sprotocol);
		if (s != INVALID_SOCKET)
		{
			if (SetSocket(s))
			{
				UpdateSocketInfo();
				return;
			}
		}

		throw SocketException(Util::ToStringA(
			Util::FormatString(L"Failed to create socket (%s)", GetLastSocketErrorString().c_str())).c_str());
	}

	Socket::Socket(const SOCKET s)
	{
		if (s != INVALID_SOCKET)
		{
			if (SetSocket(s))
			{
				UpdateSocketInfo();
				return;
			}
			else throw SocketException("Failed to set socket");
		}
		else throw SocketException("Invalid argument");
	}

	Socket::~Socket()
	{
		Release();
	}

	void Socket::Release() noexcept
	{
		if (m_IOStatus.IsOpen()) Close();

#ifdef USE_SOCKET_EVENT
		m_Event.Release();
#endif
	}

	Socket::Socket(Socket&& other) noexcept :
		m_Socket(std::exchange(other.m_Socket, INVALID_SOCKET)),
#ifdef USE_SOCKET_EVENT
		m_Event(std::exchange(other.m_Event, Concurrency::Event{})),
#endif
		m_IOStatus(std::exchange(other.m_IOStatus, IOStatus{})),
		m_BytesReceived(std::exchange(other.m_BytesReceived, 0)),
		m_BytesSent(std::exchange(other.m_BytesSent, 0)),
		m_LocalEndpoint(std::move(other.m_LocalEndpoint)),
		m_PeerEndpoint(std::move(other.m_PeerEndpoint)),
		m_ConnectedSteadyTime(std::exchange(other.m_ConnectedSteadyTime, SteadyTime{}))
	{}

	Socket& Socket::operator=(Socket&& other) noexcept
	{
		// Check for same object
		if (this == &other) return *this;

		Release();

		m_Socket = std::exchange(other.m_Socket, INVALID_SOCKET);
#ifdef USE_SOCKET_EVENT
		m_Event = std::exchange(other.m_Event, {});
#endif
		m_IOStatus = std::exchange(other.m_IOStatus, IOStatus());

		m_BytesReceived = std::exchange(other.m_BytesReceived, 0);
		m_BytesSent = std::exchange(other.m_BytesSent, 0);

		m_LocalEndpoint = std::move(other.m_LocalEndpoint);
		m_PeerEndpoint = std::move(other.m_PeerEndpoint);

		m_ConnectedSteadyTime = std::exchange(other.m_ConnectedSteadyTime, SteadyTime{});

		return *this;
	}

	bool Socket::SetSocket(const SOCKET s, const bool excl_addr_use, const bool blocking) noexcept
	{
		assert(s != INVALID_SOCKET);

		m_Socket = s;
		m_IOStatus.SetOpen(true);

		if (excl_addr_use)
		{
			// Enable exclusive address use for added security to prevent port hijacking
			// Docs: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740621(v=vs.85).aspx
			if (!SetExclusiveAddressUse(true)) return false;
		}

		if (!blocking)
		{
			if (!SetBlockingMode(false)) return false;
		}

		if (GetProtocol() == IP::Protocol::TCP)
		{
			if (!SetNoDelay(true)) return false;
		}

#ifdef USE_SOCKET_EVENT
		if (!AttachEvent()) return false;
#endif

		return true;
	}

	void Socket::Close(const bool linger) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		m_CloseCallback();

		if (GetProtocol() == IP::Protocol::TCP)
		{
			// If we're supposed to abort the connection, set the linger value on the socket to 0,
			// else keep connection alive for a few seconds to give time for shutdown
			if (linger) DiscardReturnValue(SetLinger(Socket::DefaultLingerTime));
			else DiscardReturnValue(SetLinger(0s));
		}

#ifdef USE_SOCKET_EVENT
		DetachEvent();
#endif

		closesocket(m_Socket);

		m_IOStatus.Reset();
	}

#ifdef USE_SOCKET_EVENT
	bool Socket::AttachEvent() noexcept
	{
		const auto ret = WSAEventSelect(m_Socket, m_Event.GetHandle(),
										FD_ACCEPT | FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE);
		if (ret != SOCKET_ERROR)
		{
			return true;
		}
		else LogErr(L"Could not set event for socket (%s)", GetLastSocketErrorString().c_str());

		return false;
	}

	void Socket::DetachEvent() noexcept
	{
		WSAEventSelect(m_Socket, nullptr, 0);
	}
#endif

	void Socket::UpdateSocketInfo() noexcept
	{
		m_ConnectedSteadyTime = Util::GetCurrentSteadyTime();

		sockaddr_storage addr{ 0 };
		int nlen = sizeof(sockaddr_storage);

		// This will only work for connected or bound (listener) sockets
		auto err = getsockname(m_Socket, reinterpret_cast<sockaddr*>(&addr), &nlen);
		if (err != SOCKET_ERROR)
		{
			if (!SockAddrGetIPEndpoint(&addr, m_LocalEndpoint))
			{
				LogErr(L"Could not get local endpoint for socket");
			}

			err = getpeername(m_Socket, reinterpret_cast<sockaddr*>(&addr), &nlen);
			if (err != SOCKET_ERROR)
			{
				if (!SockAddrGetIPEndpoint(&addr, m_PeerEndpoint))
				{
					LogErr(L"Could not get peer endpoint for socket");
				}
			}
		}
	}

	Buffer& Socket::GetReceiveBuffer() const noexcept
	{
		static thread_local Buffer rcvbuf{ Socket::ReadWriteBufferSize };
		return rcvbuf;
	}

	bool Socket::SetBlockingMode(const bool blocking) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		ULong mode = blocking ? 0 : 1;

		const auto ret = ioctlsocket(m_Socket, FIONBIO, &mode);
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set socket blocking mode for endpoint %s (%s)",
				   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

			return false;
		}

		return true;
	}

	bool Socket::SetExclusiveAddressUse(const bool exclusive) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const int optval = exclusive ? 1 : 0;

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
									reinterpret_cast<const char*>(&optval), sizeof(optval));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set exclusive address use for socket (%s)", GetLastSocketErrorString().c_str());
			return false;
		}

		return true;
	}

	bool Socket::GetExclusiveAddressUse() const noexcept
	{
		return (GetSockOptInt(SO_EXCLUSIVEADDRUSE) == 1) ? true : false;
	}

	bool Socket::SetSendTimeout(const std::chrono::milliseconds& milliseconds) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const DWORD optval = static_cast<DWORD>(milliseconds.count());

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_SNDTIMEO,
									reinterpret_cast<const char*>(&optval), sizeof(optval));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set send timeout socket option for socket (%s)", GetLastSocketErrorString().c_str());
			return false;
		}

		return true;
	}

	bool Socket::SetReceiveTimeout(const std::chrono::milliseconds& milliseconds) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const DWORD optval = static_cast<DWORD>(milliseconds.count());

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_RCVTIMEO,
									reinterpret_cast<const char*>(&optval), sizeof(optval));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set receive timeout socket option for socket (%s)", GetLastSocketErrorString().c_str());
			return false;
		}

		return true;
	}

	bool Socket::SetIPTimeToLive(const std::chrono::seconds& seconds) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const DWORD optval = static_cast<DWORD>(seconds.count());

		const auto ret = setsockopt(m_Socket, IPPROTO_IP, IP_TTL,
									reinterpret_cast<const char*>(&optval), sizeof(optval));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set TTL socket option for socket (%s)", GetLastSocketErrorString().c_str());
			return false;
		}

		return true;
	}

	bool Socket::SetReuseAddress(const bool reuse) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const int optval = reuse ? 1 : 0;

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR,
									reinterpret_cast<const char*>(&optval), sizeof(optval));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set reuse address socket option for socket (%s)", GetLastSocketErrorString().c_str());
			return false;
		}

		return true;
	}

	bool Socket::SetSendBufferSize(const int len) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_SNDBUF,
									reinterpret_cast<const char*>(&len), sizeof(len));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set socket send buffer size for socket (%s)", GetLastSocketErrorString().c_str());
			return false;
		}

		return true;
	}

	bool Socket::SetReceiveBufferSize(const int len) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_RCVBUF,
									reinterpret_cast<const char*>(&len), sizeof(len));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set socket receive buffer size for socket (%s)", GetLastSocketErrorString().c_str());
			return false;
		}

		return true;
	}

	bool Socket::SetLinger(const std::chrono::seconds& seconds) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		LINGER lstruct{ 0 };

		if (seconds == 0s)
		{
			lstruct.l_onoff = 0;
			lstruct.l_linger = 0;
		}
		else
		{
			lstruct.l_onoff = 1;
			lstruct.l_linger = static_cast<u_short>(seconds.count());
		}

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_LINGER,
									reinterpret_cast<char*>(&lstruct), sizeof(lstruct));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set socket linger option for endpoint %s (%s)",
				   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

			return false;
		}

		return true;
	}

	bool Socket::SetNATTraversal(const bool nat_traversal) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		// Enable NAT traversal (in order to accept connections from the Internet on a LAN)
		// Docs: https://msdn.microsoft.com/en-us/library/windows/desktop/aa832668(v=vs.85).aspx
		const int pl = nat_traversal ? PROTECTION_LEVEL_UNRESTRICTED : PROTECTION_LEVEL_DEFAULT;

		const auto ret = setsockopt(m_Socket, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL,
									reinterpret_cast<const char*>(&pl), sizeof(pl));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set IPV6 protection level for endpoint %s (%s)",
				   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

			return false;
		}

		return true;
	}

	bool Socket::SetConditionalAccept(const bool cond_accept) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		// Enable conditional accept (in order to check IP access settings before allowing connection)
		// Docs: https://msdn.microsoft.com/en-us/library/windows/desktop/dd264794(v=vs.85).aspx
		const int ca = cond_accept ? 1 : 0;

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_CONDITIONAL_ACCEPT,
									reinterpret_cast<const char*>(&ca), sizeof(ca));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set conditional accept socket option for endpoint %s (%s)",
				   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

			return false;
		}

		return true;
	}

	bool Socket::SetNoDelay(const bool no_delay) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		// Disables the Nagle algorithm for send coalescing
		// Docs: https://docs.microsoft.com/en-us/windows/win32/winsock/ipproto-tcp-socket-options
		const int nd = no_delay ? 1 : 0;

		const auto ret = setsockopt(m_Socket, IPPROTO_TCP, TCP_NODELAY,
									reinterpret_cast<const char*>(&nd), sizeof(nd));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not disable nagle algorithm for endpoint %s (%s)",
					GetLocalName().c_str(), GetLastSocketErrorString().c_str());

			return false;
		}

		return true;
	}

	bool Socket::Bind(const IPEndpoint& endpoint, const bool nat_traversal) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		if (!SetNATTraversal(nat_traversal)) return false;

		sockaddr_storage saddr{ 0 };

		if (SockAddrSetEndpoint(saddr, endpoint))
		{
			// Bind our name to the socket
			const auto ret = bind(m_Socket, reinterpret_cast<sockaddr*>(&saddr), sizeof(sockaddr_storage));
			if (ret != SOCKET_ERROR)
			{
				UpdateSocketInfo();
				return true;
			}
			else
			{
				LogErr(L"bind() error for endpoint %s (%s)",
					   endpoint.GetString().c_str(), GetLastSocketErrorString().c_str());
			}
		}
		else
		{
			LogErr(L"Endpoint %s not supported or not correct (%s)",
				   endpoint.GetString().c_str(), GetLastSocketErrorString().c_str());
		}

		return false;
	}

	bool Socket::Listen(const IPEndpoint& endpoint, const bool cond_accept,
						const bool nat_traversal) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		if (!SetConditionalAccept(cond_accept)) return false;

		if (!SetNATTraversal(nat_traversal)) return false;

		sockaddr_storage saddr{ 0 };

		if (SockAddrSetEndpoint(saddr, endpoint))
		{
			// Bind our name to the socket
			auto ret = bind(m_Socket, reinterpret_cast<sockaddr*>(&saddr), sizeof(sockaddr_storage));
			if (ret != SOCKET_ERROR)
			{
				// Set the socket to listen
				ret = listen(m_Socket, SOMAXCONN);
				if (ret != SOCKET_ERROR)
				{
					m_IOStatus.SetListening(true);
					UpdateSocketInfo();
					return true;
				}
				else
				{
					LogErr(L"listen() error for endpoint %s (%s)",
						   endpoint.GetString().c_str(), GetLastSocketErrorString().c_str());
				}
			}
			else
			{
				LogErr(L"bind() error for endpoint %s (%s)",
					   endpoint.GetString().c_str(), GetLastSocketErrorString().c_str());
			}
		}
		else
		{
			LogErr(L"Endpoint %s not supported or not correct (%s)",
				   endpoint.GetString().c_str(), GetLastSocketErrorString().c_str());
		}

		return false;
	}

	bool Socket::Accept(Socket& s, const bool cond_accept, const LPCONDITIONPROC cond_func, void* cbdata) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		sockaddr_storage addr{ 0 };
		int addrlen = sizeof(sockaddr_storage);
		SOCKET as{ INVALID_SOCKET };

		if (cond_accept)
		{
			assert(cond_func != nullptr && cbdata != nullptr);

			as = WSAAccept(m_Socket, reinterpret_cast<sockaddr*>(&addr), &addrlen,
						   cond_func, reinterpret_cast<DWORD_PTR>(cbdata));
		}
		else
		{
			as = accept(m_Socket, reinterpret_cast<sockaddr*>(&addr), &addrlen);
		}

		if (as != INVALID_SOCKET)
		{
			if (s.SetSocket(as))
			{
				s.m_IOStatus.SetConnected(true);

				s.UpdateSocketInfo();

				s.m_AcceptCallback();

				return s.m_ConnectCallback();
			}
		}
		else
		{
			Dbg(GetLastSocketErrorString().c_str());
			LogErr(L"A connection could not be accepted on endpoint %s (%s)",
				   GetLocalName().c_str(), GetLastSocketErrorString().c_str());
		}

		return false;
	}

	bool Socket::BeginConnect(const IPEndpoint& endpoint) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		m_IOStatus.SetConnecting(false);

		sockaddr_storage saddr{ 0 };

		if (SockAddrSetEndpoint(saddr, endpoint))
		{
			const auto ret = connect(m_Socket, reinterpret_cast<sockaddr*>(&saddr), sizeof(sockaddr_storage));
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
			{
				LogErr(L"Error connecting to endpoint %s (%s)",
					   endpoint.GetString().c_str(), GetLastSocketErrorString().c_str());
			}
			else
			{
				// While connection attempt succeeded, this doesn't mean a connection was established.
				// A later call to select() to check if the socket is writable will determine if 
				// the connection was established. If the socket is successfully connected 
				// call CompleteConnect().

				m_IOStatus.SetConnecting(true);

				UpdateSocketInfo();

				m_ConnectingCallback();
			}
		}

		return m_IOStatus.IsConnecting();
	}

	bool Socket::CompleteConnect() noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		m_IOStatus.SetConnecting(false);
		m_IOStatus.SetConnected(true);

		UpdateSocketInfo();

		return m_ConnectCallback();
	}

	bool Socket::Send(Buffer& buffer, const Size max_snd_size) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const auto send_size = std::invoke([&]()
		{
			if (max_snd_size > 0 && max_snd_size < buffer.GetSize()) return max_snd_size;
			else return buffer.GetSize();
		});

		const auto bytessent = send(m_Socket, reinterpret_cast<char*>(buffer.GetBytes()),
									static_cast<int>(send_size), 0);

		Dbg(L"%d bytes sent", bytessent);

		if (bytessent > 0)
		{
			try
			{
				buffer.RemoveFirst(bytessent);

				// Update the total amount of bytes sent
				m_BytesSent += bytessent;

				return true;
			}
			catch (const std::exception& e)
			{
				LogErr(L"Send exception for endpoint %s: %s", GetPeerName().c_str(), Util::ToStringW(e.what()).c_str());
			}
		}
		else if (bytessent == SOCKET_ERROR)
		{
			const auto error = WSAGetLastError();
			if (error == WSAENOBUFS || error == WSAEWOULDBLOCK)
			{
				// Send buffer is full or temporarily unavailable, we'll try again later
				LogDbg(L"Send buffer full/unavailable for endpoint %s (%s)",
					   GetPeerName().c_str(), GetLastSocketErrorString().c_str());

				return true;
			}
			else
			{
				LogDbg(L"Send error for endpoint %s (%s)",
					   GetPeerName().c_str(), GetLastSocketErrorString().c_str());
			}
		}

		return false;
	}

	bool Socket::SendTo(const IPEndpoint& endpoint, Buffer& buffer, const Size max_snd_size) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		sockaddr_storage sock_addr{ 0 };
		if (!SockAddrSetEndpoint(sock_addr, endpoint))
		{
			LogDbg(L"Send error on endpoint %s - SockAddrFill() failed for endpoint %s",
				   GetLocalName().c_str(), endpoint.GetString().c_str());
			return false;
		}

		const auto send_size = std::invoke([&]()
		{
			if (max_snd_size > 0 && max_snd_size < buffer.GetSize()) return max_snd_size;
			else return buffer.GetSize();
		});

		if (GetType() == Type::Datagram) assert(send_size < GetMaxDatagramMessageSize());

		const auto bytessent = sendto(m_Socket, reinterpret_cast<char*>(buffer.GetBytes()),
									  static_cast<int>(send_size), 0,
									  reinterpret_cast<sockaddr*>(&sock_addr), sizeof(sock_addr));

		Dbg(L"%d bytes sent", bytessent);

		if (bytessent > 0)
		{
			try
			{
				buffer.RemoveFirst(bytessent);

				// Update the total amount of bytes sent
				m_BytesSent += bytessent;

				return true;
			}
			catch (const std::exception& e)
			{
				LogErr(L"Send exception on endpoint %s: %s", GetLocalName().c_str(), Util::ToStringW(e.what()).c_str());
			}
		}
		else if (bytessent == SOCKET_ERROR)
		{
			const auto error = WSAGetLastError();
			if (error == WSAENOBUFS || error == WSAEWOULDBLOCK)
			{
				// Send buffer is full or temporarily unavailable, we'll try again later
				LogDbg(L"Send buffer full/unavailable on endpoint %s (%s)",
					   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

				return true;
			}
			else
			{
				LogDbg(L"Send error on endpoint %s (%s)",
					   GetLocalName().c_str(), GetLastSocketErrorString().c_str());
			}
		}

		return false;
	}

	bool Socket::Receive(Buffer& buffer, const Size max_rcv_size) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		auto& rcvbuf = GetReceiveBuffer();

		const auto read_size = std::invoke([&]()
		{
			if (max_rcv_size > 0 && max_rcv_size < rcvbuf.GetSize()) return max_rcv_size;
			else return rcvbuf.GetSize();
		});

		const auto bytesrcv = recv(m_Socket, reinterpret_cast<char*>(rcvbuf.GetBytes()), static_cast<int>(read_size), 0);

		Dbg(L"%d bytes received", bytesrcv);

		if (bytesrcv > 0)
		{
			try
			{
				buffer += BufferView(rcvbuf.GetBytes(), bytesrcv);

				// Update the total amount of bytes received
				m_BytesReceived += bytesrcv;

				return true;
			}
			catch (const std::exception& e)
			{
				LogErr(L"Receive exception for endpoint %s: %s", GetPeerName().c_str(), Util::ToStringW(e.what()).c_str());
			}
		}
		else if (bytesrcv == 0)
		{
			if (GetType() == Socket::Type::Stream)
			{
				LogDbg(L"Connection closed for endpoint %s", GetPeerName().c_str());
			}
			else return true;
		}
		else if (bytesrcv == SOCKET_ERROR)
		{
			const auto error = WSAGetLastError();
			if (error == WSAENOBUFS || error == WSAEWOULDBLOCK)
			{
				// Buffer is temporarily unavailable, we'll try again later
				LogDbg(L"Receive buffer unavailable for endpoint %s (%s)",
					   GetPeerName().c_str(), GetLastSocketErrorString().c_str());

				return true;
			}
			else
			{
				LogDbg(L"Receive error for endpoint %s (%s)",
					   GetPeerName().c_str(), GetLastSocketErrorString().c_str());
			}
		}

		return false;
	}

	bool Socket::ReceiveFrom(IPEndpoint& endpoint, Buffer& buffer, const Size max_rcv_size) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		auto& rcvbuf = GetReceiveBuffer();

		const auto read_size = std::invoke([&]()
		{
			if (max_rcv_size > 0 && max_rcv_size < rcvbuf.GetSize()) return max_rcv_size;
			else return rcvbuf.GetSize();
		});

		sockaddr_storage sock_addr{ 0 };
		int sock_addr_len{ sizeof(sock_addr) };

		const auto bytesrcv = recvfrom(m_Socket, reinterpret_cast<char*>(rcvbuf.GetBytes()), static_cast<int>(read_size),
									   0, reinterpret_cast<sockaddr*>(&sock_addr), &sock_addr_len);

		Dbg(L"%d bytes received", bytesrcv);

		if (sock_addr.ss_family != 0)
		{
			if (!SockAddrGetIPEndpoint(&sock_addr, endpoint))
			{
				LogDbg(L"Receive error on endpoint %s - SockAddrGetIPEndpoint() failed",
					   GetLocalName().c_str());
				return false;
			}
		}

		if (bytesrcv > 0)
		{
			try
			{
				buffer += BufferView(rcvbuf.GetBytes(), bytesrcv);

				// Update the total amount of bytes received
				m_BytesReceived += bytesrcv;

				return true;
			}
			catch (const std::exception& e)
			{
				LogErr(L"Receive exception on endpoint %s: %s", GetLocalName().c_str(), Util::ToStringW(e.what()).c_str());
			}
		}
		else if (bytesrcv == 0)
		{
			if (GetType() == Socket::Type::Stream)
			{
				LogDbg(L"Connection closed for endpoint %s", GetLocalName().c_str());
			}
			else return true;
		}
		else if (bytesrcv == SOCKET_ERROR)
		{
			const auto error = WSAGetLastError();
			if (error == WSAENOBUFS || error == WSAEWOULDBLOCK)
			{
				// Buffer is temporarily unavailable, we'll try again later
				LogDbg(L"Receive buffer unavailable on endpoint %s (%s)",
					   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

				return true;
			}
			else if (error == WSAECONNRESET)
			{
				LogDbg(L"Port unreachable for endpoint %s", endpoint.GetString().c_str());
			}
			else
			{
				LogDbg(L"Receive error on endpoint %s (%s)",
					   GetLocalName().c_str(), GetLastSocketErrorString().c_str());
			}
		}

		return false;
	}

#ifdef USE_SOCKET_EVENT
	template<bool read, bool write, bool exception>
	bool Socket::UpdateIOStatusEvent(const std::chrono::milliseconds& mseconds) noexcept
	{
		const auto handle = m_Event.GetHandle();
		const auto ret = WSAWaitForMultipleEvents(1, &handle, false, static_cast<DWORD>(mseconds.count()), false);
		if (ret != WSA_WAIT_FAILED)
		{
			WSANETWORKEVENTS events{ 0 };

			const auto ret2 = WSAEnumNetworkEvents(m_Socket, handle, &events);
			if (ret2 == WSA_WAIT_TIMEOUT) return true;
			else if (ret2 != SOCKET_ERROR)
			{
				if constexpr (read)
				{
					m_IOStatus.SetRead((events.lNetworkEvents & FD_READ) ||
									   (events.lNetworkEvents & FD_ACCEPT) ||
									   (events.lNetworkEvents & FD_CLOSE));
				}

				if constexpr (write)
				{
					if (!m_IOStatus.CanWrite())
					{
						m_IOStatus.SetWrite((events.lNetworkEvents & FD_WRITE) || (events.lNetworkEvents & FD_CONNECT));
					}
				}

				if constexpr (exception)
				{
					const auto set_error = [&](const int idx) noexcept
					{
						m_IOStatus.SetException(true);
						m_IOStatus.SetErrorCode(events.iErrorCode[idx]);
					};

					if ((events.lNetworkEvents & FD_CONNECT) && events.iErrorCode[FD_CONNECT_BIT] != 0) set_error(FD_CONNECT_BIT);
					else if ((events.lNetworkEvents & FD_READ) && events.iErrorCode[FD_READ_BIT] != 0) set_error(FD_READ_BIT);
					else if ((events.lNetworkEvents & FD_WRITE) && events.iErrorCode[FD_WRITE_BIT] != 0) set_error(FD_WRITE_BIT);
					else if ((events.lNetworkEvents & FD_CLOSE) && events.iErrorCode[FD_CLOSE_BIT] != 0) set_error(FD_CLOSE_BIT);
					else if ((events.lNetworkEvents & FD_ACCEPT) && events.iErrorCode[FD_ACCEPT_BIT] != 0) set_error(FD_ACCEPT_BIT);
				}

				return true;
			}
		}

		return false;
	}
#endif

	template<bool read, bool write, bool exception>
	bool Socket::UpdateIOStatusImpl(const std::chrono::milliseconds& mseconds) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

#ifdef USE_SOCKET_EVENT
		return UpdateIOStatusEvent<read, write, exception>(mseconds);
#else
		return UpdateIOStatusFDSet<read, write, exception>(mseconds);
#endif
	}

	template<bool read, bool write, bool exception>
	bool Socket::UpdateIOStatusFDSet(const std::chrono::milliseconds& mseconds) noexcept
	{
		fd_set rset{ 0 }, wset{ 0 }, eset{ 0 };
		fd_set* rset_ptr{ nullptr };
		fd_set* wset_ptr{ nullptr };
		fd_set* eset_ptr{ nullptr };

		if constexpr (read)
		{
			FD_ZERO(&rset);
			FD_SET(m_Socket, &rset);
			rset_ptr = &rset;
		}

		if constexpr (write)
		{
			FD_ZERO(&wset);
			FD_SET(m_Socket, &wset);
			wset_ptr = &wset;
		}

		if constexpr (exception)
		{
			FD_ZERO(&eset);
			FD_SET(m_Socket, &eset);
			eset_ptr = &eset;
		}

		const TIMEVAL tval{
			.tv_sec = 0,
			.tv_usec = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(mseconds).count())
		};

		// Get socket status
		const auto ret = select(0, rset_ptr, wset_ptr, eset_ptr, &tval);
		if (ret != SOCKET_ERROR)
		{
			if constexpr (read) m_IOStatus.SetRead(FD_ISSET(m_Socket, rset_ptr));
			if constexpr (write) m_IOStatus.SetWrite(FD_ISSET(m_Socket, wset_ptr));

			if constexpr (exception)
			{
				if (FD_ISSET(m_Socket, eset_ptr))
				{
					m_IOStatus.SetException(true);
					m_IOStatus.SetErrorCode(GetError());
				}
			}

			return true;
		}

		return false;
	}

	bool Socket::UpdateIOStatus(const std::chrono::milliseconds& mseconds, const IOStatus::Update ioupdate) noexcept
	{
		switch (ioupdate)
		{
			case IOStatus::Update::All:
				return UpdateIOStatusImpl<true, true, true>(mseconds);
			case (IOStatus::Update::Read | IOStatus::Update::Exception):
				return UpdateIOStatusImpl<true, false, true>(mseconds);
			case (IOStatus::Update::Write | IOStatus::Update::Exception):
				return UpdateIOStatusImpl<false, true, true>(mseconds);
			case (IOStatus::Update::Read | IOStatus::Update::Write):
				return UpdateIOStatusImpl<true, true, false>(mseconds);
			case IOStatus::Update::Read:
				return UpdateIOStatusImpl<true, false, false>(mseconds);
			case IOStatus::Update::Write:
				return UpdateIOStatusImpl<false, true, false>(mseconds);
			case IOStatus::Update::Exception:
				return UpdateIOStatusImpl<false, false, true>(mseconds);
			default:
				assert(false);
				break;
		}

		return false;
	}

	SystemTime Socket::GetConnectedTime() const noexcept
	{
		const auto dif = std::chrono::duration_cast<std::chrono::seconds>(Util::GetCurrentSteadyTime() -
																		  GetConnectedSteadyTime());
		return (Util::GetCurrentSystemTime() - dif);
	}

	bool Socket::SockAddrGetIPEndpoint(const sockaddr_storage* addr, IPEndpoint& endpoint) noexcept
	{
		assert(addr != nullptr);

		try
		{
			const IPAddress ip(addr);
			switch (ip.GetFamily())
			{
				case IPAddress::Family::IPv4:
					endpoint = IPEndpoint(ip, ntohs(reinterpret_cast<const sockaddr_in*>(addr)->sin_port));
					return true;
				case IPAddress::Family::IPv6:
					endpoint = IPEndpoint(ip, ntohs(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port));
					return true;
				default:
					assert(false);
					break;
			}
		}
		catch (...) {}

		return false;
	}

	bool Socket::SockAddrSetEndpoint(sockaddr_storage& addr, const IPEndpoint& endpoint) noexcept
	{
		switch (endpoint.GetIPAddress().GetFamily())
		{
			case IPAddress::Family::IPv4:
			{
				auto* saddr = reinterpret_cast<sockaddr_in*>(&addr);
				saddr->sin_port = htons(static_cast<UShort>(endpoint.GetPort()));
				saddr->sin_family = AF_INET;
				saddr->sin_addr.s_addr = endpoint.GetIPAddress().GetBinary().UInt32s[0];
				return true;
			}
			case IPAddress::Family::IPv6:
			{
				auto* saddr = reinterpret_cast<sockaddr_in6*>(&addr);
				saddr->sin6_port = htons(static_cast<UShort>(endpoint.GetPort()));
				saddr->sin6_family = AF_INET6;
				saddr->sin6_flowinfo = 0;
				saddr->sin6_scope_id = 0;

				static_assert(sizeof(endpoint.GetIPAddress().GetBinary().Bytes) >= sizeof(in6_addr),
							  "IP Address length mismatch");

				std::memcpy(&saddr->sin6_addr, &endpoint.GetIPAddress().GetBinary().Bytes, sizeof(in6_addr));
				return true;
			}
			default:
			{
				assert(false);
				break;
			}
		}

		return false;
	}

	IP::AddressFamily Socket::GetAddressFamily() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		WSAPROTOCOL_INFO info{ 0 };
		int len = sizeof(WSAPROTOCOL_INFO);

		if (getsockopt(m_Socket, SOL_SOCKET, SO_PROTOCOL_INFO, reinterpret_cast<char*>(&info), &len) != SOCKET_ERROR)
		{
			switch (info.iAddressFamily)
			{
				case AF_INET:
					return IP::AddressFamily::IPv4;
				case AF_INET6:
					return IP::AddressFamily::IPv6;
				default:
					assert(false);
					break;
			}
		}
		else LogDbg(L"getsockopt() failed for option %d (%s)", SO_PROTOCOL_INFO, GetLastSocketErrorString().c_str());

		return IP::AddressFamily::Unspecified;
	}

	IP::Protocol Socket::GetProtocol() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		WSAPROTOCOL_INFO info{ 0 };
		int len = sizeof(WSAPROTOCOL_INFO);

		if (getsockopt(m_Socket, SOL_SOCKET, SO_PROTOCOL_INFO, reinterpret_cast<char*>(&info), &len) != SOCKET_ERROR)
		{
			switch (info.iProtocol)
			{
				case IPPROTO_TCP:
					return IP::Protocol::TCP;
				case IPPROTO_UDP:
					return IP::Protocol::UDP;
				case IPPROTO_ICMP:
					return IP::Protocol::ICMP;
				case IPPROTO_IP:
					return IP::Protocol::Unspecified;
				default:
					assert(false);
					break;
			}
		}
		else LogDbg(L"getsockopt() failed for option %d (%s)", SO_PROTOCOL_INFO, GetLastSocketErrorString().c_str());

		return IP::Protocol::Unspecified;
	}

	Socket::Type Socket::GetType() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		switch (GetSockOptInt(SO_TYPE))
		{
			case SOCK_STREAM:
				return Type::Stream;
			case SOCK_DGRAM:
				return Type::Datagram;
			case SOCK_RAW:
				return Type::RAW;
			default:
				assert(false);
				break;
		}

		return Type::Unspecified;
	}

	int Socket::GetMaxDatagramMessageSize() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const auto size = GetSockOptInt(SO_MAX_MSG_SIZE);
		if (size != SOCKET_ERROR) return size;
		return 0;
	}

	int Socket::GetSendBufferSize() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const auto size = GetSockOptInt(SO_SNDBUF);
		if (size != SOCKET_ERROR) return size;
		return 0;
	}

	int Socket::GetReceiveBufferSize() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const auto size = GetSockOptInt(SO_RCVBUF);
		if (size != SOCKET_ERROR) return size;
		return 0;
	}

	int Socket::GetError() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		return GetSockOptInt(SO_ERROR);
	}

	int Socket::GetSockOptInt(const int optname) const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		int value = 0;
		int value_len = sizeof(int);

		if (getsockopt(m_Socket, SOL_SOCKET, optname, reinterpret_cast<char*>(&value), &value_len) != SOCKET_ERROR)
		{
			return value;
		}
		else LogDbg(L"getsockopt() failed for option %d (%s)", optname, GetLastSocketErrorString().c_str());

		return SOCKET_ERROR;
	}
}