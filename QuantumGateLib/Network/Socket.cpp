// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Socket.h"

#include <ws2tcpip.h>
#include <Mstcpip.h>

using namespace std::literals;

namespace QuantumGate::Implementation::Network
{
	Socket::Socket() noexcept
	{}

	Socket::Socket(const IPAddressFamily af, const Int32 type, const Int32 protocol) noexcept : Socket()
	{
		auto saf = AF_UNSPEC;
		if (af == IPAddressFamily::IPv4) saf = AF_INET;
		else if (af == IPAddressFamily::IPv6) saf = AF_INET6;
		else
		{
			LogErr(L"Couldn't create socket - Unsupported address family %d", af);
			return;
		}
		
		const auto s = socket(saf, type, protocol);
		if (s != INVALID_SOCKET)
		{
			if (SetSocket(s)) UpdateSocketInfo();
		}
		else
		{
			LogErr(L"Couldn't create socket (%s)", GetLastSysErrorString().c_str());
		}
	}

	Socket::Socket(SOCKET s) noexcept : Socket()
	{
		if (SetSocket(s)) UpdateSocketInfo();
	}

	Socket::~Socket()
	{
		if (m_IOStatus.IsOpen()) Close();
	}

	Socket::Socket(Socket&& other) noexcept
	{
		*this = std::move(other);
	}

	Socket& Socket::operator=(Socket&& other) noexcept
	{
		// Check for same object
		if (this == &other) return *this;

		m_Socket = std::exchange(other.m_Socket, INVALID_SOCKET);
		m_IOStatus = std::exchange(other.m_IOStatus, SocketIOStatus());

		m_BytesReceived = std::exchange(other.m_BytesReceived, 0);
		m_BytesSent = std::exchange(other.m_BytesSent, 0);

		m_LocalEndpoint = std::move(other.m_LocalEndpoint);
		m_PeerEndpoint = std::move(other.m_PeerEndpoint);
		m_AddressFamily = std::exchange(other.m_AddressFamily, IPAddressFamily::Unknown);
		m_Protocol = std::exchange(other.m_Protocol, 0);
		m_Type = std::exchange(other.m_Type, 0);
		m_MaxDGramMsgSize = std::exchange(other.m_MaxDGramMsgSize, 0);

		m_ConnectedSteadyTime = std::exchange(other.m_ConnectedSteadyTime, SteadyTime{});

		return *this;
	}
	
	const bool Socket::SetSocket(const SOCKET s, const bool excl_addr_use, const bool blocking) noexcept
	{
		if (s != INVALID_SOCKET)
		{
			m_Socket = s;
			m_IOStatus.SetOpen(true);

			if (excl_addr_use)
			{
				// Enable exclusive address use for added security to prevent port hijacking
				// Docs: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740621(v=vs.85).aspx
				SockOptSetExclusiveAddressUse(true);
			}

			if (!blocking) SockOptSetBlockingMode(false);

			return true;
		}
		else m_IOStatus.SetOpen(false);

		return false;
	}

	void Socket::Close(const bool linger) noexcept
	{
		m_OnCloseCallback();

		// If we're supposed to abort the connection, set the linger value on the socket to 0,
		// else keep connection alive for a few seconds to give time for shutdown
		if (linger) SockOptSetLinger(Socket::LingerTime);
		else SockOptSetLinger(0s);

		closesocket(m_Socket);

		m_IOStatus.Reset();
	}

	void Socket::UpdateSocketInfo() noexcept
	{
		m_ConnectedSteadyTime = Util::GetCurrentSteadyTime();

		m_AddressFamily = SockOptGetAddressFamily(m_Socket);
		m_Protocol = SockOptGetProtocol(m_Socket);
		m_Type = SockOptGetType(m_Socket);
		m_MaxDGramMsgSize = SockOptGetMaxDGramMsgSize(m_Socket);

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

	const bool Socket::SockOptSetBlockingMode(const bool blocking) noexcept
	{
		ULong mode = blocking ? 0 : 1;

		const auto ret = ioctlsocket(m_Socket, FIONBIO, &mode);
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set socket blocking mode for endpoint %s (%s)",
				   GetLocalName().c_str(), GetLastSysErrorString().c_str());

			return false;
		}

