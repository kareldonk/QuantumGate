// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "ListenerManager.h"
#include "..\API\Access.h"

using namespace std::literals;
using namespace QuantumGate::Implementation::Network;

namespace QuantumGate::Implementation::Core::Listener
{
	Manager::Manager(const Settings_CThS& settings, Access::Manager& accessmgr, Peer::Manager& peers) noexcept :
		m_Settings(settings), m_AccessManager(accessmgr), m_PeerManager(peers)
	{}

	// Starts listening on the default interfaces
	bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"Listenermanager starting up...");

		PreStartup();

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.Listeners.TCP.Ports;
		const auto nat_traversal = settings.Local.Listeners.NATTraversal;
		const auto cond_accept = settings.Local.Listeners.TCP.UseConditionalAcceptFunction;

		// Should have at least one port
		if (listener_ports.empty())
		{
			LogErr(L"Listenermanager startup failed; no ports given");
			return false;
		}

		const std::array<IPAddress::Family, 2> afs{ IPAddress::Family::IPv4, IPAddress::Family::IPv6 };

		for (const auto& af : afs)
		{
			const std::optional<IPAddress> address = std::invoke([&]() -> std::optional<IPAddress>
			{
				switch (af)
				{
					case IPAddress::Family::IPv4:
						return IPAddress::AnyIPv4();
						break;
					case IPAddress::Family::IPv6:
						return IPAddress::AnyIPv6();
						break;
					default:
						assert(false);
						break;
				}

				return std::nullopt;
			});

			if (address.has_value())
			{
				DiscardReturnValue(AddTCPListenerThreads(*address, listener_ports, cond_accept, nat_traversal));
			}
		}

		if (m_ListenerThreadPool.Startup())
		{
			m_Running = true;
			m_ListeningOnAnyAddresses = true;

			LogSys(L"Listenermanager startup successful");
		}
		else LogErr(L"Listenermanager startup failed");

		return m_Running;
	}

	// Starts listening on all active interfaces
	bool Manager::Startup(const Vector<API::Local::Environment::EthernetInterface>& interfaces) noexcept
	{
		if (m_Running) return true;

		LogSys(L"Listenermanager starting...");

		PreStartup();

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.Listeners.TCP.Ports;
		const auto nat_traversal = settings.Local.Listeners.NATTraversal;
		const auto cond_accept = settings.Local.Listeners.TCP.UseConditionalAcceptFunction;

		// Should have at least one port
		if (listener_ports.empty())
		{
			LogErr(L"Listenermanager startup failed; no ports given");
			return false;
		}

		// Create a listening socket for each interface that's online
		for (const auto& ifs : interfaces)
		{
			if (ifs.Operational)
			{
				for (const auto& address : ifs.IPAddresses)
				{
					// Only for IPv4 and IPv6 addresses
					if (address.GetFamily() == IPAddress::Family::IPv4 || address.GetFamily() == IPAddress::Family::IPv6)
					{
						DiscardReturnValue(AddTCPListenerThreads(address, listener_ports, cond_accept, nat_traversal));
					}
					else assert(false);
				}
			}
		}

		if (m_ListenerThreadPool.Startup())
		{
			m_Running = true;
			m_ListeningOnAnyAddresses = false;

			LogSys(L"Listenermanager startup successful");
		}
		else LogErr(L"Listenermanager startup failed");

		return m_Running;
	}

	bool Manager::AddTCPListenerThreads(const IPAddress& address, const Vector<UInt16> ports,
										const bool cond_accept, const bool nat_traversal) noexcept
	{
		// Separate listener for every port
		for (const auto port : ports)
		{
			try
			{
				const auto endpoint = IPEndpoint(IPEndpoint::Protocol::TCP, address, port);

				TCPListenerData ldata{
					.Socket = Network::Socket(endpoint.GetIPAddress().GetFamily(), Network::Socket::Type::Stream, Network::IP::Protocol::TCP),
					.UseConditionalAcceptFunction = cond_accept
				};

				// Create and start the listenersocket
				if (ldata.Socket.Listen(endpoint, true, nat_traversal))
				{
					ThreadData ltd(std::move(ldata));

					if (m_ListenerThreadPool.AddThread(L"QuantumGate Listener Thread (TCP) " + endpoint.GetString(),
													   std::move(ltd), MakeCallback(this, &Manager::WorkerThreadProcessor)))
					{
						LogSys(L"Listening on endpoint %s", endpoint.GetString().c_str());
					}
					else
					{
						LogErr(L"Could not add listener thread for endpoint %s", endpoint.GetString().c_str());
					}
				}
			}
			catch (const std::exception& e)
			{
				LogErr(L"Could not add listener thread for IP %s due to exception: %s",
					   address.GetString().c_str(), Util::ToStringW(e.what()).c_str());
			}
			catch (...) {}
		}

		return true;
	}

	bool Manager::AddUDPListenerThreads(const IPAddress& address, const Vector<UInt16> ports, const bool nat_traversal) noexcept
	{
		// Separate listener for every port
		for (const auto port : ports)
		{
			try
			{
				const auto endpoint = IPEndpoint(IPEndpoint::Protocol::UDP, address, port);

				UDPListenerData ldata{
					.Socket = Network::Socket(endpoint.GetIPAddress().GetFamily(), Network::Socket::Type::Datagram, Network::IP::Protocol::UDP),
				};

				// Create and start the listenersocket
				if (ldata.Socket.Bind(endpoint, nat_traversal))
				{
					ThreadData ltd(std::move(ldata));

					if (m_ListenerThreadPool.AddThread(L"QuantumGate Listener Thread (UDP) " + endpoint.GetString(),
													   std::move(ltd), MakeCallback(this, &Manager::WorkerThreadProcessor)))
					{
						LogSys(L"Listening on endpoint %s", endpoint.GetString().c_str());
					}
					else
					{
						LogErr(L"Could not add listener thread for endpoint %s", endpoint.GetString().c_str());
					}
				}
			}
			catch (const std::exception& e)
			{
				LogErr(L"Could not add listener thread for IP %s due to exception: %s",
					   address.GetString().c_str(), Util::ToStringW(e.what()).c_str());
			}
			catch (...) {}
		}

		return true;
	}

	std::optional<Manager::ThreadPool::Thread> Manager::RemoveListenerThread(Manager::ThreadPool::Thread&& thread) noexcept
	{
		IPEndpoint endpoint;
		String type;

		std::visit(Util::Overloaded{
			[&](TCPListenerData& data)
			{
				endpoint = data.Socket.GetLocalEndpoint();
			},
			[&](UDPListenerData& data)
			{
				endpoint = data.Socket.GetLocalEndpoint();
			}
		}, thread.GetData().ListenerData);

		const auto [success, next_thread] = m_ListenerThreadPool.RemoveThread(std::move(thread));
		if (success)
		{
			LogSys(L"Stopped listening on endpoint %s", endpoint.GetString().c_str());
		}
		else
		{
			LogErr(L"Could not remove listener thread for endpoint %s", endpoint.GetString().c_str());
		}

		return next_thread;
	}

	bool Manager::Update(const Vector<API::Local::Environment::EthernetInterface>& interfaces) noexcept
	{
		if (!m_Running) return false;

		// No need to update in this case
		if (m_ListeningOnAnyAddresses) return true;

		LogSys(L"Updating Listenermanager...");

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.Listeners.TCP.Ports;
		const auto nat_traversal = settings.Local.Listeners.NATTraversal;
		const auto cond_accept = settings.Local.Listeners.TCP.UseConditionalAcceptFunction;

		// Check for interfaces/IP addresses that were added for which
		// there are no listeners; we add listeners for those
		for (const auto& ifs : interfaces)
		{
			if (ifs.Operational)
			{
				for (const auto& address : ifs.IPAddresses)
				{
					// Only for IPv4 and IPv6 addresses
					if (address.GetFamily() == IP::AddressFamily::IPv4 || address.GetFamily() == IP::AddressFamily::IPv6)
					{
						auto found = false;

						auto thread = m_ListenerThreadPool.GetFirstThread();

						while (thread.has_value())
						{
							std::visit(Util::Overloaded{
								[&](const TCPListenerData& data)
								{
									found = (data.Socket.GetLocalIPAddress() == address);
								},
								[&](const UDPListenerData& data)
								{
									found = (data.Socket.GetLocalIPAddress() == address);
								}
							}, thread->GetData().ListenerData);

							if (found)
							{
								break;
							}
							else thread = m_ListenerThreadPool.GetNextThread(*thread);
						}

						if (!found)
						{
							DiscardReturnValue(AddTCPListenerThreads(address, listener_ports, cond_accept, nat_traversal));
						}
					}
				}
			}
		}

		// Check for interfaces/IP addresses that were removed for which
		// there are still listeners; we remove listeners for those
		auto thread = m_ListenerThreadPool.GetFirstThread();

		while (thread.has_value())
		{
			auto found = false;

			for (const auto& ifs : interfaces)
			{
				if (ifs.Operational)
				{
					for (const auto& address : ifs.IPAddresses)
					{
						std::visit(Util::Overloaded{
							[&](const TCPListenerData& data)
							{
								found = (data.Socket.GetLocalIPAddress() == address);
							},
							[&](const UDPListenerData& data)
							{
								found = (data.Socket.GetLocalIPAddress() == address);
							}
						}, thread->GetData().ListenerData);

						if (found) break;
					}
				}

				if (found) break;
			}

			if (!found)
			{
				thread = RemoveListenerThread(std::move(*thread));
			}
			else thread = m_ListenerThreadPool.GetNextThread(*thread);
		}

		return true;
	}

	void Manager::Shutdown() noexcept
	{
		if (!m_Running) return;

		m_Running = false;

		LogSys(L"Listenermanager shutting down...");

		m_ListenerThreadPool.Shutdown();

		ResetState();

		LogSys(L"Listenermanager shut down");
	}

	void Manager::PreStartup() noexcept
	{
		ResetState();
	}

	void Manager::ResetState() noexcept
	{
		m_ListeningOnAnyAddresses = false;
		m_ListenerThreadPool.Clear();
	}

	void Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		Network::Socket* socket{ nullptr };

		std::visit(Util::Overloaded{
			[&](TCPListenerData& data)
			{
				socket = &data.Socket;
			},
			[&](UDPListenerData& data)
			{
				socket = &data.Socket;
			}
		}, thdata.ListenerData);

		while (!shutdown_event.IsSet())
		{
			// Check if we have a read event waiting for us
			if (socket->UpdateIOStatus(1ms))
			{
				if (socket->GetIOStatus().CanRead())
				{
					// Probably have a connection waiting to accept
					LogInfo(L"Accepting new connection on endpoint %s",
							socket->GetLocalEndpoint().GetString().c_str());

					std::visit(Util::Overloaded{
						[&](TCPListenerData& data)
						{
							AcceptTCPConnection(*socket, data.UseConditionalAcceptFunction);
						},
						[&](UDPListenerData& data)
						{
							AcceptUDPConnection(*socket);
						}
					}, thdata.ListenerData);
				}
				else if (socket->GetIOStatus().HasException())
				{
					LogErr(L"Exception on listener socket for endpoint %s (%s)",
						   socket->GetLocalEndpoint().GetString().c_str(),
						   GetSysErrorString(socket->GetIOStatus().GetErrorCode()).c_str());
				}
			}
			else
			{
				LogErr(L"Could not get status of listener socket for endpoint %s",
					   socket->GetLocalEndpoint().GetString().c_str());
			}
		}
	}

	void Manager::AcceptTCPConnection(Network::Socket& listener_socket, const bool cond_accept) noexcept
	{
		auto peerths = m_PeerManager.Create(PeerConnectionType::Inbound, std::nullopt);
		if (peerths != nullptr)
		{
			peerths->WithUniqueLock([&](Peer::Peer& peer)
			{
				if (cond_accept)
				{
					if (!listener_socket.Accept(peer.GetSocket<Network::Socket>(), true,
												&Manager::TCPAcceptConditionFunction, this))
					{
						// Couldn't accept for some reason
						return;
					}
				}
				else
				{
					if (listener_socket.Accept(peer.GetSocket<Network::Socket>(), false, nullptr, nullptr))
					{
						// Check if the IP address is allowed
						if (!CanAcceptConnection(peer.GetPeerIPAddress()))
						{
							peer.Close();
							LogWarn(L"Incoming connection from peer %s was rejected; IP address is not allowed by access configuration",
									peer.GetPeerName().c_str());

							return;
						}
					}
				}

				if (m_PeerManager.Accept(peerths))
				{
					LogInfo(L"Connection accepted from peer %s", peer.GetPeerName().c_str());
				}
				else
				{
					peer.Close();
					LogErr(L"Could not accept connection from peer %s", peer.GetPeerName().c_str());
				}
			});
		}
	}

	void Manager::AcceptUDPConnection(Network::Socket& listener_socket) noexcept
	{
		auto peerths = m_PeerManager.Create(PeerConnectionType::Inbound, std::nullopt);
		if (peerths != nullptr)
		{
			peerths->WithUniqueLock([&](Peer::Peer& peer)
			{
				if (listener_socket.Accept(peer.GetSocket<Network::Socket>(), false, nullptr, nullptr))
				{
					// Check if the IP address is allowed
					if (!CanAcceptConnection(peer.GetPeerIPAddress()))
					{
						peer.Close();
						LogWarn(L"Incoming connection from peer %s was rejected; IP address is not allowed by access configuration",
								peer.GetPeerName().c_str());

						return;
					}
				}

				if (m_PeerManager.Accept(peerths))
				{
					LogInfo(L"Connection accepted from peer %s", peer.GetPeerName().c_str());
				}
				else
				{
					peer.Close();
					LogErr(L"Could not accept connection from peer %s", peer.GetPeerName().c_str());
				}
			});
		}
	}

	bool Manager::CanAcceptConnection(const IPAddress& ipaddr) const noexcept
	{
		// Increase connection attempts for this IP; if attempts get too high
		// for a given interval the IP will get a bad reputation and this will fail
		if (m_AccessManager.AddIPConnectionAttempt(ipaddr))
		{
			// Check if IP is allowed through filters/limits and if it has acceptible reputation
			if (const auto result = m_AccessManager.GetIPConnectionAllowed(ipaddr, Access::CheckType::All); result.Succeeded())
			{
				return *result;
			}
		}

		// If anything goes wrong we always deny access
		return false;
	}

	int CALLBACK Manager::TCPAcceptConditionFunction(LPWSABUF lpCallerId, LPWSABUF lpCallerData, LPQOS lpSQOS, LPQOS lpGQOS,
													 LPWSABUF lpCalleeId, LPWSABUF lpCalleeData, GROUP FAR* g,
													 DWORD_PTR dwCallbackData) noexcept
	{
		const IPEndpoint endpoint(IPEndpoint::Protocol::TCP, reinterpret_cast<sockaddr_storage*>(lpCallerId->buf));

		if (reinterpret_cast<Manager*>(dwCallbackData)->CanAcceptConnection(endpoint.GetIPAddress()))
		{
			return CF_ACCEPT;
		}

		LogWarn(L"Incoming connection attempt from peer %s was rejected; IP address is not allowed by access configuration",
				endpoint.GetString().c_str());

		return CF_REJECT;
	}
}