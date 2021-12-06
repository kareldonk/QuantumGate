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

	Socket::Socket(const AddressFamily af, const Type type, const Protocol protocol) : Socket()
	{
		int saf{ 0 };
		int stype{ 0 };
		int sprotocol{ 0 };

		switch (af)
		{
			case AddressFamily::Unspecified:
				saf = AF_UNSPEC;
				break;
			case AddressFamily::IPv4:
				saf = AF_INET;
				break;
			case AddressFamily::IPv6:
				saf = AF_INET6;
				break;
			case AddressFamily::BTH:
				saf = AF_BTH;
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
			case Protocol::Unspecified:
				sprotocol = IPPROTO_IP;
				break;
			case Protocol::TCP:
				sprotocol = IPPROTO_TCP;
				break;
			case Protocol::UDP:
				sprotocol = IPPROTO_UDP;
				break;
			case Protocol::ICMP:
				sprotocol = IPPROTO_ICMP;
				break;
			case Protocol::BTH:
				sprotocol = BTHPROTO_RFCOMM;
				break;
			default:
				throw SocketException("Unsupported protocol");
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

	Socket::Socket(const IP::AddressFamily af, const Type type, const IP::Protocol protocol) :
		Socket(IP::AddressFamilyToNetwork(af), type, IP::ProtocolToNetwork(protocol))
	{}

	Socket::Socket(const BTH::AddressFamily af, const Type type, const BTH::Protocol protocol) :
		Socket(BTH::AddressFamilyToNetwork(af), type, BTH::ProtocolToNetwork(protocol))
	{}

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

		if (GetProtocol() == Protocol::TCP)
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

		switch (GetProtocol())
		{
			case Protocol::TCP:
			{
				// If we're supposed to abort the connection, set the linger value on the socket to 0,
				// else keep connection alive for a few seconds to give time for shutdown
				if (linger) DiscardReturnValue(SetLinger(Socket::DefaultLingerTime));
				else DiscardReturnValue(SetLinger(0s));
				break;
			}
			case Protocol::BTH:
			{
				shutdown(m_Socket, SD_BOTH);
				break;
			}
			default:
			{
				break;
			}
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
		const auto addr_len = std::invoke([&]() noexcept
		{
			return (GetAddressFamily() == AddressFamily::BTH) ? sizeof(SOCKADDR_BTH) : sizeof(sockaddr_storage);
		});

		int len = static_cast<int>(addr_len);

		// This will only work for connected or bound (listener) sockets
		auto err = getsockname(m_Socket, reinterpret_cast<sockaddr*>(&addr), &len);
		if (err != SOCKET_ERROR)
		{
			if (!SockAddrGetEndpoint(GetProtocol(), &addr, m_LocalEndpoint))
			{
				LogErr(L"Could not get local endpoint for socket");
			}

			len = static_cast<int>(addr_len);

			err = getpeername(m_Socket, reinterpret_cast<sockaddr*>(&addr), &len);
			if (err != SOCKET_ERROR)
			{
				if (!SockAddrGetEndpoint(GetProtocol(), &addr, m_PeerEndpoint))
				{
					LogErr(L"Could not get peer endpoint for socket");
				}
			}
		}
	}

	Socket::ReceiveBuffer& Socket::GetReceiveBuffer() const noexcept
	{
		static thread_local ReceiveBuffer rcvbuf{ ReceiveBuffer::GetMaxSize() };
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

	Result<bool> Socket::GetExclusiveAddressUse() const noexcept
	{
		const auto val = GetSockOptInt(SO_EXCLUSIVEADDRUSE);
		if (val != SOCKET_ERROR)
		{
			return (val == 1) ? true : false;
		}

		return std::error_code(WSAGetLastError(), std::system_category());
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

	Result<bool> Socket::GetNATTraversal() noexcept
	{
		const auto pl = GetOptInt(IPPROTO_IPV6, IPV6_PROTECTION_LEVEL);
		if (pl == PROTECTION_LEVEL_UNRESTRICTED)
		{
			return false;
		}

		return false;
	}

	bool Socket::SetBluetoothAuthentication(const bool bthauth) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		// Enable Bluetooth authentication
		// Docs: https://docs.microsoft.com/en-us/windows/win32/bluetooth/bluetooth-and-socket-options
		const ULONG ba = bthauth ? TRUE : FALSE;

		// Enable/Disable encryption together with authentication
		const auto ret1 = setsockopt(m_Socket, SOL_RFCOMM, SO_BTH_AUTHENTICATE,
									 reinterpret_cast<const char*>(&ba), sizeof(ba));
		const auto ret2 = setsockopt(m_Socket, SOL_RFCOMM, SO_BTH_ENCRYPT,
									 reinterpret_cast<const char*>(&ba), sizeof(ba));
		if (ret1 == SOCKET_ERROR || ret2 == SOCKET_ERROR)
		{
			LogErr(L"Could not set Bluetooth authentication for endpoint %s (%s)",
				   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

			return false;
		}

		return true;
	}

	bool Socket::SetConditionalAccept(const bool cond_accept) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		assert(GetProtocol() == Protocol::TCP);

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
			LogErr(L"Could not set nagle algorithm for endpoint %s (%s)",
					GetLocalName().c_str(), GetLastSocketErrorString().c_str());

			return false;
		}

		return true;
	}

	bool Socket::SetMTUDiscovery(const bool enabled) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		// Sets MTU discovery
		// Docs: https://docs.microsoft.com/en-us/windows/win32/winsock/ipproto-ip-socket-options
		const DWORD popt = enabled ? IP_PMTUDISC_PROBE : IP_PMTUDISC_NOT_SET;
		int ret{ SOCKET_ERROR };

		const auto af = GetAddressFamily();
		if (af == AddressFamily::IPv4)
		{
			ret = setsockopt(m_Socket, IPPROTO_IP, IP_MTU_DISCOVER, reinterpret_cast<const char*>(&popt), sizeof(popt));
		}
		else if (af == AddressFamily::IPv6)
		{
			ret = setsockopt(m_Socket, IPPROTO_IPV6, IPV6_MTU_DISCOVER, reinterpret_cast<const char*>(&popt), sizeof(popt));
		}

		if (ret == SOCKET_ERROR)
		{
			LogErr(L"Could not set MTU discovery option for endpoint %s (%s)",
				   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

			return false;
		}

		return true;
	}

	Result<bool> Socket::IsMTUDiscoveryEnabled() noexcept
	{
		DWORD popt{ 0 };
		int popt_len = sizeof(DWORD);

		const auto af = GetAddressFamily();
		if (af == AddressFamily::IPv4)
		{
			if (getsockopt(m_Socket, IPPROTO_IP, IP_MTU_DISCOVER, reinterpret_cast<char*>(&popt), &popt_len) != SOCKET_ERROR)
			{
				if (popt == IP_PMTUDISC_PROBE || popt == IP_PMTUDISC_DO) return true;
				else return false;
			}
		}
		else if (af == AddressFamily::IPv6)
		{
			if (getsockopt(m_Socket, IPPROTO_IPV6, IPV6_MTU_DISCOVER, reinterpret_cast<char*>(&popt), &popt_len) != SOCKET_ERROR)
			{
				if (popt == IP_PMTUDISC_PROBE || popt == IP_PMTUDISC_DO) return true;
				else return false;
			}
		}
		else return false;

		return std::error_code(WSAGetLastError(), std::system_category());
	}

	bool Socket::Bind(const Endpoint& endpoint, const bool nat_traversal) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		assert(GetProtocol() == Protocol::ICMP || GetProtocol() == Protocol::UDP);
		DbgInvoke([&]()
		{
			if (endpoint.GetType() == Endpoint::Type::IP)
			{
				assert(endpoint.GetIPEndpoint().GetProtocol() == IPEndpoint::Protocol::ICMP ||
					   endpoint.GetIPEndpoint().GetProtocol() == IPEndpoint::Protocol::UDP);
			}
		});

		if (!SetNATTraversal(nat_traversal)) return false;

		sockaddr_storage saddr{ 0 };

		if (SockAddrSetEndpoint(saddr, endpoint))
		{
			// Bind our name to the socket
			const auto ret = bind(m_Socket, reinterpret_cast<sockaddr*>(&saddr), sizeof(sockaddr_storage));
			if (ret != SOCKET_ERROR)
			{
				m_IOStatus.SetBound(true);
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

	bool Socket::Listen(const Endpoint& endpoint) noexcept
	{
		return Listen(endpoint, false, false);
	}

	bool Socket::Listen(const Endpoint& endpoint, const bool cond_accept, const bool nat_traversal) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		DbgInvoke([&]()
		{
			if (endpoint.GetType() == Endpoint::Type::IP)
			{
				assert(endpoint.GetIPEndpoint().GetProtocol() == IPEndpoint::Protocol::TCP);
			}
			else if (endpoint.GetType() == Endpoint::Type::BTH)
			{
				assert(endpoint.GetBTHEndpoint().GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			}
		});

		if (endpoint.GetType() == Endpoint::Type::IP)
		{
			if (!SetConditionalAccept(cond_accept)) return false;

			if (!SetNATTraversal(nat_traversal)) return false;
		}

		sockaddr_storage saddr{ 0 };

		if (SockAddrSetEndpoint(saddr, endpoint))
		{
			m_LocalEndpoint = endpoint;

			const auto saddr_len = std::invoke([&]() noexcept
			{
				return (endpoint.GetType() == Endpoint::Type::BTH) ? sizeof(SOCKADDR_BTH) : sizeof(sockaddr_storage);
			});

			// Bind our name to the socket
			auto ret = bind(m_Socket, reinterpret_cast<sockaddr*>(&saddr), sizeof(SOCKADDR_BTH));
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

	bool Socket::SetService(const WChar* service_name, const WChar* service_comment, const GUID& guid,
							const ServiceOperation op) noexcept
	{
		if (m_LocalEndpoint.GetType() == Endpoint::Type::BTH)
		{
			SOCKADDR_BTH laddr{ 0 };
			laddr.addressFamily = AF_BTH;
			laddr.btAddr = m_LocalEndpoint.GetBTHEndpoint().GetBTHAddress().GetBinary().UInt64s;

			if (m_LocalEndpoint.GetBTHEndpoint().GetPort() == 0)
			{
				laddr.port = BT_PORT_ANY;
			}
			else laddr.port = m_LocalEndpoint.GetBTHEndpoint().GetPort();

			laddr.serviceClassId = guid;

			CSADDR_INFO addrinfo{ 0 };
			addrinfo.iProtocol = BTHPROTO_RFCOMM;
			addrinfo.iSocketType = SOCK_STREAM;
			addrinfo.LocalAddr.iSockaddrLength = sizeof(SOCKADDR_BTH);
			addrinfo.LocalAddr.lpSockaddr = reinterpret_cast<sockaddr*>(&laddr);
			addrinfo.RemoteAddr.iSockaddrLength = sizeof(SOCKADDR_BTH);
			addrinfo.RemoteAddr.lpSockaddr = reinterpret_cast<sockaddr*>(&laddr);

			WSAQUERYSET wsaset{ 0 };
			wsaset.dwSize = sizeof(WSAQUERYSET);
			wsaset.lpServiceClassId = const_cast<GUID*>(&guid);
			wsaset.lpszServiceInstanceName = const_cast<WChar*>(service_name);
			wsaset.lpszComment = const_cast<WChar*>(service_comment);
			wsaset.dwNameSpace = NS_BTH;
			wsaset.dwNumberOfCsAddrs = 1;
			wsaset.lpcsaBuffer = &addrinfo;

			auto essop{ WSAESETSERVICEOP::RNRSERVICE_REGISTER };
			switch (op)
			{
				case ServiceOperation::Register:
					essop = WSAESETSERVICEOP::RNRSERVICE_REGISTER;
					break;
				case ServiceOperation::Delete:
					essop = WSAESETSERVICEOP::RNRSERVICE_DELETE;
					break;
				default:
					return false;
			}

			const auto ret = WSASetService(&wsaset, essop, 0);
			if (ret == SOCKET_ERROR)
			{
				LogErr(L"WSASetService() error for endpoint %s (%s)",
					   m_LocalEndpoint.GetString().c_str(), GetLastSocketErrorString().c_str());
				return false;
			}
		}

		return true;
	}

	bool Socket::Accept(Socket& s, const bool cond_accept, const LPCONDITIONPROC cond_func, void* cbdata) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		assert(GetProtocol() == Protocol::TCP || GetProtocol() == Protocol::BTH);

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
			const auto error = WSAGetLastError();
			if (error != WSAEWOULDBLOCK)
			{
				LogErr(L"A connection could not be accepted on endpoint %s (%s)",
					   GetLocalName().c_str(), GetLastSocketErrorString().c_str());
			}
		}

		return false;
	}

	bool Socket::BeginConnect(const Endpoint& endpoint) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		assert(GetProtocol() == Protocol::TCP || GetProtocol() == Protocol::BTH);
		DbgInvoke([&]()
		{
			if (endpoint.GetType() == Endpoint::Type::IP)
			{
				assert(endpoint.GetIPEndpoint().GetProtocol() == IPEndpoint::Protocol::TCP);
			}
			else if (endpoint.GetType() == Endpoint::Type::BTH)
			{
				assert(endpoint.GetBTHEndpoint().GetProtocol() == BTHEndpoint::Protocol::RFCOMM);
			}
		});

		m_IOStatus.SetConnecting(false);

		sockaddr_storage saddr{ 0 };

		if (SockAddrSetEndpoint(saddr, endpoint))
		{
			m_PeerEndpoint = endpoint;

			const auto saddr_len = std::invoke([&]() noexcept
			{
				return (endpoint.GetType() == Endpoint::Type::BTH) ? sizeof(SOCKADDR_BTH) : sizeof(sockaddr_storage);
			});

			const auto ret = connect(m_Socket, reinterpret_cast<sockaddr*>(&saddr), static_cast<int>(saddr_len));
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
			{
				const auto error_code = WSAGetLastError();
				auto error_ex = GetExtendedErrorString(error_code);
				LogErr(L"Error connecting to endpoint %s (%s%s%s)",
					   endpoint.GetString().c_str(), GetSocketErrorString(error_code).c_str(),
					   error_ex == L"" ? L"" : L" ", error_ex);
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
		assert(GetProtocol() == Protocol::TCP || GetProtocol() == Protocol::BTH);

		m_IOStatus.SetConnecting(false);
		m_IOStatus.SetConnected(true);

		UpdateSocketInfo();

		return m_ConnectCallback();
	}

	Result<Size> Socket::Send(const BufferView& buffer, const Size max_snd_size) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		assert(GetProtocol() == Protocol::TCP || GetProtocol() == Protocol::BTH);

		const auto send_size = std::invoke([&]()
		{
			if (max_snd_size > 0 && max_snd_size < buffer.GetSize()) return max_snd_size;
			else return buffer.GetSize();
		});

		const auto bytessent = send(m_Socket, reinterpret_cast<const char*>(buffer.GetBytes()),
									static_cast<int>(send_size), 0);

		Dbg(L"%d bytes sent", bytessent);

		if (bytessent >= 0)
		{
			// Update the total amount of bytes sent
			m_BytesSent += bytessent;

			return bytessent;
		}
		else if (bytessent == SOCKET_ERROR)
		{
			const auto error = WSAGetLastError();
			if (error == WSAENOBUFS || error == WSAEWOULDBLOCK)
			{
				// Send buffer is full or temporarily unavailable, we'll try again later
				LogDbg(L"Send buffer full/unavailable for endpoint %s (%s)",
					   GetPeerName().c_str(), GetLastSocketErrorString().c_str());

				return 0;
			}
			else
			{
				LogDbg(L"Send error for endpoint %s (%s)",
					   GetPeerName().c_str(), GetLastSocketErrorString().c_str());

				return std::error_code(error, std::system_category());
			}
		}

		return ResultCode::Failed;
	}

	Result<Size> Socket::SendTo(const Endpoint& endpoint, const BufferView& buffer, const Size max_snd_size) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		assert(GetProtocol() == Protocol::ICMP || GetProtocol() == Protocol::UDP);
		DbgInvoke([&]()
		{
			if (endpoint.GetType() == Endpoint::Type::IP)
			{
				assert(endpoint.GetIPEndpoint().GetProtocol() == IPEndpoint::Protocol::ICMP ||
					   endpoint.GetIPEndpoint().GetProtocol() == IPEndpoint::Protocol::UDP);
			}
		});

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

		DbgInvoke([&]()
		{
			if (GetType() == Type::Datagram)
			{
				const auto result = GetMaxDatagramMessageSize();
				assert(result && send_size <= static_cast<Size>(*result));
			}
		});

		const auto bytessent = sendto(m_Socket, reinterpret_cast<const char*>(buffer.GetBytes()),
									  static_cast<int>(send_size), 0,
									  reinterpret_cast<sockaddr*>(&sock_addr), sizeof(sock_addr));

		Dbg(L"%d bytes sent", bytessent);

		if (bytessent >= 0)
		{
			// Update the total amount of bytes sent
			m_BytesSent += bytessent;

			if (!m_IOStatus.IsBound() && GetType() == Type::Datagram)
			{
				m_IOStatus.SetBound(true);
				UpdateSocketInfo();
			}

			return bytessent;
		}
		else if (bytessent == SOCKET_ERROR)
		{
			const auto error = WSAGetLastError();
			if (error == WSAENOBUFS || error == WSAEWOULDBLOCK)
			{
				// Send buffer is full or temporarily unavailable, we'll try again later
				LogDbg(L"Send buffer full/unavailable on endpoint %s (%s)",
					   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

				return 0;
			}
			else
			{
				LogDbg(L"Send error on endpoint %s (%s)",
					   GetLocalName().c_str(), GetLastSocketErrorString().c_str());

				return std::error_code(error, std::system_category());
			}
		}

		return ResultCode::Failed;
	}

	Result<Size> Socket::Receive(Buffer& buffer, const Size max_rcv_size) noexcept
	{
		auto& rcvbuf = GetReceiveBuffer();

		const auto read_size = std::invoke([&]()
		{
			if (max_rcv_size > 0 && max_rcv_size < rcvbuf.GetSize()) return max_rcv_size;
			else return rcvbuf.GetSize();
		});

		auto rcvbuf_span = BufferSpan(rcvbuf.GetBytes(), read_size);

		auto result = Receive(rcvbuf_span);
		if (result.Succeeded() && *result > 0)
		{
			try
			{
				buffer += rcvbuf_span.GetFirst(*result);
			}
			catch (const std::exception& e)
			{
				LogErr(L"Receive exception for endpoint %s: %s", GetPeerName().c_str(), Util::ToStringW(e.what()).c_str());

				return ResultCode::Failed;
			}
		}

		return result;
	}

	Result<Size> Socket::Receive(BufferSpan& buffer) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		assert(GetProtocol() == Protocol::TCP || GetProtocol() == Protocol::BTH);

		const auto bytesrcv = recv(m_Socket, reinterpret_cast<char*>(buffer.GetBytes()), static_cast<int>(buffer.GetSize()), 0);

		Dbg(L"%d bytes received", bytesrcv);

		if (bytesrcv > 0)
		{
			// Update the total amount of bytes received
			m_BytesReceived += bytesrcv;

			return bytesrcv;
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
				// Buffer is temporarily unavailable,
				// or there is no data to receive
				return 0;
			}
			else
			{
				LogDbg(L"Receive error for endpoint %s (%s)",
					   GetPeerName().c_str(), GetLastSocketErrorString().c_str());

				return std::error_code(error, std::system_category());
			}
		}

		return ResultCode::Failed;
	}

	Result<Size> Socket::ReceiveFrom(Endpoint& endpoint, Buffer& buffer, const Size max_rcv_size) noexcept
	{
		auto& rcvbuf = GetReceiveBuffer();

		const auto read_size = std::invoke([&]()
		{
			if (max_rcv_size > 0 && max_rcv_size < rcvbuf.GetSize()) return max_rcv_size;
			else return rcvbuf.GetSize();
		});

		auto rcvbuf_span = BufferSpan(rcvbuf.GetBytes(), read_size);

		auto result = ReceiveFrom(endpoint, rcvbuf_span);
		if (result.Succeeded() && *result > 0)
		{
			try
			{
				buffer += rcvbuf_span.GetFirst(*result);
			}
			catch (const std::exception& e)
			{
				LogErr(L"Receive exception on endpoint %s: %s", GetLocalName().c_str(), Util::ToStringW(e.what()).c_str());
				
				return ResultCode::Failed;
			}
		}

		return result;
	}

	Result<Size> Socket::ReceiveFrom(Endpoint& endpoint, BufferSpan& buffer) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		assert(GetProtocol() == Protocol::ICMP || GetProtocol() == Protocol::UDP);

		sockaddr_storage sock_addr{ 0 };
		int sock_addr_len{ sizeof(sock_addr) };

		const auto bytesrcv = recvfrom(m_Socket, reinterpret_cast<char*>(buffer.GetBytes()), static_cast<int>(buffer.GetSize()),
									   0, reinterpret_cast<sockaddr*>(&sock_addr), &sock_addr_len);

		Dbg(L"%d bytes received", bytesrcv);

		if (sock_addr.ss_family != 0)
		{
			if (!SockAddrGetEndpoint(Protocol::UDP, &sock_addr, endpoint))
			{
				LogDbg(L"Receive error on endpoint %s - SockAddrGetIPEndpoint() failed",
					   GetLocalName().c_str());

				return ResultCode::Failed;
			}
		}

		if (bytesrcv > 0)
		{
			// Update the total amount of bytes received
			m_BytesReceived += bytesrcv;

			return bytesrcv;
		}
		else if (bytesrcv == 0)
		{
			LogDbg(L"Connection closed for endpoint %s", GetLocalName().c_str());
		}
		else if (bytesrcv == SOCKET_ERROR)
		{
			const auto error = WSAGetLastError();
			if (error == WSAENOBUFS || error == WSAEWOULDBLOCK)
			{
				// Buffer is temporarily unavailable,
				// or there is no data to receive
				return 0;
			}
			else 
			{
				if (error == WSAECONNRESET)
				{
					LogDbg(L"Port unreachable for endpoint %s", endpoint.GetString().c_str());
				}
				else
				{
					LogDbg(L"Receive error on endpoint %s (%s)",
						   GetLocalName().c_str(), GetLastSocketErrorString().c_str());
				}

				return std::error_code(error, std::system_category());
			}
		}

		return ResultCode::Failed;
	}

#ifdef USE_SOCKET_EVENT
	bool Socket::UpdateIOStatusEvent(const std::chrono::milliseconds& mseconds) noexcept
	{
		if (mseconds.count() > 0)
		{
			const auto handle = m_Event.GetHandle();
			const auto ret = WSAWaitForMultipleEvents(1, &handle, false, static_cast<DWORD>(mseconds.count()), false);
			if (ret == WSA_WAIT_FAILED) return false;
		}

		WSANETWORKEVENTS events{ 0 };

		const auto ret = WSAEnumNetworkEvents(m_Socket, m_Event.GetHandle(), &events);
		if (ret == SOCKET_ERROR) return false;

		// Behavior below tries to closely match the results a select() would 
		// give in UpdateIOStatusFDSet()

		if (!m_IOStatus.IsClosing()) m_IOStatus.SetClosing(events.lNetworkEvents & FD_CLOSE);

		m_IOStatus.SetRead((events.lNetworkEvents & FD_READ) ||
							(events.lNetworkEvents & FD_ACCEPT) || m_IOStatus.IsClosing());

		if (!m_IOStatus.CanWrite())
		{
			m_IOStatus.SetWrite((events.lNetworkEvents & FD_WRITE && events.iErrorCode[FD_WRITE_BIT] == 0) ||
								(events.lNetworkEvents & FD_CONNECT && events.iErrorCode[FD_CONNECT_BIT] == 0));
		}
		else
		{
			m_IOStatus.SetWrite(!(events.lNetworkEvents & FD_CLOSE ||
								  (events.lNetworkEvents & FD_WRITE && events.iErrorCode[FD_WRITE_BIT] != 0)));
		}

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

		return true;
	}
#endif

	bool Socket::UpdateIOStatusFDSet(const std::chrono::milliseconds& mseconds) noexcept
	{
		fd_set rset{ 0 }, wset{ 0 }, eset{ 0 };
		fd_set* rset_ptr{ nullptr };
		fd_set* wset_ptr{ nullptr };
		fd_set* eset_ptr{ nullptr };

		FD_ZERO(&rset);
		FD_SET(m_Socket, &rset);
		rset_ptr = &rset;

		FD_ZERO(&wset);
		FD_SET(m_Socket, &wset);
		wset_ptr = &wset;

		FD_ZERO(&eset);
		FD_SET(m_Socket, &eset);
		eset_ptr = &eset;

		const TIMEVAL tval{
			.tv_sec = 0,
			.tv_usec = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(mseconds).count())
		};

		// Get socket status
		const auto ret = select(0, rset_ptr, wset_ptr, eset_ptr, &tval);
		if (ret == SOCKET_ERROR) return false;

		m_IOStatus.SetRead(FD_ISSET(m_Socket, rset_ptr));
		m_IOStatus.SetWrite(FD_ISSET(m_Socket, wset_ptr));

		if (FD_ISSET(m_Socket, eset_ptr))
		{
			m_IOStatus.SetException(true);
			m_IOStatus.SetErrorCode(GetError());
		}

		return true;
	}

	bool Socket::UpdateIOStatus(const std::chrono::milliseconds& mseconds) noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