		return true;
	}

	const bool Socket::SockOptSetExclusiveAddressUse(const bool exclusive) noexcept
	{
		const int optval = exclusive ? 1 : 0;

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
									reinterpret_cast<const char*>(&optval), sizeof(optval));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set exclusive address use for socket (%s)", GetLastSysErrorString().c_str());
			return false;
		}

		return true;
	}

	const bool Socket::SockOptSetReuseAddress(const bool reuse) noexcept
	{
		const int optval = reuse ? 1 : 0;

		const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR,
									reinterpret_cast<const char*>(&optval), sizeof(optval));
		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set reuse address socket option for socket (%s)", GetLastSysErrorString().c_str());
			return false;
		}

		return true;
	}

	const bool Socket::SockOptSetLinger(const std::chrono::seconds& seconds) noexcept
	{
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
				   GetLocalName().c_str(), GetLastSysErrorString().c_str());

			return false;
		}

		return true;
	}

	const bool Socket::Listen(const IPEndpoint& endpoint, const bool cond_accept,
							  const bool nat_traversal) noexcept
	{
		// Enable conditional accept (in order to check IP access settings before allowing connection)
		// Docs: https://msdn.microsoft.com/en-us/library/windows/desktop/dd264794(v=vs.85).aspx
		if (cond_accept)
		{
			const DWORD ca = 1;

			const auto ret = setsockopt(m_Socket, SOL_SOCKET, SO_CONDITIONAL_ACCEPT, 
										reinterpret_cast<const char*>(&ca), sizeof(ca));
			if (ret == SOCKET_ERROR)
			{
				LogErr(L"Could not set conditional accept socket option for endpoint %s (%s)", 
					   endpoint.GetString().c_str(), GetLastSysErrorString().c_str());

				return false;
			}
		}

		// Enable NAT traversal (in order to accept connections from the Internet on a LAN)
		// Docs: https://msdn.microsoft.com/en-us/library/windows/desktop/aa832668(v=vs.85).aspx
		if (nat_traversal)
		{
			const int pl = PROTECTION_LEVEL_UNRESTRICTED;

			const auto ret = setsockopt(m_Socket, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL,
										reinterpret_cast<const char*>(&pl), sizeof(pl));
			if (ret == SOCKET_ERROR)
			{
				LogErr(L"Could not set IPV6 protection level for endpoint %s (%s)", 
					   endpoint.GetString().c_str(), GetLastSysErrorString().c_str());

				return false;
			}
		}

		sockaddr_storage saddr{ 0 };

		if (SockAddrFill(saddr, endpoint))
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
						   endpoint.GetString().c_str(), GetLastSysErrorString().c_str());
				}
			}
			else
			{
				LogErr(L"bind() error for endpoint %s (%s)",
					   endpoint.GetString().c_str(), GetLastSysErrorString().c_str());
			}
		}
		else
		{
			LogErr(L"Endpoint %s not supported or not correct (%s)",
				   endpoint.GetString().c_str(), GetLastSysErrorString().c_str());
		}

		return false;
	}

	const bool Socket::Accept(Socket& s, const bool cond_accept, const LPCONDITIONPROC cond_func, void* cbdata) noexcept
	{
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

				s.m_OnAcceptCallback();

				return s.m_OnConnectCallback();
			}
		}
		else
		{
			LogErr(L"A connection could not be accepted on endpoint %s (%s)",
				   GetLocalName().c_str(), GetLastSysErrorString().c_str());
		}

		return false;
	}

	const bool Socket::BeginConnect(const IPEndpoint& endpoint) noexcept
	{
		m_IOStatus.SetConnecting(false);

		sockaddr_storage saddr{ 0 };

		if (SockAddrFill(saddr, endpoint))
		{
			const auto ret = connect(m_Socket, reinterpret_cast<sockaddr*>(&saddr), sizeof(sockaddr_storage));
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
			{
				LogErr(L"Error connecting to endpoint %s (%s)",
					   endpoint.GetString().c_str(), GetLastSysErrorString().c_str());
			}
			else
			{
				// While connection attempt succeeded, this doesn't mean a connection was established.
				// A later call to select() to check if the socket is writable will determine if 
				// the connection was established. If the socket is successfully connected 
				// call CompleteConnect().

				m_IOStatus.SetConnecting(true);

				UpdateSocketInfo();

				m_OnConnectingCallback();
			}
		}

		return m_IOStatus.IsConnecting();
	}

	const bool Socket::CompleteConnect() noexcept
	{
		m_IOStatus.SetConnecting(false);
		m_IOStatus.SetConnected(true);

		UpdateSocketInfo();

		return m_OnConnectCallback();
	}

	const bool Socket::Send(Buffer& buffer) noexcept
	{
		auto success = false;

		auto len = buffer.GetSize();

		// Number of bytes to be sent must not exceed the maximum allowable size supported by socket
		if (m_Type == SOCK_DGRAM && len > m_MaxDGramMsgSize) len = m_MaxDGramMsgSize;

		const auto bytessent = send(m_Socket, reinterpret_cast<char*>(buffer.GetBytes()), static_cast<int>(len), 0);
		
		Dbg(L"%d bytes sent", bytessent);

		if (bytessent > 0)
		{
			try
			{
				buffer.RemoveFirst(bytessent);

				// Update the total amount of bytes sent
				m_BytesSent += bytessent;

				success = true;
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
					   GetPeerName().c_str(), GetLastSysErrorString().c_str());
				
				success = true;
			}
			else
			{
				LogDbg(L"Send error for endpoint %s (%s)",
					   GetPeerName().c_str(), GetLastSysErrorString().c_str());
			}
		}

		return success;
	}

	const bool Socket::Receive(Buffer& buffer) noexcept
	{
		auto success = false;

		auto& rcvbuf = GetReceiveBuffer();

		const auto bytesrcv = recv(m_Socket, reinterpret_cast<char*>(rcvbuf.GetBytes()),
								   static_cast<int>(rcvbuf.GetSize()), 0);
		
		Dbg(L"%d bytes received", bytesrcv);

		if (bytesrcv > 0)
		{
			try
			{
				buffer += BufferView(rcvbuf.GetBytes(), bytesrcv);

				// Update the total amount of bytes received
				m_BytesReceived += bytesrcv;

				success = true;
			}
			catch (const std::exception& e)
			{
				LogErr(L"Receive exception for endpoint %s: %s", GetPeerName().c_str(), Util::ToStringW(e.what()).c_str());
			}
		}
		else if (bytesrcv == 0)
		{
			LogDbg(L"Connection closed for endpoint %s", GetPeerName().c_str());
		}
		else if (bytesrcv == SOCKET_ERROR)
		{
			const auto error = WSAGetLastError();
			if (error == WSAENOBUFS || error == WSAEWOULDBLOCK)
			{
				// Buffer is temporarily unavailable, we'll try again later
				LogDbg(L"Receive buffer unavailable for endpoint %s (%s)", 
					   GetPeerName().c_str(), GetLastSysErrorString().c_str());

				success = true;
			}
			else
			{
				LogDbg(L"Receive error for endpoint %s (%s)",
					   GetPeerName().c_str(), GetLastSysErrorString().c_str());
			}
		}

		return success;
	}

	const bool Socket::UpdateIOStatus(const std::chrono::milliseconds& mseconds) noexcept
	{
		auto success = false;

		fd_set rset{ 0 }, wset{ 0 }, eset{ 0 };

		FD_ZERO(&rset);
		FD_ZERO(&wset);
		FD_ZERO(&eset);

		FD_SET(m_Socket, &rset);
		FD_SET(m_Socket, &wset);
		FD_SET(m_Socket, &eset);

		TIMEVAL tval;
		tval.tv_sec = 0;
		tval.tv_usec = static_cast<long>(mseconds.count());

		// Get socket status
		const auto ret = select(0, &rset, &wset, &eset, &tval);
		if (ret == 0)
		{
			// Select timed out,
			// no events on socket
			m_IOStatus.SetRead(false);
			m_IOStatus.SetWrite(false);
			m_IOStatus.SetException(false);

			success = true;
		}
		else if (ret != SOCKET_ERROR)
		{
			m_IOStatus.SetRead(FD_ISSET(m_Socket, &rset));
			m_IOStatus.SetWrite(FD_ISSET(m_Socket, &wset));

			if (FD_ISSET(m_Socket, &eset))
			{
				m_IOStatus.SetException(true);
				m_IOStatus.SetErrorCode(SockOptGetError(m_Socket));
			}

			success = true;
		}

		return success;
	}

	const SystemTime Socket::GetConnectedTime() const noexcept
	{
		const auto dif = std::chrono::duration_cast<std::chrono::seconds>(Util::GetCurrentSteadyTime() -
																		  GetConnectedSteadyTime());
		return (Util::GetCurrentSystemTime() - dif);
	}

	const bool Socket::SockAddrGetIPEndpoint(const sockaddr_storage* addr, IPEndpoint& endpoint) noexcept
	{
		assert(addr != nullptr);

		auto success = false;

		IPAddress ip(addr);
		if (ip.GetFamily() == IPAddressFamily::IPv4)
		{
			endpoint = IPEndpoint(ip, ntohs(reinterpret_cast<const sockaddr_in*>(addr)->sin_port));

			success = true;
		}
		else if (ip.GetFamily() == IPAddressFamily::IPv6)
		{
			endpoint = IPEndpoint(ip, ntohs(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port));

			success = true;
		}
		else assert(false);

		return success;
	}

	const bool Socket::SockAddrFill(sockaddr_storage& addr, const IPEndpoint& endpoint) noexcept
	{
		if (endpoint.GetIPAddress().GetFamily() == IPAddressFamily::IPv4)
		{
			auto* saddr = reinterpret_cast<sockaddr_in*>(&addr);
			saddr->sin_port = htons(static_cast<UShort>(endpoint.GetPort()));
			saddr->sin_family = AF_INET;
			saddr->sin_addr.s_addr = endpoint.GetIPAddress().GetBinary().UInt32s[0];
			return true;
		}
		else if (endpoint.GetIPAddress().GetFamily() == IPAddressFamily::IPv6)
		{
			auto* saddr = reinterpret_cast<sockaddr_in6*>(&addr);
			saddr->sin6_port = htons(static_cast<UShort>(endpoint.GetPort()));
			saddr->sin6_family = AF_INET6;
			saddr->sin6_flowinfo = 0;
			saddr->sin6_scope_id = 0;

			static_assert(sizeof(endpoint.GetIPAddress().GetBinary().Bytes) >= sizeof(in6_addr),
						  "IP Address length mismatch");

			memcpy(&saddr->sin6_addr, &endpoint.GetIPAddress().GetBinary().Bytes, sizeof(in6_addr));
			return true;
		}
		else LogErr(L"Unsupported IP address family %d", endpoint.GetIPAddress().GetFamily());

		return false;
	}

	IPAddressFamily Socket::SockOptGetAddressFamily(const SOCKET s) noexcept
	{
		WSAPROTOCOL_INFO info{ 0 };
		int len = sizeof(WSAPROTOCOL_INFO);

		if (getsockopt(s, SOL_SOCKET, SO_PROTOCOL_INFO, reinterpret_cast<char*>(&info), &len) != SOCKET_ERROR)
		{
			if (info.iAddressFamily == AF_INET) return IPAddressFamily::IPv4;
			else if (info.iAddressFamily == AF_INET6) return IPAddressFamily::IPv6;
		}
		else LogDbg(L"SockOptGetAddressFamily failed (%s)", GetLastSysErrorString().c_str());

		return IPAddressFamily::Unknown;
	}

	const int Socket::SockOptGetProtocol(const SOCKET s) noexcept
	{
		WSAPROTOCOL_INFO info{ 0 };
		int len = sizeof(WSAPROTOCOL_INFO);

		if (getsockopt(s, SOL_SOCKET, SO_PROTOCOL_INFO, reinterpret_cast<char*>(&info), &len) != SOCKET_ERROR)
		{
			return info.iProtocol;
		}
		else LogDbg(L"SockOptGetProtocol failed (%s)", GetLastSysErrorString().c_str());

		return SOCKET_ERROR;
	}

	const int Socket::SockOptGetType(const SOCKET s) noexcept
	{
		return SockOptGetInt(s, SO_TYPE);
	}

	const int Socket::SockOptGetMaxDGramMsgSize(const SOCKET s) noexcept
	{
		return SockOptGetInt(s, SO_MAX_MSG_SIZE);
	}

	const int Socket::SockOptGetSendBufferSize(const SOCKET s) noexcept
	{
		return SockOptGetInt(s, SO_SNDBUF);
	}

	const int Socket::SockOptGetReceiveBufferSize(const SOCKET s) noexcept
	{
		return SockOptGetInt(s, SO_RCVBUF);
	}

	const int Socket::SockOptGetExclusiveAddressUse(const SOCKET s) noexcept
	{
		return SockOptGetInt(s, SO_EXCLUSIVEADDRUSE);
	}

	const int Socket::SockOptGetError(const SOCKET s) noexcept
	{
		return SockOptGetInt(s, SO_ERROR);
	}

	const int Socket::SockOptGetInt(const SOCKET s, const int optname) noexcept
	{
		int error = 0;
		int len = sizeof(int);

		if (getsockopt(s, SOL_SOCKET, optname, reinterpret_cast<char*>(&error), &len) != SOCKET_ERROR)
		{
			return error;
		}
		else LogDbg(L"getsockopt failed for option %d (%s)", optname, GetLastSysErrorString().c_str());

		return SOCKET_ERROR;
	}
}