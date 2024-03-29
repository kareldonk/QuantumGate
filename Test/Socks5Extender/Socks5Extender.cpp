// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Socks5Extender.h"

#include <cassert>
#include <regex>

// Windows Sockets includes and library
#include <ws2tcpip.h>
#include <Iphlpapi.h>
#pragma comment(lib, "Ws2_32.lib")

#include "Console.h"
#include "Common\Util.h"
#include "Common\Endian.h"
#include "Common\ScopeGuard.h"
#include "Memory\BufferWriter.h"
#include "Memory\BufferReader.h"
#include "Crypto\Crypto.h"

using namespace QuantumGate::Implementation;
using namespace QuantumGate::Implementation::Memory;
using namespace QuantumGate::Implementation::Network;
using namespace std::literals;

namespace QuantumGate::Socks5Extender
{
	Extender::Extender() noexcept :
		QuantumGate::Extender(UUID, String(L"QuantumGate Socks5 Extender"))
	{
		if (!SetStartupCallback(MakeCallback(this, &Extender::OnStartup)) ||
			!SetPostStartupCallback(MakeCallback(this, &Extender::OnPostStartup)) ||
			!SetPreShutdownCallback(MakeCallback(this, &Extender::OnPreShutdown)) ||
			!SetShutdownCallback(MakeCallback(this, &Extender::OnShutdown)) ||
			!SetPeerEventCallback(MakeCallback(this, &Extender::OnPeerEvent)) ||
			!SetPeerMessageCallback(MakeCallback(this, &Extender::OnPeerMessage)))
		{
			LogErr(L"%s: couldn't set one or more extender callbacks", GetName().c_str());
		}
	}

	Extender::~Extender()
	{}

	void Extender::SetAcceptIncomingConnections(const bool accept)
	{
		m_UseListener = accept;

		if (m_UseListener)
		{
			if (IsRunning())
			{
				DiscardReturnValue(StartupListener());
			}
		}
		else
		{
			if (IsRunning())
			{
				DiscardReturnValue(ShutdownListener());
			}
		}
	}

	bool Extender::SetCredentials(const ProtectedStringA& username, const ProtectedStringA& password)
	{
		if (!username.empty() && !password.empty())
		{
			ProtectedBuffer tmp(username.size());
			memcpy(tmp.GetBytes(), username.data(), username.size());

			// Store a hash of the username
			if (Crypto::Hash(tmp, m_Username, Algorithm::Hash::BLAKE2B512))
			{
				tmp.Resize(password.size());
				memcpy(tmp.GetBytes(), password.data(), password.size());

				// Store a hash of the password
				if (Crypto::Hash(tmp, m_Password, Algorithm::Hash::BLAKE2B512))
				{
					m_RequireAuthentication = true;

					return true;
				}
			}
		}
		else
		{
			m_Username.Clear();
			m_Password.Clear();
			m_RequireAuthentication = false;

			return true;
		}

		return false;
	}

