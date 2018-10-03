// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "ListenerManager.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::Listener
{
	Manager::Manager(const Settings_CThS& settings, Access::Manager& accessmgr, Peer::Manager& peers) noexcept :
		m_Settings(settings), m_AccessManager(accessmgr), m_PeerManager(peers)
	{}

	// Starts listening on the default interfaces
	const bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"Listenermanager starting up...");

		PreStartup();

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.ListenerPorts;
		const auto nat_traversal = settings.Local.NATTraversal;
		const auto cond_accept = settings.Local.UseConditionalAcceptFunction;

		// Should have at least one port
		if (listener_ports.empty())
		{
			LogErr(L"Listenermanager startup failed; no ports given");
			return false;
		}

		std::array<IPAddressFamily, 2> afs{ IPAddressFamily::IPv4, IPAddressFamily::IPv6 };

		for (const auto& af : afs)
		{
			// Separate listener for every port
			for (const auto port : listener_ports)
			{
				try
				{
					IPEndpoint endpoint;
					ThreadData ltd;

					switch (af)
					{
						case IPAddressFamily::IPv4:
							endpoint = IPEndpoint(IPAddress::AnyIPv4(), port);
							break;
						case IPAddressFamily::IPv6:
							endpoint = IPEndpoint(IPAddress::AnyIPv6(), port);
							break;
						default:
							assert(false);
							break;
					}

					ltd.UseConditionalAcceptFunction = cond_accept;

					// Create and start the listenersocket
					ltd.Socket = Network::Socket(endpoint.GetIPAddress().GetFamily(), SOCK_STREAM, IPPROTO_TCP);

					if (ltd.Socket.Listen(endpoint, true, nat_traversal))
					{
						LogSys(L"Listening on endpoint %s", endpoint.GetString().c_str());

						if (!m_ListenerThreadPool.AddThread(L"QuantumGate Listener Thread " + endpoint.GetString(),
															MakeCallback(this, &Manager::WorkerThreadProcessor),
															std::move(ltd)))
						{
							LogErr(L"Could not add listener thread");
						}
					}
				}
				catch (...) {}
			}
		}

		m_ListenerThreadPool.SetWorkerThreadsMaxBurst(settings.Local.Concurrency.WorkerThreadsMaxBurst);
		m_ListenerThreadPool.SetWorkerThreadsMaxSleep(settings.Local.Concurrency.WorkerThreadsMaxSleep);

		if (m_ListenerThreadPool.Startup())
		{
			m_Running = true;

			LogSys(L"Listenermanager startup successful");
		}
		else LogErr(L"Listenermanager startup failed");

		return m_Running;
	}

	// Starts listening on all active interfaces
	const bool Manager::Startup(const Vector<EthernetInterface>& interfaces) noexcept
	{
		if (m_Running) return true;

		LogSys(L"Listenermanager starting...");

		PreStartup();

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.ListenerPorts;
		const auto nat_traversal = settings.Local.NATTraversal;
		const auto cond_accept = settings.Local.UseConditionalAcceptFunction;

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
					if (address.GetFamily() == IPAddressFamily::IPv4 || address.GetFamily() == IPAddressFamily::IPv6)
					{
						// Separate listener for every port
						for (const auto port : listener_ports)
						{
							try
							{
								const auto endpoint = IPEndpoint(address, port);

								ThreadData ltd;
								ltd.UseConditionalAcceptFunction = cond_accept;

								// Create and start the listenersocket
								ltd.Socket = Network::Socket(endpoint.GetIPAddress().GetFamily(), SOCK_STREAM, IPPROTO_TCP);

								if (ltd.Socket.Listen(endpoint, true, nat_traversal))
								{
									LogSys(L"Listening on endpoint %s", endpoint.GetString().c_str());

									if (!m_ListenerThreadPool.AddThread(L"QuantumGate Listener Thread " + endpoint.GetString(),
																		MakeCallback(this, &Manager::WorkerThreadProcessor),
																		std::move(ltd)))
									{
										LogErr(L"Could not add listener thread");
									}
								}
							}
							catch (...) {}
						}
					}
					else assert(false);
				}
			}
		}

		m_ListenerThreadPool.SetWorkerThreadsMaxBurst(settings.Local.Concurrency.WorkerThreadsMaxBurst);
		m_ListenerThreadPool.SetWorkerThreadsMaxSleep(settings.Local.Concurrency.WorkerThreadsMaxSleep);

		if (m_ListenerThreadPool.Startup())
		{
			m_Running = true;

			LogSys(L"Listenermanager startup successful");
		}
		else LogErr(L"Listenermanager startup failed");

		return m_Running;
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
		m_ListenerThreadPool.Clear();
	}

	const std::pair<bool, bool> Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata,
															   const Concurrency::EventCondition& shutdown_event)
	{
		auto success = true;
		auto didwork = false;

		// Check if we have a read event waiting for us
		if (thdata.Socket.UpdateIOStatus(0ms))
		{
			if (thdata.Socket.GetIOStatus().CanRead())
			{
				// Probably have a connection waiting to accept
				LogInfo(L"Accepting new connection on endpoint %s",
						thdata.Socket.GetLocalEndpoint().GetString().c_str());

				AcceptConnection(thdata.Socket, thdata.UseConditionalAcceptFunction);

				didwork = true;
			}
			else if (thdata.Socket.GetIOStatus().HasException())
			{
				LogErr(L"Exception on listener socket for endpoint %s (%s)",
					   thdata.Socket.GetLocalEndpoint().GetString().c_str(),
					   GetSysErrorString(thdata.Socket.GetIOStatus().GetErrorCode()).c_str());

				success = false;
			}
		}
		else
		{
			LogErr(L"Could not get status of listener socket for endpoint %s",
				   thdata.Socket.GetLocalEndpoint().GetString().c_str());

			success = false;
		}

		return std::make_pair(success, didwork);
	}

	void Manager::AcceptConnection(Network::Socket& listener_socket, const bool cond_accept) noexcept
	{
		auto peerths = m_PeerManager.Create(PeerConnectionType::Inbound, std::nullopt);
		if (peerths != nullptr)
		{
			peerths->WithUniqueLock([&](Peer::Peer& peer)
			{
				if (cond_accept)
				{
					if (!listener_socket.Accept(peer.GetSocket<Network::Socket>(), true,
												&Manager::AcceptConditionFunction,
												this))
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
							LogWarn(L"Incoming connection from peer %s was rejected", peer.GetPeerName().c_str());

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

	const bool Manager::CanAcceptConnection(const IPAddress& ipaddr) const noexcept
	{
		// Increase connection attempts for this IP; if attempts get too high
		// for a given interval the IP will get a bad reputation and this will fail
		if (m_AccessManager.AddIPConnectionAttempt(ipaddr))
		{
			// Check if IP is allowed through filters/limits and if it has acceptible reputation
			if (const auto result = m_AccessManager.IsIPConnectionAllowed(ipaddr, AccessCheck::All); result.Succeeded())
			{
				return *result;
			}
		}

		// If anything goes wrong we always deny access
		return false;
	}

	int CALLBACK Manager::AcceptConditionFunction(LPWSABUF lpCallerId, LPWSABUF lpCallerData, LPQOS lpSQOS, LPQOS lpGQOS,
												  LPWSABUF lpCalleeId, LPWSABUF lpCalleeData, GROUP FAR* g,
												  DWORD_PTR dwCallbackData) noexcept
	{
		const IPEndpoint endpoint(reinterpret_cast<sockaddr_storage*>(lpCallerId->buf));

		if (reinterpret_cast<Manager*>(dwCallbackData)->CanAcceptConnection(endpoint.GetIPAddress()))
		{
			return CF_ACCEPT;
		}

		LogWarn(L"Incoming connection attempt from peer %s was rejected", endpoint.GetString().c_str());

		return CF_REJECT;
	}
}