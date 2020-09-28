// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPListenerManager.h"

using namespace std::literals;
using namespace QuantumGate::Implementation::Network;

namespace QuantumGate::Implementation::Core::UDP::Listener
{
	Manager::Manager(const Settings_CThS& settings, Access::Manager& accessmgr, UDP::Connection::Manager& udpmgr,
					 Peer::Manager& peermgr) noexcept :
		m_Settings(settings), m_AccessManager(accessmgr), m_UDPConnectionManager(udpmgr), m_PeerManager(peermgr)
	{}

	// Starts listening on the default interfaces
	bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"UDP listenermanager starting up...");

		PreStartup();

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.Listeners.UDP.Ports;
		const auto nat_traversal = settings.Local.Listeners.NATTraversal;

		// Should have at least one port
		if (listener_ports.empty())
		{
			LogErr(L"UDP listenermanager startup failed; no ports given");
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
				DiscardReturnValue(AddListenerThreads(*address, listener_ports, nat_traversal));
			}
		}

		if (m_ThreadPool.Startup())
		{
			m_Running = true;
			m_ListeningOnAnyAddresses = true;

			LogSys(L"UDP listenermanager startup successful");
		}
		else LogErr(L"UDP listenermanager startup failed");

		return m_Running;
	}

	// Starts listening on all active interfaces
	bool Manager::Startup(const Vector<API::Local::Environment::EthernetInterface>& interfaces) noexcept
	{
		if (m_Running) return true;

		LogSys(L"UDP listenermanager starting...");

		PreStartup();

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.Listeners.UDP.Ports;
		const auto nat_traversal = settings.Local.Listeners.NATTraversal;

		// Should have at least one port
		if (listener_ports.empty())
		{
			LogErr(L"UDP listenermanager startup failed; no ports given");
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
					if (address.GetFamily() == IPAddress::Family::IPv4 ||
						address.GetFamily() == IPAddress::Family::IPv6)
					{
						DiscardReturnValue(AddListenerThreads(address, listener_ports, nat_traversal));
					}
					else assert(false);
				}
			}
		}

		if (m_ThreadPool.Startup())
		{
			m_Running = true;
			m_ListeningOnAnyAddresses = false;

			LogSys(L"UDP listenermanager startup successful");
		}
		else LogErr(L"UDP listenermanager startup failed");

		return m_Running;
	}

	bool Manager::AddListenerThreads(const IPAddress& address, const Vector<UInt16> ports, const bool nat_traversal) noexcept
	{
		// Separate listener for every port
		for (const auto port : ports)
		{
			try
			{
				const auto endpoint = IPEndpoint(IPEndpoint::Protocol::UDP, address, port);

				// Create and start the listenersocket
				ThreadData ltd;
				ltd.Socket = std::make_shared<Socket_ThS>(endpoint.GetIPAddress().GetFamily(),
														  Network::Socket::Type::Datagram, Network::IP::Protocol::UDP);

				if (ltd.Socket->WithUniqueLock()->Bind(endpoint, nat_traversal))
				{
					if (m_ThreadPool.AddThread(L"QuantumGate Listener Thread " + endpoint.GetString(),
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

	std::optional<Manager::ThreadPool::ThreadType> Manager::RemoveListenerThread(Manager::ThreadPool::ThreadType&& thread) noexcept
	{
		const IPEndpoint endpoint = thread.GetData().Socket->WithSharedLock()->GetLocalEndpoint();

		const auto [success, next_thread] = m_ThreadPool.RemoveThread(std::move(thread));
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

		LogSys(L"Updating UDP listenermanager...");

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.Listeners.UDP.Ports;
		const auto nat_traversal = settings.Local.Listeners.NATTraversal;

		// Check for interfaces/IP addresses that were added for which
		// there are no listeners; we add listeners for those
		for (const auto& ifs : interfaces)
		{
			if (ifs.Operational)
			{
				for (const auto& address : ifs.IPAddresses)
				{
					// Only for IPv4 and IPv6 addresses
					if (address.GetFamily() == IP::AddressFamily::IPv4 ||
						address.GetFamily() == IP::AddressFamily::IPv6)
					{
						auto found = false;

						auto thread = m_ThreadPool.GetFirstThread();

						while (thread.has_value())
						{
							if (thread->GetData().Socket->WithSharedLock()->GetLocalIPAddress() == address)
							{
								found = true;
								break;
							}
							else thread = m_ThreadPool.GetNextThread(*thread);
						}

						if (!found)
						{
							DiscardReturnValue(AddListenerThreads(address, listener_ports, nat_traversal));
						}
					}
				}
			}
		}

		// Check for interfaces/IP addresses that were removed for which
		// there are still listeners; we remove listeners for those
		auto thread = m_ThreadPool.GetFirstThread();

		while (thread.has_value())
		{
			auto found = false;

			for (const auto& ifs : interfaces)
			{
				if (ifs.Operational)
				{
					for (const auto& address : ifs.IPAddresses)
					{
						if (thread->GetData().Socket->WithSharedLock()->GetLocalIPAddress() == address)
						{
							found = true;
							break;
						}
					}
				}

				if (found) break;
			}

			if (!found)
			{
				thread = RemoveListenerThread(std::move(*thread));
			}
			else thread = m_ThreadPool.GetNextThread(*thread);
		}

		return true;
	}

	void Manager::Shutdown() noexcept
	{
		if (!m_Running) return;

		m_Running = false;

		LogSys(L"UDP listenermanager shutting down...");

		m_ThreadPool.Shutdown();

		ResetState();

		LogSys(L"UDP listenermanager shut down");
	}

	void Manager::PreStartup() noexcept
	{
		ResetState();
	}

	void Manager::ResetState() noexcept
	{
		m_ListeningOnAnyAddresses = false;
		m_ThreadPool.Clear();
	}

	void Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		IPEndpoint lendpoint = thdata.Socket->WithSharedLock()->GetLocalEndpoint();
		IPEndpoint pendpoint;
		Buffer buffer;

		while (!shutdown_event.IsSet())
		{
			auto accept = false;

			thdata.Socket->WithUniqueLock([&](auto& socket)
			{
				// Check if we have a read event waiting for us
				if (socket.UpdateIOStatus(1ms))
				{
					if (socket.GetIOStatus().CanRead())
					{
						pendpoint = IPEndpoint();
						buffer.Clear();

						const auto result = socket.ReceiveFrom(pendpoint, buffer);
						if (result.Succeeded() && *result > 0)
						{
							accept = true;
						}
					}
					else if (socket.GetIOStatus().HasException())
					{
						LogErr(L"Exception on listener socket for endpoint %s (%s)",
							   socket.GetLocalEndpoint().GetString().c_str(),
							   GetSysErrorString(socket.GetIOStatus().GetErrorCode()).c_str());
					}
				}
				else
				{
					LogErr(L"Could not get status of listener socket for endpoint %s",
						   socket.GetLocalEndpoint().GetString().c_str());
				}
			});

			if (accept)
			{
				AcceptConnection(thdata.Socket, lendpoint, pendpoint, buffer);
			}
		}
	}

	void Manager::AcceptConnection(const std::shared_ptr<Socket_ThS>& socket, const IPEndpoint& lendpoint,
								   const IPEndpoint& pendpoint, const Buffer& buffer) noexcept
	{
		if (CanAcceptConnection(pendpoint.GetIPAddress()))
		{
			auto reputation_update = false;

			Message msg(Message::Type::Unknown, Message::Direction::Incoming);
			if (msg.Read(buffer) && msg.IsValid() && msg.GetType() == Message::Type::Syn)
			{
				const auto& syn_data = msg.GetSynData();

				if (syn_data.ProtocolVersionMajor == UDP::ProtocolVersion::Major &&
					syn_data.ProtocolVersionMinor == UDP::ProtocolVersion::Minor)
				{
					if (!m_UDPConnectionManager.HasConnection(syn_data.ConnectionID, PeerConnectionType::Inbound))
					{
						auto peerths = m_PeerManager.CreateUDP(pendpoint.GetIPAddress().GetFamily(), PeerConnectionType::Inbound,
															   syn_data.ConnectionID, msg.GetMessageSequenceNumber(), std::nullopt);
						if (peerths != nullptr)
						{
							peerths->WithUniqueLock([&](Peer::Peer& peer)
							{
								if (peer.GetSocket<Socket>().Accept(socket, lendpoint, pendpoint))
								{
									if (m_PeerManager.Accept(peerths))
									{
										LogInfo(L"Connection accepted from peer %s", peer.GetPeerName().c_str());
									}
									else
									{
										peer.Close();
										LogErr(L"Could not accept connection from peer %s", peer.GetPeerName().c_str());
									}
								}
							});
						}
					}
					else
					{
						LogWarn(L"UDP listenermanager cannot accept incoming connection with ID %llu from peer %s; connection already exists",
								syn_data.ConnectionID, pendpoint.GetString().c_str());
					}
				}
				else
				{
					LogErr(L"Could not accept connection from peer %s; unsupported UDP protocol version", pendpoint.GetString().c_str());
					reputation_update = true;
				}
			}
			else
			{
				LogErr(L"Peer %s sent invalid message for establishing UDP connection", pendpoint.GetString().c_str());
				reputation_update = true;
			}

			if (reputation_update)
			{
				const auto result = m_AccessManager.UpdateIPReputation(pendpoint.GetIPAddress(),
																	   Access::IPReputationUpdate::DeteriorateMinimal);
				if (!result.Succeeded())
				{
					LogWarn(L"UDP listener manager couldn't update IP reputation for peer %s (%s)",
							pendpoint.GetString().c_str(), result.GetErrorString().c_str());
				}
			}
		}
		else
		{
			LogWarn(L"Discarding incoming data from peer %s; IP address is not allowed by access configuration",
					pendpoint.GetString().c_str());
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
}