	bool Extender::CheckCredentials(const BufferView& username, const BufferView& password) const
	{
		if (!m_RequireAuthentication) return true;

		if (!username.IsEmpty() && !password.IsEmpty())
		{
			ProtectedBuffer tmp(username.GetSize());
			std::memcpy(tmp.GetBytes(), username.GetBytes(), username.GetSize());

			ProtectedBuffer usrhash;
			if (Crypto::Hash(tmp, usrhash, Algorithm::Hash::BLAKE2B512))
			{
				if (m_Username == usrhash)
				{
					tmp.Resize(password.GetSize());
					std::memcpy(tmp.GetBytes(), password.GetBytes(), password.GetSize());

					ProtectedBuffer pwdhash;
					if (Crypto::Hash(tmp, pwdhash, Algorithm::Hash::BLAKE2B512))
					{
						if (m_Password == pwdhash)
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	void Extender::SetTCPListenerPort(const UInt16 port) noexcept
	{
		auto restart_listener = false;

		{
			std::unique_lock<std::shared_mutex> lock(m_Listener.Mutex);

			if (m_Listener.TCPPort != port)
			{
				m_Listener.TCPPort = port;

				if (m_UseListener && IsRunning())
				{
					restart_listener = true;
				}
			}
		}

		if (restart_listener)
		{
			DiscardReturnValue(ShutdownListener());
			DiscardReturnValue(StartupListener());
		}
	}

	bool Extender::IsOutgoingIPAllowed(const IPAddress& ip) const noexcept
	{
		if (const auto result = m_IPFilters.WithSharedLock()->GetAllowed(ip); result.Succeeded())
		{
			return *result;
		}

		return false;
	}

	bool Extender::OnStartup()
	{
		LogDbg(L"Extender '%s' starting...", GetName().c_str());

		auto success = false;

		if (InitializeIPFilters())
		{
			if (StartupThreadPool())
			{
				success = true;
			}
			else DeInitializeIPFilters();
		}

		// Return true if initialization was successful, otherwise return false and
		// QuantumGate won't be sending this extender any notifications
		return success;
	}

	void Extender::OnPostStartup()
	{
		LogDbg(L"Extender '%s' running...", GetName().c_str());

		if (m_UseListener)
		{
			DiscardReturnValue(StartupListener());
		}
	}

	void Extender::OnPreShutdown()
	{
		LogDbg(L"Extender '%s' will begin shutting down...", GetName().c_str());

		// Stop accepting incoming connections
		ShutdownListener();

		// Disconnect all connections gracefully
		DisconnectAll();
	}

	void Extender::OnShutdown()
	{
		LogDbg(L"Extender '%s' shutting down...", GetName().c_str());

		ShutdownThreadPool();

		m_Peers.WithUniqueLock()->clear();
		m_AllConnections.WithUniqueLock()->clear();
		m_AllConnectionFDs.WithUniqueLock()->clear();
		m_AllConnectionsSendEvent.Reset();
		m_AllConnectionsReceiveEvent.Reset();
		m_DNSCache.WithUniqueLock()->clear();

		DeInitializeIPFilters();
	}

	bool Extender::InitializeIPFilters()
	{
		auto success = true;

		// Allow all addresses by default
		constexpr const std::array<const WChar*, 2> allowed_nets = {
			L"0.0.0.0/0",	// IPv4
			L"::/0"			// IPv6
		};

		// Block internal networks to prevent incoming connections
		// from connecting to internal addresses
		constexpr const std::array<const WChar*, 15> internal_nets = {
			L"0.0.0.0/8",		// Local system
			L"169.254.0.0/16",	// Link local
			L"127.0.0.0/8",		// Loopback
			L"192.168.0.0/16",	// Local LAN
			L"10.0.0.0/8",		// Local LAN
			L"172.16.0.0/12",	// Local LAN
			L"224.0.0.0/4",		// Multicast
			L"240.0.0.0/4",		// Future use
			L"::/8",			// Local system
			L"fc00::/7",		// Unique Local Addresses
			L"fd00::/8",		// Unique Local Addresses
			L"fec0::/10",		// Site local
			L"fe80::/10",		// Link local
			L"ff00::/8",		// Multicast
			L"::/127"			// Inter-Router Links
		};

		m_IPFilters.WithUniqueLock([&](Core::Access::IPFilters& filters)
		{
			for (const auto net : allowed_nets)
			{
				const auto result = filters.AddFilter(net, QuantumGate::Access::IPFilterType::Allowed);
				if (result.Failed())
				{
					LogErr(L"%s: could not add %s to IP filters", GetName().c_str(), net);
					success = false;
					break;
				}
			}

			if (success)
			{
				for (const auto net : internal_nets)
				{
					const auto result = filters.AddFilter(net, QuantumGate::Access::IPFilterType::Blocked);
					if (result.Failed())
					{
						LogErr(L"%s: could not add %s to IP filters", GetName().c_str(), net);
						success = false;
						break;
					}
				}
			}
		});

		if (!success) DeInitializeIPFilters();

		return success;
	}

	void Extender::DeInitializeIPFilters() noexcept
	{
		m_IPFilters.WithUniqueLock()->Clear();
	}

	bool Extender::StartupListener()
	{
		std::unique_lock<std::shared_mutex> lock(m_Listener.Mutex);

		LogInfo(L"%s: listener starting...", GetName().c_str());

		try
		{
			m_Listener.ShutdownEvent.Reset();

			const auto endpoint = IPEndpoint(QuantumGate::IPEndpoint::Protocol::TCP, IPAddress::AnyIPv4(), m_Listener.TCPPort);
			m_Listener.Socket = Network::Socket(Network::AddressFamily::IPv4, Network::Socket::Type::Stream, Protocol::TCP);
			if (m_Listener.Socket.Listen(endpoint, false, false))
			{
				LogInfo(L"%s: listening on endpoint %s", GetName().c_str(), endpoint.GetString().c_str());

				m_Listener.Thread = std::thread(Extender::ListenerThreadLoop, this);

				return true;
			}
		}
		catch (...) {}

		LogErr(L"%s: listener startup failed", GetName().c_str());

		return false;
	}

	void Extender::ShutdownListener()
	{
		std::unique_lock<std::shared_mutex> lock(m_Listener.Mutex);

		LogInfo(L"%s: listener shutting down...", GetName().c_str());

		m_Listener.ShutdownEvent.Set();

		if (m_Listener.Thread.joinable())
		{
			// Wait for the thread to shut down
			m_Listener.Thread.join();
		}
	}

	bool Extender::StartupThreadPool()
	{
		if (m_ThreadPool.AddThread(GetName() + L" Main Worker Thread",
								   MakeCallback(this, &Extender::MainWorkerThreadLoop),
								   MakeCallback(this, &Extender::MainWorkerThreadWait)) &&
			m_ThreadPool.AddThread(GetName() + L" DataRelay Worker Thread",
								   MakeCallback(this, &Extender::DataRelayWorkerThreadLoop),
								   MakeCallback(this, &Extender::DataRelayWorkerThreadWait)))
		{
			if (m_ThreadPool.Startup())
			{
				return true;
			}
			else LogErr(L"Couldn't start a Socks5 threadpool");
		}

		return false;
	}

	void Extender::ShutdownThreadPool() noexcept
	{
		m_ThreadPool.Shutdown();
		m_ThreadPool.Clear();
	}

	void Extender::OnPeerEvent(PeerEvent&& event)
	{
		String ev(L"Unknown");

		switch (event.GetType())
		{
			case PeerEvent::Type::Connected:
			{
				ev = L"Connect";

				if (!AddPeer(event.GetPeerLUID()))
				{
					LogErr(L"Extender '%s' failed to add peer %llu", GetName().c_str(), event.GetPeerLUID());
				}

				break;
			}
			case PeerEvent::Type::Resumed:
			{
				ev = L"Resumed";

				if (!AddPeer(event.GetPeerLUID()))
				{
					LogErr(L"Extender '%s' failed to add peer %llu", GetName().c_str(), event.GetPeerLUID());
				}

				break;
			}
			case PeerEvent::Type::Suspended:
			{
				ev = L"Suspended";
				
				RemovePeer(event.GetPeerLUID());

				break;
			}
			case PeerEvent::Type::Disconnected:
			{
				ev = L"Disconnect";

				RemovePeer(event.GetPeerLUID());

				break;
			}
			default:
			{
				assert(false);
			}
		}

		LogInfo(L"Extender '%s' got peer event: %s, Peer LUID: %llu", GetName().c_str(), ev.c_str(), event.GetPeerLUID());
	}

	QuantumGate::Extender::PeerEvent::Result Extender::OnPeerMessage(PeerEvent&& event)
	{
		assert(event.GetType() == PeerEvent::Type::Message);

		PeerEvent::Result result;

		if (const auto msgdata = event.GetMessageData(); msgdata != nullptr)
		{
			UInt16 mtype = 0;
			BufferReader rdr(*msgdata, true);

			// Get message type first
			if (rdr.Read(mtype))
			{
				switch (static_cast<MessageType>(mtype))
				{
					case MessageType::ConnectDomain:
					{
						result.Handled = true;

						Connection::ID cid{ 0 };
						UInt8 socks_version{ 0 };
						String domain;
						UInt16 port{ 0 };

						if (rdr.Read(cid, socks_version, WithSize(domain, MaxSize::_1KB), port))
						{
							result.Success = HandleConnectDomainPeerMessage(event.GetPeerLUID(), cid,
																			static_cast<SocksProtocolVersion>(socks_version),
																			domain, port);
						}
						else LogErr(L"%s: could not read ConnectDomain message from peer %llu",
									GetName().c_str(), event.GetPeerLUID());

						break;
					}
					case MessageType::ConnectIP:
					{
						result.Handled = true;

						Connection::ID cid{ 0 };
						UInt8 socks_version{ 0 };
						SerializedBinaryIPAddress ip;
						UInt16 port{ 0 };

						if (rdr.Read(cid, socks_version, ip, port))
						{
							result.Success = HandleConnectIPPeerMessage(event.GetPeerLUID(), cid,
																		static_cast<SocksProtocolVersion>(socks_version),
																		ip, port);
						}
						else LogErr(L"%s: could not read ConnectIP message from peer %llu",
									GetName().c_str(), event.GetPeerLUID());

						break;
					}
					case MessageType::Socks4ReplyRelay:
					{
						result.Handled = true;

						Connection::ID cid{ 0 };
						Socks4Protocol::Replies reply{ Socks4Protocol::Replies::FailedOrRejected };
						SerializedBinaryIPAddress ip;
						UInt16 port{ 0 };

						if (rdr.Read(cid, reply, ip, port))
						{
							result.Success = HandleSocks4ReplyRelayPeerMessage(event.GetPeerLUID(), cid, reply,
																			   BufferView(reinterpret_cast<Byte*>(&ip.Bytes),
																						  sizeof(SerializedBinaryIPAddress::Bytes)), port);
						}
						else LogErr(L"%s: could not read Socks4ReplyRelay message from peer %llu",
									GetName().c_str(), event.GetPeerLUID());

						break;
					}
					case MessageType::Socks5ReplyRelay:
					{
						result.Handled = true;

						Connection::ID cid{ 0 };
						Socks5Protocol::Replies reply{ Socks5Protocol::Replies::GeneralFailure };
						Socks5Protocol::AddressTypes atype{ Socks5Protocol::AddressTypes::Unknown };
						SerializedBinaryIPAddress ip;
						UInt16 port{ 0 };

						if (rdr.Read(cid, reply, atype, ip, port))
						{
							result.Success = HandleSocks5ReplyRelayPeerMessage(event.GetPeerLUID(), cid, reply, atype,
																			   BufferView(reinterpret_cast<Byte*>(&ip.Bytes),
																						  sizeof(SerializedBinaryIPAddress::Bytes)), port);
						}
						else LogErr(L"%s: could not read Socks5ReplyRelay message from peer %llu",
									GetName().c_str(), event.GetPeerLUID());

						break;
					}
					case MessageType::DataRelay:
					{
						result.Handled = true;

						Connection::ID cid{ 0 };
						Buffer data;

						if (rdr.Read(cid, WithSize(data, GetMaxDataRelayDataSize())))
						{
							auto con = GetConnection(event.GetPeerLUID(), cid);
							if (con)
							{
								con->WithUniqueLock([&](Connection& connection)
								{
									if (!connection.SendRelayedData(std::move(data)))
									{
										LogErr(L"%s: error sending relayed data to connection %llu", GetName().c_str(), cid);
										connection.SetDisconnectCondition();
									}
								});

								result.Success = true;
							}
							else
							{
								LogErr(L"%s: received DataRelay from peer %llu for unknown connection %llu",
									   GetName().c_str(), event.GetPeerLUID(), cid);
							}
						}
						else LogErr(L"%s: could not read DataRelay message from peer %llu",
									GetName().c_str(), event.GetPeerLUID());

						break;
					}
					case MessageType::Disconnect:
					{
						result.Handled = true;

						Connection::ID cid{ 0 };

						if (rdr.Read(cid))
						{
							auto con = GetConnection(event.GetPeerLUID(), cid);
							if (con)
							{
								LogDbg(L"%s: received Disconnect from peer %llu for connection %llu",
									   GetName().c_str(), event.GetPeerLUID(), cid);

								con->WithUniqueLock([&](Connection& connection)
								{
									connection.SetPeerConnected(false);
									connection.SetDisconnectCondition();

									DiscardReturnValue(SendDisconnectAck(event.GetPeerLUID(), cid));
								});

								result.Success = true;
							}
							else
							{
								LogErr(L"%s: received Disconnect from peer %llu for unknown connection %llu",
									   GetName().c_str(), event.GetPeerLUID(), cid);
							}
						}
						else LogErr(L"%s: could not read Disconnect message from peer %llu",
									GetName().c_str(), event.GetPeerLUID());

						break;
					}
					case MessageType::DisconnectAck:
					{
						result.Handled = true;

						Connection::ID cid{ 0 };

						if (rdr.Read(cid))
						{
							auto con = GetConnection(event.GetPeerLUID(), cid);
							if (con)
							{
								LogDbg(L"%s: received DisconnectAck from peer %llu for connection %llu",
									   GetName().c_str(), event.GetPeerLUID(), cid);

								con->WithUniqueLock([](Connection& connection) noexcept
								{
									connection.SetPeerConnected(false);
									connection.SetStatus(Connection::Status::Disconnected);
								});

								result.Success = true;
							}
							else
							{
								LogErr(L"%s: received DisconnectAck from peer %llu for unknown connection %llu",
									   GetName().c_str(), event.GetPeerLUID(), cid);
							}
						}
						else LogErr(L"%s: could not read DisconnectAck message from peer %llu",
									GetName().c_str(), event.GetPeerLUID());

						break;
					}
					default:
					{
						LogErr(L"%s: received unknown message type from %llu: %u",
							   GetName().c_str(), event.GetPeerLUID(), mtype);
						break;
					}
				}
			}
		}

		return result;
	}

	bool Extender::HandleConnectDomainPeerMessage(const PeerLUID pluid, const Connection::ID cid,
												  const SocksProtocolVersion socks_version,
												  const String& domain, const UInt16 port)
	{
		if ((socks_version == SocksProtocolVersion::Socks4 || socks_version == SocksProtocolVersion::Socks5) &&
			!domain.empty() && port != 0)
		{
			LogDbg(L"%s: received ConnectDomain from peer %llu for connection %llu for domain %s",
				   GetName().c_str(), pluid, cid, domain.c_str());

			const auto ip = ResolveDomainIP(domain);
			if (ip)
			{
				SLogInfo(GetName() << L": domain " << SLogFmt(FGBrightMagenta) << domain.c_str() <<
						 SLogFmt(Default) << L" resolved to IP " << SLogFmt(FGBrightMagenta) <<
						 ip->GetString() << SLogFmt(Default) << L" for connection " << cid);

				DiscardReturnValue(MakeOutgoingConnection(pluid, cid, socks_version, *ip, port));
			}
			else
			{
				LogErr(L"%s: could not resolve IP addresses for domain %s", GetName().c_str(), domain.c_str());

				// Could not resolve domain
				switch (socks_version)
				{
					case SocksProtocolVersion::Socks4:
						DiscardReturnValue(SendSocks4Reply(pluid, cid, Socks4Protocol::Replies::FailedOrRejected));
					case SocksProtocolVersion::Socks5:
						DiscardReturnValue(SendSocks5Reply(pluid, cid, Socks5Protocol::Replies::HostUnreachable));
						break;
					default:
						assert(false);
						break;
				}
			}

			return true;
		}
		else LogErr(L"%s: received invalid ConnectDomain parameters from peer %llu", GetName().c_str(), pluid);

		return false;
	}

	bool Extender::HandleConnectIPPeerMessage(const PeerLUID pluid, const Connection::ID cid,
											  const SocksProtocolVersion socks_version,
											  const BinaryIPAddress& ip, const UInt16 port)
	{
		if ((socks_version == SocksProtocolVersion::Socks4 || socks_version == SocksProtocolVersion::Socks5) &&
			(ip.AddressFamily == BinaryIPAddress::Family::IPv4 ||
			 ip.AddressFamily == BinaryIPAddress::Family::IPv6) && port != 0)
		{
			LogDbg(L"%s: received ConnectIP from peer %llu for connection %llu", GetName().c_str(), pluid, cid);

			DiscardReturnValue(MakeOutgoingConnection(pluid, cid, socks_version, IPAddress(ip), port));

			return true;
		}
		else LogErr(L"%s: received invalid ConnectIP parameters from peer %llu", GetName().c_str(), pluid);

		return false;
	}

	bool Extender::HandleSocks4ReplyRelayPeerMessage(const PeerLUID pluid, const Connection::ID cid,
													 const Socks4Protocol::Replies reply,
													 const BufferView& address, const UInt16 port)
	{
		switch (reply)
		{
			case Socks4Protocol::Replies::Succeeded:
			case Socks4Protocol::Replies::FailedOrRejected:
			case Socks4Protocol::Replies::FailedIdentDUnreachable:
			case Socks4Protocol::Replies::FailedUnknownUser:
			{
				auto con = GetConnection(pluid, cid);
				if (con)
				{
					con->WithUniqueLock([&](Connection& connection)
					{
						// If incoming connection is still active
						// (might have been closed in the mean time)
						if (connection.IsActive())
						{
							if (reply == Socks4Protocol::Replies::Succeeded)
							{
								connection.SetStatus(Connection::Status::Ready);
							}
							else
							{
								// Error
								connection.SetPeerConnected(false);
								connection.SetDisconnectCondition();
							}

							DiscardReturnValue(connection.SendSocks4Reply(reply, address, port));
						}
					});

					return true;
				}
				else
				{
					LogErr(L"%s: received Socks4ReplyRelay (%u) from peer %llu for unknown connection ID %llu",
						   GetName().c_str(), reply, pluid, cid);
				}

				break;
			}
			default:
			{
				LogErr(L"%s: received unknown Socks4 reply from %llu: %u", GetName().c_str(), pluid, reply);
				break;
			}
		}

		return false;
	}

	bool Extender::HandleSocks5ReplyRelayPeerMessage(const PeerLUID pluid, const Connection::ID cid,
													 const Socks5Protocol::Replies reply,
													 const Socks5Protocol::AddressTypes atype,
													 const BufferView& address, const UInt16 port)
	{
		switch (reply)
		{
			case Socks5Protocol::Replies::Succeeded:
			case Socks5Protocol::Replies::GeneralFailure:
			case Socks5Protocol::Replies::ConnectionNotAllowed:
			case Socks5Protocol::Replies::NetworkUnreachable:
			case Socks5Protocol::Replies::HostUnreachable:
			case Socks5Protocol::Replies::ConnectionRefused:
			case Socks5Protocol::Replies::TTLExpired:
			{
				switch (atype)
				{
					case Socks5Protocol::AddressTypes::IPv4:
					case Socks5Protocol::AddressTypes::IPv6:
					{
						auto con = GetConnection(pluid, cid);
						if (con)
						{
							con->WithUniqueLock([&](Connection& connection)
							{
								// If incoming connection is still active
								// (might have been closed in the mean time)
								if (connection.IsActive())
								{
									if (reply == Socks5Protocol::Replies::Succeeded)
									{
										connection.SetStatus(Connection::Status::Ready);
									}
									else
									{
										// Error
										connection.SetPeerConnected(false);
										connection.SetDisconnectCondition();
									}

									DiscardReturnValue(connection.SendSocks5Reply(reply, atype, address, port));
								}
							});

							return true;
						}
						else
						{
							LogErr(L"%s: received Socks5ReplyRelay (%u) from peer %llu for unknown connection ID %llu",
								   GetName().c_str(), reply, pluid, cid);
						}

						break;
					}
					default:
					{
						LogErr(L"%s: received unsupported address type from %llu: %u", GetName().c_str(), pluid, atype);
						break;
					}
				}

				break;
			}
			default:
			{
				LogErr(L"%s: received unknown Socks5 reply from %llu: %u", GetName().c_str(), pluid, reply);
				break;
			}
		}

		return false;
	}

	bool Extender::AddPeer(const PeerLUID pluid) noexcept
	{
		try
		{
			auto peer_ths = std::make_shared<Peer_ThS>(pluid, GetMaxDataRelayDataSize());

			[[maybe_unused]] const auto [it, inserted] = m_Peers.WithUniqueLock()->insert({ pluid, std::move(peer_ths) });

			assert(inserted);
			return inserted;
		}
		catch (...) {}

		return false;
	}

	void Extender::RemovePeer(const PeerLUID pluid) noexcept
	{
		DisconnectFor(pluid);

		const auto num = m_Peers.WithUniqueLock()->erase(pluid);
		assert(num == 1);
	}

	std::shared_ptr<Peer_ThS> Extender::GetPeer(const PeerLUID pluid) const noexcept
	{
		std::shared_ptr<Peer_ThS> peer_ths{ nullptr };

		m_Peers.WithSharedLock([&](const Peers& peers)
		{
			if (const auto it = peers.find(pluid); it != peers.end())
			{
				peer_ths = it->second;
			}
		});

		return peer_ths;
	}

	bool Extender::AddConnection(const PeerLUID pluid, const Connection::ID cid, std::shared_ptr<Connection_ThS>&& c) noexcept
	{
		auto success = false;

		Connection::Key key{ 0 };
		SOCKET shandle{ INVALID_SOCKET };
		
		c->WithSharedLock([&](const Connection& connection)
		{
			key = connection.GetKey();
			shandle = connection.GetSocket().GetHandle();
		});

		m_AllConnections.WithUniqueLock([&](Connections& connections)
		{
			[[maybe_unused]] const auto [it, inserted] = connections.insert({ key, c });

			assert(inserted);
			success = inserted;
		});

		if (success)
		{
			success = false;

			// Remove if we fail
			auto sg = MakeScopeGuard([&]() noexcept { RemoveConnection(key); });

			if (AddConnectionFD(shandle))
			{
				// Remove if we fail
				auto sg2 = MakeScopeGuard([&]() noexcept { RemoveConnectionFD(shandle); });

				const auto peer_ths = GetPeer(pluid);
				if (peer_ths)
				{
					peer_ths->WithUniqueLock([&](Peer& peer)
					{
						[[maybe_unused]] const auto [it, inserted] = peer.Connections.insert({ key, c });

						assert(inserted);
						success = inserted;

						peer.CalcMaxSndRcvSize();

						LogDbg(L"%s: MaxSndRcv size for peer %llu: %zu", GetName().c_str(), peer.ID, peer.MaxSndRcvSize);
					});
				}

				if (success)
				{
					sg.Deactivate();
					sg2.Deactivate();

					// Start doing some processing for speed
					c->WithUniqueLock([](Connection& connection)
					{
						if (connection.IsActive()) connection.ProcessEvents();
					});

					SetConnectionSendEvent();
				}
			}
		}

		if (!success)
		{
			LogErr(L"%s: could not add new connection %llu for peer %llu", GetName().c_str(), cid, pluid);
		}

		return success;
	}

	void Extender::RemoveConnection(const Connection::Key key) noexcept
	{
		std::optional<SOCKET> socket;
		std::optional<PeerLUID> pluid;

		m_AllConnections.WithUniqueLock([&](Connections& connections)
		{
			const auto it = connections.find(key);
			if (it != connections.end())
			{
				it->second->WithSharedLock([&](const Connection& c)
				{
					pluid = c.GetPeerLUID();
					socket = c.GetSocket().GetHandle();
				});

				connections.erase(it);

				LogDbg(L"%s: total number of connections: %zu", GetName().c_str(), connections.size());
			}
		});

		if (socket)
		{
			RemoveConnectionFD(*socket);
		}

		if (pluid)
		{
			const auto peer_ths = GetPeer(*pluid);
			if (peer_ths)
			{
				peer_ths->WithUniqueLock([&](Peer& peer)
				{
					peer.Connections.erase(key);
					peer.CalcMaxSndRcvSize();

					LogDbg(L"%s: total number of connections for peer %llu: %zu",
						   GetName().c_str(), peer.ID, peer.Connections.size());

					LogDbg(L"%s: MaxSndRcv size for peer %llu: %zu", GetName().c_str(), peer.ID, peer.MaxSndRcvSize);
				});
			}
		}
	}

	[[nodiscard]] bool Extender::AddConnectionFD(const SOCKET s) noexcept
	{
		try
		{
			m_AllConnectionFDs.WithUniqueLock()->emplace_back(WSAPOLLFD{ .fd = s, .events = POLLRDNORM });
		}
		catch (...) { return false; }

		return true;
	}

	void Extender::RemoveConnectionFD(const SOCKET s) noexcept
	{
		m_AllConnectionFDs.WithUniqueLock([&](auto& value)
		{
			const auto it = std::find_if(value.begin(), value.end(), [&](const auto& wsapollfd)
			{
				return (wsapollfd.fd == s);
			});
			if (it != value.end()) value.erase(it);
		});
	}

	void Extender::RemoveConnections(const std::vector<Connection::Key>& conn_list) noexcept
	{
		for (const auto key : conn_list)
		{
			RemoveConnection(key);
		}
	}

	std::shared_ptr<Connection_ThS> Extender::GetConnection(const PeerLUID pluid, const Connection::ID cid) const noexcept
	{
		std::shared_ptr<Connection_ThS> con{ nullptr };

		m_AllConnections.WithSharedLock([&](const Connections& connections)
		{
			if (const auto it = connections.find(Connection::MakeKey(pluid, cid)); it != connections.end())
			{
				con = it->second;
			}
		});

		return con;
	}

	void Extender::Disconnect(Connection_ThS& c)
	{
		c.WithUniqueLock([&](Connection& connection)
		{
			Disconnect(connection);
		});
	}

	void Extender::Disconnect(Connection& c)
	{
		if (c.IsActive())
		{
			c.Disconnect();
		}
	}

	void Extender::DisconnectFor(const PeerLUID pluid)
	{
		LogInfo(L"%s: disconnecting connections for peer %llu", GetName().c_str(), pluid);

		const auto peer_ths = GetPeer(pluid);
		if (peer_ths)
		{
			peer_ths->WithSharedLock([&](const Peer& peer)
			{
				for (auto& connection : peer.Connections)
				{
					connection.second->WithUniqueLock([&](Connection& c) noexcept
					{
						c.SetPeerConnected(false);
						c.SetDisconnectCondition();
					});
				}
			});
		}
	}

	void Extender::DisconnectAll()
	{
		LogInfo(L"%s: disconnecting all connections", GetName().c_str());

		m_AllConnections.WithUniqueLock([&](const Connections& connections)
		{
			for (auto& connection : connections)
			{
				Disconnect(*connection.second);
			}
		});
	}

	void Extender::ListenerThreadLoop(Extender* extender)
	{
		assert(extender != nullptr);

		auto& extname = extender->GetName();

		LogDbg(L"%s: listener thread %u starting", extname.c_str(), std::this_thread::get_id());

		Util::SetCurrentThreadName(extname + L" Listener Thread");

		// If the shutdown event is set quit the loop
		while (!extender->m_Listener.ShutdownEvent.IsSet())
		{
			// Check if we have a read event waiting for us
			if (extender->m_Listener.Socket.UpdateIOStatus(10ms))
			{
				if (extender->m_Listener.Socket.GetIOStatus().CanRead())
				{
					// Probably have a connection waiting to accept
					LogDbg(L"%s: accepting new incoming connection", extname.c_str());

					extender->AcceptIncomingConnection();
				}
				else if (extender->m_Listener.Socket.GetIOStatus().HasException())
				{
					LogErr(L"%s: exception on listener socket (%s)", extname.c_str(),
						   GetSysErrorString(extender->m_Listener.Socket.GetIOStatus().GetErrorCode()).c_str());
					break;
				}
			}
			else
			{
				LogErr(L"%s: could not get status of listener socket", extname.c_str());
				break;
			}
		}

		if (extender->m_Listener.Socket.GetIOStatus().IsOpen()) extender->m_Listener.Socket.Close();

		LogDbg(L"%s: listener thread %u exiting", extname.c_str(), std::this_thread::get_id());
	}

	void Extender::MainWorkerThreadWait(const Concurrency::Event& shutdown_event)
	{
		auto waited = false;

		// This could be made faster by waiting on multiple events with one call
		// instead of 2 seperate calls
		if (!m_AllConnectionsSendEvent.Wait(0ms))
		{
			m_AllConnectionFDs.IfUniqueLock([&](auto& value)
			{
				if (!value.empty())
				{
					const auto ret = WSAPoll(value.data(), static_cast<ULONG>(value.size()), 1);
					waited = (ret != SOCKET_ERROR);
				}
			});
		}
		else waited = true;

		if (!waited) shutdown_event.Wait(1ms);
	}

	void Extender::MainWorkerThreadLoop(const Concurrency::Event& shutdown_event)
	{
		m_AllConnectionsSendEvent.Reset();

		std::vector<Connection::Key> rlist;

		m_AllConnections.WithSharedLock([&](const Connections& connections)
		{
			for (auto it = connections.begin(); it != connections.end() && !shutdown_event.IsSet(); ++it)
			{
				it->second->IfUniqueLock([&](Connection& connection)
				{
					if (connection.IsActive())
					{
						connection.ProcessEvents();

						if (connection.IsTimedOut())
						{
							LogInfo(L"%s: connection %llu timed out; will disconnect", GetName().c_str(), connection.GetID());

							connection.SetDisconnectCondition();
						}
					}
					else if ((connection.IsDisconnected() || connection.IsDisconnecting()) && connection.IsTimedOut())
					{
						LogDbg(L"%s: removing connection %llu", GetName().c_str(), connection.GetID());

						rlist.emplace_back(connection.GetKey());
					}
				});
			}
		});

		if (!rlist.empty())
		{
			RemoveConnections(rlist);
			rlist.clear();
		}
	}

	void Extender::DataRelayWorkerThreadWait(const Concurrency::Event& shutdown_event)
	{
		m_AllConnectionsReceiveEvent.Wait(1ms);
	}

	void Extender::DataRelayWorkerThreadLoop(const Concurrency::Event& shutdown_event)
	{
		m_AllConnectionsReceiveEvent.Reset();

		m_Peers.WithSharedLock([&](const Peers& peers)
		{
			for (auto& pit : peers)
			{
				if (shutdown_event.IsSet()) break;

				Size act_send{ 0 };

				pit.second->WithSharedLock([&](const Peer& peer)
				{
					const auto max_send = peer.MaxSndRcvSize;
					act_send = (std::max)(max_send, peer.ActSndRcvSize);

					for (auto it = peer.Connections.begin(); it != peer.Connections.end() && !shutdown_event.IsSet(); ++it)
					{
						Size sent{ 0 };

						it->second->IfUniqueLock([&](Connection& connection)
						{
							if (connection.IsActive())
							{
								connection.ProcessRelayEvents(act_send, sent);
							}
						});

						if (sent < max_send)
						{
							act_send += (max_send - sent);
						}
						else
						{
							act_send -= sent;
							act_send += max_send;
						}

						if (act_send > peer.MaxDataRelayDataSize)
						{
							act_send = peer.MaxDataRelayDataSize;
						}
					}
				});

				pit.second->WithUniqueLock([&](Peer& peer)
				{
					if (peer.ActSndRcvSize != act_send)
					{
						LogDbg(L"%s: ActSndRcvSize for peer %llu: %zu", GetName().c_str(), peer.ID, act_send);
					}

					peer.ActSndRcvSize = act_send;
				});
			}
		});
	}

	void Extender::AcceptIncomingConnection()
	{
		Socket s;
		if (m_Listener.Socket.Accept(s))
		{
			const auto pluid = GetPeerForConnection();
			if (pluid)
			{
				const auto endp = s.GetPeerEndpoint().GetString();

				auto cths = std::make_shared<Connection_ThS>(*this, *pluid, std::move(s));

				const auto cid = cths->WithSharedLock()->GetID();

				if (AddConnection(*pluid, cid, std::move(cths)))
				{
					LogInfo(L"%s: accepted connection %llu from endpoint %s and associated with peer %llu",
							GetName().c_str(), cid, endp.c_str(), *pluid);
				}
			}
			else
			{
				LogErr(L"%s: found no peers to associate with socket %s",
					   GetName().c_str(), s.GetPeerEndpoint().GetString().c_str());

				s.Close();
			}
		}
		else LogErr(L"%s: could not accept new connection", GetName().c_str());
	}

	bool Extender::SendConnectDomain(const PeerLUID pluid, const Connection::ID cid, const SocksProtocolVersion socks_version,
									 const String& domain, const UInt16 port) const noexcept
	{
		SLogInfo(GetName() << L": connecting to " << SLogFmt(FGBrightMagenta) << domain.c_str() <<
				 SLogFmt(Default) << L" through peer " << pluid << L" for connection " << cid <<
				 L" (Socks version " << static_cast<UInt8>(socks_version) << L")");

		constexpr UInt16 msgtype = static_cast<const UInt16>(MessageType::ConnectDomain);
		const UInt8 socksv = static_cast<UInt8>(socks_version);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid, socksv, WithSize(domain, MaxSize::_1KB), port))
		{
			if (Send(pluid, writer.MoveWrittenBytes())) return true;

			LogErr(L"%s: could not send ConnectDomain message for connection %llu to peer %llu",
				   GetName().c_str(), cid, pluid);
		}
		else LogErr(L"%s: could not prepare ConnectDomain message for connection %llu", GetName().c_str(), cid);

		return false;
	}

	bool Extender::SendConnectIP(const PeerLUID pluid, const Connection::ID cid, const SocksProtocolVersion socks_version,
								 const BinaryIPAddress& ip, const UInt16 port) const noexcept
	{
		assert(ip.AddressFamily != BinaryIPAddress::Family::Unspecified);

		SLogInfo(GetName() << L": connecting to " << SLogFmt(FGBrightMagenta) <<
				 IPEndpoint(IPEndpoint::Protocol::TCP, IPAddress(ip), port).GetString().c_str() <<
				 SLogFmt(Default) << L" through peer " << pluid << L" for connection " << cid << L" (Socks version " <<
				 static_cast<UInt8>(socks_version) << L")");

		constexpr UInt16 msgtype = static_cast<const UInt16>(MessageType::ConnectIP);
		const UInt8 socksv = static_cast<UInt8>(socks_version);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid, socksv, SerializedBinaryIPAddress{ ip }, port))
		{
			if (Send(pluid, writer.MoveWrittenBytes())) return true;

			LogErr(L"%s: could not send ConnectIP message for connection %llu to peer %llu",
				   GetName().c_str(), cid, pluid);
		}
		else LogErr(L"%s: could not prepare ConnectIP message for connection %llu", GetName().c_str(), cid);

		return false;
	}

	bool Extender::SendDisconnect(const PeerLUID pluid, const Connection::ID cid) const noexcept
	{
		constexpr UInt16 msgtype = static_cast<const UInt16>(MessageType::Disconnect);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid))
		{
			if (Send(pluid, writer.MoveWrittenBytes())) return true;

			LogErr(L"%s: could not send Disconnect message for connection %llu to peer %llu",
				   GetName().c_str(), cid, pluid);
		}
		else LogErr(L"%s: could not prepare Disconnect message for connection %llu", GetName().c_str(), cid);