#ifdef USE_SOCKET_EVENT
		return UpdateIOStatusEvent(mseconds);
#else
		return UpdateIOStatusFDSet(mseconds);
#endif
	}

	SystemTime Socket::GetConnectedTime() const noexcept
	{
		const auto dif = std::chrono::duration_cast<std::chrono::seconds>(Util::GetCurrentSteadyTime() -
																		  GetConnectedSteadyTime());
		return (Util::GetCurrentSystemTime() - dif);
	}

	bool Socket::SockAddrGetEndpoint(const Protocol protocol, const sockaddr_storage* addr, Endpoint& endpoint) noexcept
	{
		assert(addr != nullptr);

		try
		{
			switch (protocol)
			{
				case Protocol::ICMP:
				case Protocol::TCP:
				case Protocol::UDP:
				{
					const IPAddress ip(addr);
					switch (ip.GetFamily())
					{
						case IPAddress::Family::IPv4:
							endpoint = IPEndpoint(IP::ProtocolFromNetwork(protocol), ip, ntohs(reinterpret_cast<const sockaddr_in*>(addr)->sin_port));
							return true;
						case IPAddress::Family::IPv6:
							endpoint = IPEndpoint(IP::ProtocolFromNetwork(protocol), ip, ntohs(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port));
							return true;
						default:
							assert(false);
							break;
					}
					break;
				}
				case Protocol::BTH:
				{
					const BTHAddress bth(addr);
					switch (bth.GetFamily())
					{
						case BTHAddress::Family::BTH:
						{
							const auto bthaddr = reinterpret_cast<const SOCKADDR_BTH*>(addr);
							UInt16 port{ 0 };
							if (bthaddr->port != BT_PORT_ANY)
							{
								port = static_cast<UInt16>(bthaddr->port);
							}
							endpoint = BTHEndpoint(BTH::ProtocolFromNetwork(protocol), bth, port, bthaddr->serviceClassId);
							return true;
						}
						default:
						{
							assert(false);
							break;
						}
					}
					break;
				}
				default:
				{
					assert(false);
					break;
				}
			}
		}
		catch (...) {}

		return false;
	}

	bool Socket::SockAddrSetEndpoint(sockaddr_storage& addr, const Endpoint& endpoint) noexcept
	{
		switch (endpoint.GetType())
		{
			case Endpoint::Type::IP:
			{
				const auto& ep = endpoint.GetIPEndpoint();
				const auto& ip = ep.GetIPAddress();
				switch (ip.GetFamily())
				{
					case IPAddress::Family::IPv4:
					{
						auto* saddr = reinterpret_cast<sockaddr_in*>(&addr);
						saddr->sin_port = htons(static_cast<UShort>(ep.GetPort()));
						saddr->sin_family = AF_INET;
						saddr->sin_addr.s_addr = ip.GetBinary().UInt32s[0];
						return true;
					}
					case IPAddress::Family::IPv6:
					{
						auto* saddr = reinterpret_cast<sockaddr_in6*>(&addr);
						saddr->sin6_port = htons(static_cast<UShort>(ep.GetPort()));
						saddr->sin6_family = AF_INET6;
						saddr->sin6_flowinfo = 0;
						saddr->sin6_scope_id = 0;

						static_assert(sizeof(ip.GetBinary().Bytes) >= sizeof(in6_addr), "IP Address length mismatch");

						std::memcpy(&saddr->sin6_addr, &ip.GetBinary().Bytes, sizeof(in6_addr));
						return true;
					}
					default:
					{
						assert(false);
						break;
					}
				}
				break;
			}
			case Endpoint::Type::BTH:
			{
				const auto& ep = endpoint.GetBTHEndpoint();
				const auto& bth = ep.GetBTHAddress();
				switch (bth.GetFamily())
				{
					case BTHAddress::Family::BTH:
					{
						auto* saddr = reinterpret_cast<SOCKADDR_BTH*>(&addr);
						
						if (ep.GetPort() == 0)
						{
							saddr->port = BT_PORT_ANY;
						}
						else saddr->port = ep.GetPort();

						saddr->addressFamily = AF_BTH;
						saddr->btAddr = bth.GetBinary().UInt64s;
						saddr->serviceClassId = ep.GetServiceClassID();
						return true;
					}
					default:
					{
						assert(false);
						break;
					}
				}
				break;
			}
			default:
			{
				assert(false);
				break;
			}
		}

		return false;
	}

	AddressFamily Socket::GetAddressFamily() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		WSAPROTOCOL_INFO info{ 0 };
		int len = sizeof(WSAPROTOCOL_INFO);

		if (getsockopt(m_Socket, SOL_SOCKET, SO_PROTOCOL_INFO, reinterpret_cast<char*>(&info), &len) != SOCKET_ERROR)
		{
			switch (info.iAddressFamily)
			{
				case AF_INET:
					return AddressFamily::IPv4;
				case AF_INET6:
					return AddressFamily::IPv6;
				case AF_BTH:
					return AddressFamily::BTH;
				default:
					assert(false);
					break;
			}
		}
		else LogDbg(L"getsockopt() failed for option %d (%s)", SO_PROTOCOL_INFO, GetLastSocketErrorString().c_str());

		return AddressFamily::Unspecified;
	}

	Protocol Socket::GetProtocol() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		WSAPROTOCOL_INFO info{ 0 };
		int len = sizeof(WSAPROTOCOL_INFO);

		if (getsockopt(m_Socket, SOL_SOCKET, SO_PROTOCOL_INFO, reinterpret_cast<char*>(&info), &len) != SOCKET_ERROR)
		{
			switch (info.iProtocol)
			{
				case IPPROTO_TCP:
					return Protocol::TCP;
				case IPPROTO_UDP:
					return Protocol::UDP;
				case IPPROTO_ICMP:
					return Protocol::ICMP;
				case BTHPROTO_RFCOMM:
					return Protocol::BTH;
				case IPPROTO_IP:
					return Protocol::Unspecified;
				default:
					assert(false);
					break;
			}
		}
		else LogDbg(L"getsockopt() failed for option %d (%s)", SO_PROTOCOL_INFO, GetLastSocketErrorString().c_str());

		return Protocol::Unspecified;
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

	Result<int> Socket::GetMaxDatagramMessageSize() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const auto size = GetSockOptInt(SO_MAX_MSG_SIZE);
		if (size != SOCKET_ERROR) return size;
		
		return std::error_code(WSAGetLastError(), std::system_category());
	}

	Result<int> Socket::GetSendBufferSize() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const auto size = GetSockOptInt(SO_SNDBUF);
		if (size != SOCKET_ERROR) return size;

		return std::error_code(WSAGetLastError(), std::system_category());
	}

	Result<int> Socket::GetReceiveBufferSize() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		const auto size = GetSockOptInt(SO_RCVBUF);
		if (size != SOCKET_ERROR) return size;
		
		return std::error_code(WSAGetLastError(), std::system_category());
	}

	int Socket::GetError() const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);
		return GetSockOptInt(SO_ERROR);
	}

	int Socket::GetSockOptInt(const int optname) const noexcept
	{
		return GetOptInt(SOL_SOCKET, optname);
	}

	int Socket::GetOptInt(const int level, const int optname) const noexcept
	{
		assert(m_Socket != INVALID_SOCKET);

		int value = 0;
		int value_len = sizeof(int);

		if (getsockopt(m_Socket, level, optname, reinterpret_cast<char*>(&value), &value_len) != SOCKET_ERROR)
		{
			return value;
		}
		else LogDbg(L"getsockopt() failed for option %d (%s)", optname, GetLastSocketErrorString().c_str());

		return SOCKET_ERROR;
	}

	const WChar* Socket::GetLastExtendedErrorString() const noexcept
	{
		return GetExtendedErrorString(WSAGetLastError());
	}

	const WChar* Socket::GetExtendedErrorString(const int code) const noexcept
	{
		if (GetAddressFamily() == AddressFamily::BTH)
		{
			switch (code)
			{
				case WSAEINVAL:
				case WSAENETDOWN:
					return L"Make sure Bluetooth is enabled on the local device.";
				case WSAENETUNREACH:
					return L"Check the Bluetooth address of the peer and that the devices are paired if authentication is required.";
				case WSAEHOSTDOWN:
				case WSAETIMEDOUT:
					return L"Make sure Bluetooth is enabled on the remote device.";
				default:
					break;
			}
		}

		return L"";
	}
}