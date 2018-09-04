// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Socks5Extender.h"

#include <cassert>
#include <regex>
#include <ws2tcpip.h>
#include <Iphlpapi.h>

#include "Console.h"
#include "Common\Util.h"
#include "Common\Endian.h"
#include "Common\ScopeGuard.h"
#include "Memory\BufferWriter.h"
#include "Memory\BufferReader.h"
#include "Crypto\Crypto.h"

using namespace QuantumGate::Implementation;
using namespace QuantumGate::Implementation::Memory;
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
			LogErr(GetName() + L": couldn't set one or more extender callbacks");
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
				StartupListener();
			}
		}
		else
		{
			if (IsRunning())
			{
				ShutdownListener();
			}
		}
	}

	const bool Extender::SetCredentials(const ProtectedStringA& username, const ProtectedStringA& password)
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

	const bool Extender::CheckCredentials(const BufferView& username, const BufferView& password) const
	{
		if (!m_RequireAuthentication) return true;

		if (!username.IsEmpty() && !password.IsEmpty())
		{
			ProtectedBuffer tmp(username.GetSize());
			memcpy(tmp.GetBytes(), username.GetBytes(), username.GetSize());

			ProtectedBuffer usrhash;
			if (Crypto::Hash(tmp, usrhash, Algorithm::Hash::BLAKE2B512))
			{
				if (m_Username == usrhash)
				{
					tmp.Resize(password.GetSize());
					memcpy(tmp.GetBytes(), password.GetBytes(), password.GetSize());

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

	const bool Extender::IsOutgoingIPAllowed(const IPAddress& ip) const noexcept
	{
		if (const auto result = m_IPFilters.WithSharedLock()->IsAllowed(ip); result.Succeeded())
		{
			return *result;
		}

		return false;
	}

	const bool Extender::OnStartup()
	{
		LogDbg(L"Extender '" + GetName() + L"' starting...");

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
		LogDbg(L"Extender '" + GetName() + L"' running...");
	}

	void Extender::OnPreShutdown()
	{
		LogDbg(L"Extender '" + GetName() + L"' will begin shutting down...");

		// Stop accepting incoming connections
		ShutdownListener();

		// Disconnect all connections gracefully
		DisconnectAll();
	}

	void Extender::OnShutdown()
	{
		LogDbg(L"Extender '" + GetName() + L"' shutting down...");

		ShutdownThreadPool();

		m_Peers.WithUniqueLock()->clear();
		m_Connections.WithUniqueLock()->clear();

		DeInitializeIPFilters();
	}

	const bool Extender::InitializeIPFilters()
	{
		bool success = true;

		// Block internal networks to prevent incoming connections
		// from connecting to internal addresses
		std::vector<String> internal_nets = {
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
			for (auto net : internal_nets)
			{
				const auto result = filters.AddFilter(net, IPFilterType::Blocked);
				if (result.Failed())
				{
					LogErr(GetName() + L": could not add %s to IP filters", net.c_str());
					success = false;
					break;
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

	const bool Extender::StartupListener()
	{
		std::unique_lock<std::shared_mutex> lock(m_Listener.Mutex);

		LogInfo(GetName() + L": listener starting...");

		m_Listener.ShutdownEvent.Reset();

		auto endpoint = IPEndpoint(IPAddress::AnyIPv4(), 9090);
		m_Listener.Socket = Network::Socket(endpoint.GetIPAddress().GetFamily(),
											SOCK_STREAM, IPPROTO_TCP);
		if (m_Listener.Socket.Listen(endpoint, false, false))
		{
			LogInfo(GetName() + L": listening on endpoint %s", endpoint.GetString().c_str());

			m_Listener.Thread = std::thread(Extender::ListenerThreadLoop, this);

			return true;
		}

		return false;
	}

	void Extender::ShutdownListener()
	{
		std::unique_lock<std::shared_mutex> lock(m_Listener.Mutex);

		LogInfo(GetName() + L": listener shutting down...");

		m_Listener.ShutdownEvent.Set();

		if (m_Listener.Thread.joinable())
		{
			// Wait for the thread to shut down
			m_Listener.Thread.join();
		}
	}

	const bool Extender::StartupThreadPool()
	{
		m_ThreadPool.SetWorkerThreadsMaxBurst(50);
		m_ThreadPool.SetWorkerThreadsMaxSleep(64ms);

		if (m_ThreadPool.AddThread(GetName() + L" Main Worker Thread",
								   &Extender::MainWorkerThreadLoop, ThreadData()))
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
			case PeerEventType::Connected:
			{
				ev = L"Connect";

				Peer peer;
				peer.ID = event.GetPeerLUID();

				m_Peers.WithUniqueLock()->insert({ event.GetPeerLUID(), std::move(peer) });

				break;
			}
			case PeerEventType::Disconnected:
			{
				ev = L"Disconnect";

				m_Peers.WithUniqueLock()->erase(event.GetPeerLUID());

				DisconnectFor(event.GetPeerLUID());

				break;
			}
			default:
			{
				assert(false);
			}
		}

		LogInfo(L"Extender '" + GetName() + L"' got peer event: %s, Peer LUID: %llu", ev.c_str(), event.GetPeerLUID());
	}

	const std::pair<bool, bool> Extender::OnPeerMessage(PeerEvent&& event)
	{
		assert(event.GetType() == PeerEventType::Message);

		auto handled = false;
		auto success = false;

		auto msgdata = event.GetMessageData();
		if (msgdata != nullptr)
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
						handled = true;

						ConnectionID cid{ 0 };
						String domain;
						UInt16 port{ 0 };

						if (rdr.Read(cid, WithSize(domain, MaxSize::_1KB), port))
						{
							success = HandleConnectDomainPeerMessage(event.GetPeerLUID(), cid, domain, port);
						}
						else LogErr(GetName() + L": could not read ConnectDomain message from peer %llu", event.GetPeerLUID());

						break;
					}
					case MessageType::ConnectIP:
					{
						handled = true;

						ConnectionID cid{ 0 };
						Network::SerializedBinaryIPAddress ip;
						UInt16 port{ 0 };

						if (rdr.Read(cid, ip, port))
						{
							success = HandleConnectIPPeerMessage(event.GetPeerLUID(), cid, ip, port);
						}
						else LogErr(GetName() + L": could not read ConnectIP message from peer %llu", event.GetPeerLUID());

						break;
					}
					case MessageType::Socks5ReplyRelay:
					{
						handled = true;

						ConnectionID cid{ 0 };
						Socks5Protocol::Replies reply{ Socks5Protocol::Replies::GeneralFailure };
						Socks5Protocol::AddressTypes atype{ Socks5Protocol::AddressTypes::Unknown };
						Network::SerializedBinaryIPAddress ip;
						UInt16 port{ 0 };

						if (rdr.Read(cid, reply, atype, ip, port))
						{
							success = HandleSocks5ReplyRelayPeerMessage(event.GetPeerLUID(), cid, reply, atype,
																		BufferView(reinterpret_cast<Byte*>(&ip.Bytes),
																				   sizeof(Network::SerializedBinaryIPAddress::Bytes)), port);
						}
						else LogErr(GetName() + L": could not read Socks5ReplyRelay message from peer %llu",
									event.GetPeerLUID());

						break;
					}
					case MessageType::DataRelay:
					{
						handled = true;

						ConnectionID cid{ 0 };
						Buffer data;

						if (rdr.Read(cid, WithSize(data, GetMaximumMessageDataSize())))
						{
							auto con = GetConnection(event.GetPeerLUID(), cid);
							if (con)
							{
								con->WithUniqueLock([&](Connection& connection)
								{
									if (!connection.SendRelayedData(std::move(data)))
									{
										LogErr(GetName() + L": error sending relayed data to connection %llu", cid);
										connection.SetDisconnectCondition();
									}
								});

								success = true;
							}
							else
							{
								LogErr(GetName() + L": received DataRelay from peer %llu for unknown connection %llu",
									   event.GetPeerLUID(), cid);
							}
						}
						else LogErr(GetName() + L": could not read DataRelay message from peer %llu", event.GetPeerLUID());

						break;
					}
					case MessageType::Disconnect:
					{
						handled = true;

						ConnectionID cid{ 0 };

						if (rdr.Read(cid))
						{
							auto con = GetConnection(event.GetPeerLUID(), cid);
							if (con)
							{
								con->WithUniqueLock([&](Connection& connection)
								{
									connection.SetPeerConnected(false);
									connection.SetDisconnectCondition();

									SendDisconnectAck(event.GetPeerLUID(), cid);
								});

								success = true;
							}
							else
							{
								LogErr(GetName() + L": received Disconnect from peer %llu for unknown connection %llu",
									   event.GetPeerLUID(), cid);
							}
						}
						else LogErr(GetName() + L": could not read Disconnect message from peer %llu", event.GetPeerLUID());

						break;
					}
					case MessageType::DisconnectAck:
					{
						handled = true;

						ConnectionID cid{ 0 };

						if (rdr.Read(cid))
						{
							auto con = GetConnection(event.GetPeerLUID(), cid);
							if (con)
							{
								con->WithUniqueLock([](Connection& connection) noexcept
								{
									connection.SetPeerConnected(false);
									connection.SetStatus(Connection::Status::Disconnected);
								});

								success = true;
							}
							else
							{
								LogErr(GetName() + L": received DisconnectAck from peer %llu for unknown connection %llu",
									   event.GetPeerLUID(), cid);
							}
						}
						else LogErr(GetName() + L": could not read DisconnectAck message from peer %llu",
									event.GetPeerLUID());

						break;
					}
					default:
					{
						LogErr(GetName() + L": received unknown message type from %llu: %u", event.GetPeerLUID(), mtype);
						break;
					}
				}
			}
		}

		return std::make_pair(handled, success);
	}

	const bool Extender::HandleConnectDomainPeerMessage(const PeerLUID pluid, const ConnectionID cid,
														const String& domain, const UInt16 port)
	{
		if (!domain.empty() && port != 0)
		{
			LogDbg(GetName() + L": received ConnectDomain from peer %llu for connection %llu", pluid, cid);

			const auto ip = ResolveDomainIP(domain);
			if (ip)
			{
				MakeOutgoingConnection(pluid, cid, *ip, port);
			}
			else
			{
				// Could not resolve domain
				SendSocks5Reply(pluid, cid, Socks5Protocol::Replies::HostUnreachable);
			}

			return true;
		}
		else LogErr(GetName() + L": received invalid ConnectDomain parameters from peer %llu", pluid);

		return false;
	}

	const bool Extender::HandleConnectIPPeerMessage(const PeerLUID pluid, const ConnectionID cid,
													const Network::BinaryIPAddress& ip, const UInt16 port)
	{
		if ((ip.AddressFamily == IPAddressFamily::IPv4 || ip.AddressFamily == IPAddressFamily::IPv6) && port != 0)
		{
			LogDbg(GetName() + L": received ConnectIP from peer %llu for connection %llu", pluid, cid);

			MakeOutgoingConnection(pluid, cid, IPAddress(ip), port);

			return true;
		}
		else LogErr(GetName() + L": received invalid ConnectIP parameters from peer %llu", pluid);

		return false;
	}

	const bool Extender::HandleSocks5ReplyRelayPeerMessage(const PeerLUID pluid, const ConnectionID cid,
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

									connection.SendSocks5Reply(reply, atype, address, port);
								}
							});

							return true;
						}
						else
						{
							LogErr(GetName() + L": received Socks5ReplyRelay (%u) from peer %llu for unknown connection ID %llu",
								   reply, pluid, cid);
						}

						break;
					}
					default:
					{
						LogErr(GetName() + L": received unsupported address type from %llu: %u", pluid, atype);
						break;
					}
				}

				break;
			}
			default:
			{
				LogErr(GetName() + L": received unknown Socks5 reply from %llu: %u", pluid, reply);
				break;
			}
		}

		return false;
	}

	const bool Extender::AddConnection(const PeerLUID pluid, const ConnectionID cid,
									   std::unique_ptr<Connection_ThS>&& c) noexcept
	{
		auto success = false;

		m_Connections.WithUniqueLock([&](Connections& connections)
		{
			auto key = c->WithSharedLock()->GetKey();

			[[maybe_unused]] const auto[it, inserted] = connections.insert({ key, std::move(c) });

			assert(inserted);
			success = inserted;
		});

		if (!success)
		{
			LogErr(GetName() + L": could not add new connection");
		}

		return success;
	}

	Connection_ThS* Extender::GetConnection(const PeerLUID pluid,
											const ConnectionID cid) const noexcept
	{
		Connection_ThS* con{ nullptr };

		m_Connections.WithSharedLock([&](const Connections& connections)
		{
			auto it = connections.find(Connection::MakeKey(pluid, cid));
			if (it != connections.end())
			{
				con = it->second.get();
			}
		});

		return con;
	}

	const void Extender::Disconnect(Connection_ThS& c)
	{
		c.WithUniqueLock([&](Connection& connection)
		{
			Disconnect(connection);
		});
	}

	const void Extender::Disconnect(Connection& c)
	{
		if (c.IsActive())
		{
			c.Disconnect();
		}
	}

	const void Extender::DisconnectFor(const PeerLUID pluid)
	{
		LogInfo(GetName() + L": disconnecting connections for peer %llu", pluid);

		m_Connections.WithSharedLock([&](const Connections& connections)
		{
			for (auto& connection : connections)
			{
				connection.second->WithUniqueLock([&](Connection& c) noexcept
				{
					if (c.GetPeerLUID() == pluid)
					{
						c.SetPeerConnected(false);
						c.SetDisconnectCondition();
					}
				});
			}
		});
	}

	const void Extender::DisconnectAll()
	{
		LogInfo(GetName() + L": disconnecting all connections");

		m_Connections.WithUniqueLock([&](const Connections& connections)
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

		auto extname = extender->GetName();

		LogDbg(extname + L": listener thread %u starting", std::this_thread::get_id());

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
					LogDbg(extname + L": accepting new incoming connection");

					extender->AcceptIncomingConnection();
				}
				else if (extender->m_Listener.Socket.GetIOStatus().HasException())
				{
					LogErr(extname + L": exception on listener socket (%s)",
						   GetSysErrorString(extender->m_Listener.Socket.GetIOStatus().GetErrorCode()).c_str());
					break;
				}
			}
			else
			{
				LogErr(extname + L": could not get status of listener socket");
				break;
			}
		}

		if (extender->m_Listener.Socket.GetIOStatus().IsOpen()) extender->m_Listener.Socket.Close();

		LogDbg(extname + L": listener thread %u exiting", std::this_thread::get_id());
	}

	const std::pair<bool, bool> Extender::MainWorkerThreadLoop(ThreadPoolData& thpdata,
															   ThreadData& thdata,
															   const Concurrency::EventCondition& shutdown_event)
	{
		auto didwork = false;
		std::vector<UInt64> rlist;

		thpdata.Extender.m_Connections.IfSharedLock([&](const Connections& connections)
		{
			for (auto it = connections.begin(); it != connections.end() && !shutdown_event.IsSet(); ++it)
			{
				it->second->IfUniqueLock([&](Connection& connection)
				{
					if (connection.IsActive())
					{
						connection.ProcessEvents(didwork);

						if (connection.IsTimedOut())
						{
							LogDbg(thpdata.Extender.GetName() + L": connection %llu timed out", connection.GetID());

							connection.SetDisconnectCondition();
						}
					}
					else if (connection.IsDisconnected() ||
						(connection.IsDisconnecting() && connection.IsTimedOut()))
					{
						LogDbg(thpdata.Extender.GetName() + L": removing connection %llu", connection.GetID());

						rlist.emplace_back(connection.GetKey());
					}
				});
			}
		});

		if (!rlist.empty())
		{
			thpdata.Extender.m_Connections.IfUniqueLock([&](Connections& connections)
			{
				for (auto key : rlist)
				{
					connections.erase(key);
				}
			});

			rlist.clear();
			didwork = true;
		}

		return std::make_pair(true, didwork);
	}

	void Extender::AcceptIncomingConnection()
	{
		Socket s;
		if (m_Listener.Socket.Accept(s))
		{
			const auto pluid = GetPeerForConnection();
			if (pluid)
			{
				auto endp = s.GetPeerEndpoint().GetString();

				auto cths = std::make_unique<Connection_ThS>(*this, *pluid, std::move(s));

				const auto cid = cths->WithSharedLock()->GetID();

				AddConnection(*pluid, cid, std::move(cths));

				LogInfo(GetName() + L": accepted connection %llu from endpoint %s and associated with peer %llu",
						cid, endp.c_str(), *pluid);
			}
			else
			{
				LogErr(GetName() + L": found no peers to associate with socket %s",
					   s.GetPeerEndpoint().GetString().c_str());

				s.Close();
			}
		}
		else LogErr(GetName() + L": could not accept new connection");
	}

	const bool Extender::SendConnectDomain(const PeerLUID pluid, const ConnectionID cid,
										   const String & domain, const UInt16 port) const
	{
		const UInt16 msgtype = static_cast<const UInt16>(MessageType::ConnectDomain);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid, WithSize(domain, MaxSize::_1KB), port))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
			{
				return true;
			}
			else LogErr(GetName() + L": could not send ConnectDomain message for connection %llu to peer %llu", cid, pluid);
		}
		else LogErr(GetName() + L": could not prepare ConnectDomain message for connection %llu", cid);

		return false;
	}

	const bool Extender::SendConnectIP(const PeerLUID pluid, const ConnectionID cid,
									   const Network::BinaryIPAddress& ip, const UInt16 port) const
	{
		assert(ip.AddressFamily != IPAddressFamily::Unknown);

		const UInt16 msgtype = static_cast<const UInt16>(MessageType::ConnectIP);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid, Network::SerializedBinaryIPAddress{ ip }, port))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
			{
				return true;
			}
			else LogErr(GetName() + L": could not send ConnectIP message for connection %llu to peer %llu", cid, pluid);
		}
		else LogErr(GetName() + L": could not prepare ConnectIP message for connection %llu", cid);

		return false;
	}

	const bool Extender::SendDisconnect(const PeerLUID pluid, const ConnectionID cid) const
	{
		const UInt16 msgtype = static_cast<const UInt16>(MessageType::Disconnect);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
			{
				return true;
			}
			else LogErr(GetName() + L": could not send Disconnect message for connection %llu to peer %llu", cid, pluid);
		}
		else LogErr(GetName() + L": could not prepare Disconnect message for connection %llu", cid);

		return false;
	}

	const bool Extender::SendDisconnectAck(const PeerLUID pluid, const ConnectionID cid) const
	{
		const UInt16 msgtype = static_cast<const UInt16>(MessageType::DisconnectAck);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
			{
				return true;
			}
			else LogErr(GetName() + L": could not send DisconnectAck message for connection %llu to peer %llu", cid, pluid);
		}
		else LogErr(GetName() + L": could not prepare DisconnectAck message for connection %llu", cid);

		return false;
	}

	const bool Extender::SendSocks5Reply(const PeerLUID pluid, const ConnectionID cid,
										 const Socks5Protocol::Replies reply,
										 const Socks5Protocol::AddressTypes atype,
										 const Network::BinaryIPAddress ip, const UInt16 port) const
	{
		const UInt16 msgtype = static_cast<const UInt16>(MessageType::Socks5ReplyRelay);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid, reply, atype, Network::SerializedBinaryIPAddress{ ip }, port))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
			{
				return true;
			}
			else LogErr(GetName() + L": could not send Socks5ReplyRelay message for connection %llu to peer %llu", cid, pluid);
		}
		else LogErr(GetName() + L": could not prepare Socks5ReplyRelay message for connection %llu", cid);

		return false;
	}

	const bool Extender::SendDataRelay(const PeerLUID pluid, const ConnectionID cid,
									   const BufferView& buffer) const
	{
		const UInt16 msgtype = static_cast<UInt16>(MessageType::DataRelay);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, cid, WithSize(buffer, GetMaximumMessageDataSize())))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
			{
				return true;
			}
			else LogErr(GetName() + L": could not send DataRelay message for connection %llu to peer %llu", cid, pluid);
		}
		else LogErr(GetName() + L": could not prepare DataRelay message for connection %llu", cid);

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
								  static_cast<Size>(Util::GetPseudoRandomNumber(0, peers.size() - 1u)))->second.ID;
			}
		});

		return pluid;
	}

	const bool Extender::MakeOutgoingConnection(const PeerLUID pluid, const ConnectionID cid,
												const IPAddress& ip, const UInt16 port)
	{
		if (IsOutgoingIPAllowed(ip))
		{
			IPEndpoint endp(ip, port);
			Socket s(endp.GetIPAddress().GetFamily());

			LogInfo(GetName() + L": connecting to %s for peer %llu for connection %llu",
					endp.GetString().c_str(), pluid, cid);

			if (s.BeginConnect(endp))
			{
				auto cths = std::make_unique<Connection_ThS>(*this, pluid, cid, std::move(s));

				cths->WithUniqueLock()->SetPeerConnected(true);

				return AddConnection(pluid, cid, std::move(cths));
			}
			else
			{
				// Could not connect
				SendSocks5Reply(pluid, cid, TranslateWSAErrorToSocks5(WSAGetLastError()));
			}
		}
		else
		{
			LogErr(GetName() + L": attempt by peer %llu (connection %llu) to connect to address %s that is not allowed",
				   pluid, cid, ip.GetString().c_str());

			SendSocks5Reply(pluid, cid, Socks5Protocol::Replies::ConnectionNotAllowed);
		}

		return false;
	}

	std::optional<IPAddress> Extender::ResolveDomainIP(const String& domain) const noexcept
	{
		ADDRINFOW* result{ nullptr };

		const auto ret = GetAddrInfoW(domain.c_str(), L"0", nullptr, &result);
		if (ret == 0)
		{
			// Free ADDRINFO resources when we leave
			auto sg = MakeScopeGuard([&] { FreeAddrInfoW(result); });

			for (auto ptr = result; ptr != nullptr; ptr = ptr->ai_next)
			{
				if (ptr->ai_family == AF_INET || ptr->ai_family == AF_INET6)
				{
					return { IPAddress(ptr->ai_addr) };
				}
			}
		}
		else LogErr(GetName() + L": could not resolve IP addresses for domain %s", domain.c_str());

		return std::nullopt;
	}

	const Socks5Protocol::Replies Extender::TranslateWSAErrorToSocks5(Int errorcode) const noexcept
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