		return false;
	}

	bool Extender::SendDisconnectAck(const PeerLUID pluid, const Connection::ID cid) const noexcept
	{
		constexpr UInt16 msgtype = static_cast<const UInt16>(MessageType::DisconnectAck);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid))
		{
			if (Send(pluid, writer.MoveWrittenBytes())) return true;

			LogErr(L"%s: could not send DisconnectAck message for connection %llu to peer %llu",
				   GetName().c_str(), cid, pluid);
		}
		else LogErr(L"%s: could not prepare DisconnectAck message for connection %llu", GetName().c_str(), cid);

		return false;
	}

	bool Extender::SendSocks4Reply(const PeerLUID pluid, const Connection::ID cid,
								   const Socks4Protocol::Replies reply,
								   const BinaryIPAddress ip, const UInt16 port) const noexcept
	{
		constexpr UInt16 msgtype = static_cast<const UInt16>(MessageType::Socks4ReplyRelay);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid, reply, SerializedBinaryIPAddress{ ip }, port))
		{
			if (Send(pluid, writer.MoveWrittenBytes())) return true;

			LogErr(L"%s: could not send Socks4ReplyRelay message for connection %llu to peer %llu",
				   GetName().c_str(), cid, pluid);
		}
		else LogErr(L"%s: could not prepare Socks4ReplyRelay message for connection %llu", GetName().c_str(), cid);

		return false;
	}

	bool Extender::SendSocks5Reply(const PeerLUID pluid, const Connection::ID cid,
								   const Socks5Protocol::Replies reply,
								   const Socks5Protocol::AddressTypes atype,
								   const BinaryIPAddress ip, const UInt16 port) const noexcept
	{
		constexpr UInt16 msgtype = static_cast<const UInt16>(MessageType::Socks5ReplyRelay);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid, reply, atype, SerializedBinaryIPAddress{ ip }, port))
		{
			if (Send(pluid, writer.MoveWrittenBytes())) return true;

			LogErr(L"%s: could not send Socks5ReplyRelay message for connection %llu to peer %llu",
				   GetName().c_str(), cid, pluid);
		}
		else LogErr(L"%s: could not prepare Socks5ReplyRelay message for connection %llu", GetName().c_str(), cid);

		return false;
	}

	Result<> Extender::SendDataRelay(const PeerLUID pluid, const Connection::ID cid, const BufferView& buffer) const noexcept
	{
		constexpr UInt16 msgtype = static_cast<UInt16>(MessageType::DataRelay);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid, WithSize(buffer, GetMaxDataRelayDataSize())))
		{
			auto result = SendMessageTo(pluid, writer.MoveWrittenBytes(),
										QuantumGate::SendParameters{ .Compress = m_UseCompression });
			if (result.Failed() && result != ResultCode::PeerSendBufferFull)
			{
				LogErr(L"%s: could not send DataRelay message for connection %llu to peer %llu (%s)",
					   GetName().c_str(), cid, pluid, result.GetErrorString().c_str());
			}

			return result;
		}
		else LogErr(L"%s: could not prepare DataRelay message for connection %llu; buffer size is %llu and max. data size is %llu",
					GetName().c_str(), cid, buffer.GetSize(), GetMaxDataRelayDataSize());

		return ResultCode::Failed;
	}

	bool Extender::Send(const PeerLUID pluid, Buffer&& buffer) const noexcept
	{
		// This is not the best way to handle buffer full condition
		// but this is just a test extender
		while (true)
		{
			// Make a copy
			auto temp_buf = buffer;

			if (const auto result = SendMessageTo(pluid, std::move(temp_buf),
												  QuantumGate::SendParameters{ .Compress = m_UseCompression }); result.Succeeded())
			{
				return true;
			}
			else if (result == ResultCode::PeerSendBufferFull)
			{
				// Try again after a brief wait
				std::this_thread::sleep_for(1ms);
			}
			else break;
		}

		return false;
	}

	std::optional<PeerLUID> Extender::GetPeerForConnection() const
	{
		std::optional<PeerLUID> pluid;

		m_Peers.WithSharedLock([&](const Peers& peers)
		{
			if (!peers.empty())
			{
				pluid = std::next(std::begin(peers),
								  static_cast<Size>(Util::GetPseudoRandomNumber(0, peers.size() - 1u)))->second->WithSharedLock()->ID;
			}
		});

		return pluid;
	}

	bool Extender::MakeOutgoingConnection(const PeerLUID pluid, const Connection::ID cid,
										  const SocksProtocolVersion socks_version, const IPAddress& ip, const UInt16 port)
	{
		Socks4Protocol::Replies reply4{ Socks4Protocol::Replies::FailedOrRejected };
		Socks5Protocol::Replies reply5{ Socks5Protocol::Replies::GeneralFailure };

		if (IsOutgoingIPAllowed(ip))
		{
			const IPEndpoint endp(IPEndpoint::Protocol::TCP, ip, port);
			Socket s(endp.GetIPAddress().GetFamily());

			LogInfo(L"%s: connecting to %s for peer %llu for connection %llu (Socks version %u)",
					GetName().c_str(), endp.GetString().c_str(), pluid, cid, socks_version);

			if (s.BeginConnect(endp))
			{
				auto cths = std::make_unique<Connection_ThS>(*this, pluid, cid, socks_version, std::move(s));

				cths->WithUniqueLock()->SetPeerConnected(true);

				if (AddConnection(pluid, cid, std::move(cths)))
				{
					return true;
				}
			}
			else
			{
				// Could not connect
				reply5 = TranslateWSAErrorToSocks5(WSAGetLastError());
			}
		}
		else
		{
			LogErr(L"%s: attempt by peer %llu (connection %llu) to connect to address %s that is not allowed",
				   GetName().c_str(), pluid, cid, ip.GetString().c_str());

			reply5 = Socks5Protocol::Replies::ConnectionNotAllowed;
		}

		switch (socks_version)
		{
			case SocksProtocolVersion::Socks4:
				DiscardReturnValue(SendSocks4Reply(pluid, cid, reply4));
			case SocksProtocolVersion::Socks5:
				DiscardReturnValue(SendSocks5Reply(pluid, cid, reply5));
				break;
			default:
				assert(false);
				break;
		}

		return false;
	}

	std::optional<IPAddress> Extender::ResolveDomainIP(const String& domain) noexcept
	{
		{
			auto cache = m_DNSCache.WithSharedLock();
			if (const auto it = cache->find(domain); it != cache->end())
			{
				return it->second;
			}
		}

		auto cache = m_DNSCache.WithUniqueLock();
		if (const auto it = cache->find(domain); it != cache->end())
		{
			return it->second;
		}
		else
		{
			ADDRINFOW* result{ nullptr };

			const auto ret = GetAddrInfoW(domain.c_str(), L"", nullptr, &result);
			if (ret == 0)
			{
				// Free ADDRINFO resources when we leave
				const auto sg = MakeScopeGuard([&]() noexcept { FreeAddrInfoW(result); });

				for (auto ptr = result; ptr != nullptr; ptr = ptr->ai_next)
				{
					if (ptr->ai_family == AF_INET || ptr->ai_family == AF_INET6)
					{
						const auto ip = IPAddress(ptr->ai_addr);

						cache->insert({ domain, ip });

						return ip;
					}
				}
			}
		}

		return std::nullopt;
	}

	Socks4Protocol::Replies Extender::TranslateWSAErrorToSocks4(Int errorcode) const noexcept
	{
		return Socks4Protocol::Replies::FailedOrRejected;
	}

	Socks5Protocol::Replies Extender::TranslateWSAErrorToSocks5(Int errorcode) const noexcept
	{
		switch (errorcode)
		{
			case WSAECONNREFUSED:
				return Socks5Protocol::Replies::ConnectionRefused;
			case WSAETIMEDOUT:
				return Socks5Protocol::Replies::TTLExpired;
			case WSAEHOSTDOWN:
			case WSAEHOSTUNREACH:
				return Socks5Protocol::Replies::HostUnreachable;
			case WSAENETUNREACH:
				return Socks5Protocol::Replies::NetworkUnreachable;
		}

		// Default
		return Socks5Protocol::Replies::GeneralFailure;
	}
}