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
		const auto& shared_secret = settings.Local.GlobalSharedSecret;
		const auto cookie_expiration_interval = settings.UDP.CookieExpirationInterval;

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
				DiscardReturnValue(AddWorkerListenerThreads(*address, listener_ports, nat_traversal, shared_secret));
			}
		}

		auto& connection_cookies = m_ThreadPool.GetData().ConnectionCookies;

		if (connection_cookies.WithUniqueLock()->Initialize(Util::GetCurrentSteadyTime(), cookie_expiration_interval) &&
			m_ThreadPool.Startup())
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
		const auto& shared_secret = settings.Local.GlobalSharedSecret;
		const auto cookie_expiration_interval = settings.UDP.CookieExpirationInterval;

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
						DiscardReturnValue(AddWorkerListenerThreads(address, listener_ports, nat_traversal, shared_secret));
					}
					else assert(false);
				}
			}
		}

		auto& connection_cookies = m_ThreadPool.GetData().ConnectionCookies;

		if (connection_cookies.WithUniqueLock()->Initialize(Util::GetCurrentSteadyTime(), cookie_expiration_interval) &&
			m_ThreadPool.Startup())
		{
			m_Running = true;
			m_ListeningOnAnyAddresses = false;

			LogSys(L"UDP listenermanager startup successful");
		}
		else LogErr(L"UDP listenermanager startup failed");

		return m_Running;
	}

	bool Manager::AddWorkerListenerThreads(const IPAddress& address, const Vector<UInt16> ports, const bool nat_traversal,
										   const ProtectedBuffer& shared_secret) noexcept
	{
		// Separate listener for every port
		for (const auto port : ports)
		{
			try
			{
				const auto endpoint = IPEndpoint(IPEndpoint::Protocol::UDP, address, port);

				// Create and start the listenersocket
				ThreadData ltd(shared_secret);
				ltd.Socket = Socket(endpoint.GetIPAddress().GetFamily(),
									Network::Socket::Type::Datagram, Network::IP::Protocol::UDP);
				ltd.SendQueue = std::make_shared<SendQueue_ThS>();

				if (ltd.Socket.Bind(endpoint, nat_traversal))
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
		const IPEndpoint endpoint = thread.GetData().Socket.GetLocalEndpoint();

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
							if (thread->GetData().Socket.GetLocalIPAddress() == address)
							{
								found = true;
								break;
							}
							else thread = m_ThreadPool.GetNextThread(*thread);
						}

						if (!found)
						{
							DiscardReturnValue(AddWorkerListenerThreads(address, listener_ports, nat_traversal,
																		settings.Local.GlobalSharedSecret));
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
						if (thread->GetData().Socket.GetLocalIPAddress() == address)
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
		m_ThreadPool.GetData().ConnectionCookies.WithUniqueLock()->Deinitialize();
		m_ThreadPool.Clear();
	}

	Manager::ReceiveBuffer& Manager::GetReceiveBuffer() const noexcept
	{
		static thread_local ReceiveBuffer rcvbuf{ ReceiveBuffer::GetMaxSize() };
		return rcvbuf;
	}

	void Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		auto& socket = thdata.Socket;
		const IPEndpoint lendpoint = socket.GetLocalEndpoint();
		IPEndpoint pendpoint;
		auto& buffer = GetReceiveBuffer();

		while (!shutdown_event.IsSet())
		{
			if (socket.UpdateIOStatus(1ms))
			{
				if (socket.GetIOStatus().HasException())
				{
					LogErr(L"Exception on listener socket for endpoint %s (%s); will exit thread",
						   socket.GetLocalEndpoint().GetString().c_str(),
						   GetSysErrorString(socket.GetIOStatus().GetErrorCode()).c_str());
					break;
				}
				else
				{
					if (socket.GetIOStatus().CanRead())
					{
						auto& settings = m_Settings.GetCache();

						pendpoint = IPEndpoint();
						auto bufspan = BufferSpan(buffer);

						const auto result = socket.ReceiveFrom(pendpoint, bufspan);
						if (result.Succeeded() && *result > 0)
						{
							// Check if IP is allowed through filters/limits and if it has acceptable reputation
							if (const auto result1 = m_AccessManager.GetIPConnectionAllowed(pendpoint.GetIPAddress(),
																							Access::CheckType::All); result1.Succeeded())
							{
								bufspan = bufspan.GetFirst(*result);

								[[maybe_unused]] const auto& [success, rep_update] =
									AcceptConnection(settings, Util::GetCurrentSteadyTime(), Util::GetCurrentSystemTime(),
													 thdata.SendQueue, lendpoint, pendpoint, bufspan, thdata.SymmetricKeys);
								if (rep_update != Access::IPReputationUpdate::None)
								{
									const auto result2 = m_AccessManager.UpdateIPReputation(pendpoint.GetIPAddress(), rep_update);
									if (!result2.Succeeded())
									{
										LogWarn(L"UDP listenermanager couldn't update IP reputation for peer %s (%s)",
												pendpoint.GetString().c_str(), result2.GetErrorString().c_str());
									}
								}
							}
							else
							{
								LogWarn(L"UDP listenermanager discarding incoming data from peer %s; IP address is not allowed by access configuration",
										pendpoint.GetString().c_str());
							}
						}
					}

					if (socket.GetIOStatus().CanWrite())
					{
						auto send_queue = thdata.SendQueue->WithUniqueLock();
						while (!send_queue->empty())
						{
							auto remove = false;
							const auto& item = send_queue->front();

							const auto result = socket.SendTo(item.Endpoint, item.Data);
							if (result.Succeeded())
							{
								// If data was actually sent, otherwise buffer may
								// temporarily be full/unavailable
								if (*result == item.Data.GetSize())
								{
									remove = true;
								}
								else
								{
									// We'll try again later
									break;
								}
							}
							else
							{
								LogErr(L"UDP listenermanager failed to send data to peer %s (%s)",
									   item.Endpoint.GetString().c_str(), result.GetErrorString().c_str());

								// Remove from queue (UDPConnection will retry and add back if needed)
								remove = true;
							}

							if (remove) send_queue->pop();
						}
					}
				}
			}
			else
			{
				LogErr(L"Could not get status of listener socket for endpoint %s; will exit thread",
					   socket.GetLocalEndpoint().GetString().c_str());
				break;
			}
		}
	}

	std::pair<bool, Access::IPReputationUpdate> Manager::AcceptConnection(const Settings& settings,
																		  const SteadyTime current_steadytime,
																		  const SystemTime current_systemtime,
																		  const std::shared_ptr<SendQueue_ThS>& send_queue,
																		  const IPEndpoint& lendpoint, const IPEndpoint& pendpoint,
																		  BufferSpan& buffer, const SymmetricKeys& symkeys) noexcept
	{
		Message msg(Message::Type::Unknown, Message::Direction::Incoming);
		if (msg.Read(buffer, symkeys) && msg.IsValid())
		{
			switch (msg.GetType())
			{
				case Message::Type::Syn:
				{
					auto& syn_data = msg.GetSynData();

					if (!(syn_data.ProtocolVersionMajor == UDP::ProtocolVersion::Major &&
						  syn_data.ProtocolVersionMinor == UDP::ProtocolVersion::Minor))
					{
						LogErr(L"UDP listenermanager could not accept connection from peer %s; unsupported UDP protocol version",
							   pendpoint.GetString().c_str());

						return { false, Access::IPReputationUpdate::DeteriorateMinimal };
					}

					const auto msgtime = Util::ToTime(syn_data.Time);
					if (std::chrono::abs(current_systemtime - msgtime) > settings.Message.AgeTolerance)
					{
						// Message should not be too old or too far into the future
						LogErr(L"UDP listenermanager refused connection from peer %s; message outside time tolerance (%jd seconds)",
							   pendpoint.GetString().c_str(), settings.Message.AgeTolerance.count());

						return { false, Access::IPReputationUpdate::DeteriorateModerate };
					}

					auto cookie_verified{ false };

					if (syn_data.Cookie.has_value())
					{
						auto& connection_cookies = m_ThreadPool.GetData().ConnectionCookies;
						if (connection_cookies.WithUniqueLock()->VerifyCookie(*syn_data.Cookie, syn_data.ConnectionID,
																			  pendpoint, Util::GetCurrentSteadyTime(),
																			  settings.UDP.CookieExpirationInterval))
						{
							LogDbg(L"UDP listenermanager verified cookie from peer %s for incoming connection with ID %llu",
								   pendpoint.GetString().c_str(), syn_data.ConnectionID);

							cookie_verified = true;
						}
						else
						{
							DbgInvoke([&]() noexcept
							{
								LogErr(L"UDP listenermanager failed to verify cookie from peer %s for incoming connection with ID %llu",
									   pendpoint.GetString().c_str(), syn_data.ConnectionID);
							});

							LogWarn(L"UDP listenermanager cannot accept incoming connection with ID %llu from peer %s; invalid cookie",
									syn_data.ConnectionID, pendpoint.GetString().c_str());

							return { false, Access::IPReputationUpdate::DeteriorateModerate };
						}
					}

					auto create_connection{ false };

					const auto retval = m_UDPConnectionManager.QueryAddConnection(syn_data.ConnectionID, pendpoint,
																				  PeerConnectionType::Inbound);
					switch (retval)
					{
						case UDP::Connection::Manager::AddQueryCode::OK:
						{
							create_connection = true;
							break;
						}
						case UDP::Connection::Manager::AddQueryCode::RequireSynCookie:
						{
							LogDbg(L"UDP listenermanager requires cookie for incoming connection with ID %llu from peer %s",
								   syn_data.ConnectionID, pendpoint.GetString().c_str());

							if (cookie_verified) create_connection = true;
							else
							{
								SendCookie(settings, current_steadytime, send_queue, pendpoint, syn_data.ConnectionID, symkeys);
							}
							break;
						}
						case UDP::Connection::Manager::AddQueryCode::ConnectionAlreadyExists:
						{
							LogDbg(L"UDP listenermanager cannot accept incoming connection with ID %llu from peer %s; connection already exists",
								   syn_data.ConnectionID, pendpoint.GetString().c_str());
							break;
						}
						case UDP::Connection::Manager::AddQueryCode::ConnectionIDInUse:
						{
							LogWarn(L"UDP listenermanager cannot accept incoming connection with ID %llu from peer %s; connection ID is in use by another peer",
									syn_data.ConnectionID, pendpoint.GetString().c_str());

							return { false, Access::IPReputationUpdate::DeteriorateModerate };
						}
						default:
						{
							// Shouldn't get here
							assert(false);
							break;
						}
					}

					if (create_connection)
					{
						if (CanAcceptConnection(pendpoint.GetIPAddress()))
						{
							auto peerths = m_PeerManager.CreateUDP(pendpoint.GetIPAddress().GetFamily(), PeerConnectionType::Inbound,
																   syn_data.ConnectionID, msg.GetMessageSequenceNumber(),
																   std::move(*syn_data.HandshakeDataIn), std::nullopt);
							if (peerths != nullptr)
							{
								auto peer = peerths->WithUniqueLock();
								if (peer->GetSocket<UDP::Socket>().Accept(send_queue, lendpoint, pendpoint))
								{
									if (m_PeerManager.Accept(peerths))
									{
										LogInfo(L"Connection accepted from peer %s", peer->GetPeerName().c_str());

										return { true, Access::IPReputationUpdate::None };
									}
									else
									{
										peer->Close();
										LogErr(L"Could not accept connection from peer %s", peer->GetPeerName().c_str());
									}
								}
							}
						}
						else LogWarn(L"UDP listenermanager refused connection from peer %s; IP address is not allowed by access configuration",
									 pendpoint.GetString().c_str());
					}
					break;
				}
				case Message::Type::Null:
				{
					// Ignored
					return { true, Access::IPReputationUpdate::None };
				}
				default:
				{
					LogErr(L"Peer %s sent invalid messagetype for establishing UDP connection", pendpoint.GetString().c_str());

					return { false, Access::IPReputationUpdate::DeteriorateModerate };
				}
			}
		}
		else
		{
			// Unrecognized message; this is a fatal problem and may be an attack
			LogErr(L"Peer %s sent an unrecognized message for establishing UDP connection", pendpoint.GetString().c_str());

			return { false, Access::IPReputationUpdate::DeteriorateSevere };
		}

		return { false, Access::IPReputationUpdate::None };
	}

	void Manager::SendCookie(const Settings& settings, const SteadyTime current_steadytime,
							 const std::shared_ptr<SendQueue_ThS>& send_queue, const IPEndpoint& pendpoint,
							 const ConnectionID connectionid, const SymmetricKeys& symkeys) noexcept
	{
		LogDbg(L"UDP listenermanager sending cookie to peer %s for incoming connection with ID %llu",
			   pendpoint.GetString().c_str(), connectionid);

		auto& connection_cookies = m_ThreadPool.GetData().ConnectionCookies;
		auto cookie_data = connection_cookies.WithUniqueLock()->GetCookie(connectionid, pendpoint,
																		  Util::GetCurrentSteadyTime(),
																		  settings.UDP.CookieExpirationInterval);
		if (cookie_data.has_value())
		{
			try
			{
				Message msg(Message::Type::Cookie, Message::Direction::Outgoing, Connection::UDPMessageSizes::Min);
				msg.SetCookieData(std::move(*cookie_data));

				Buffer data;
				if (msg.Write(data, symkeys))
				{
					send_queue->WithUniqueLock()->emplace(
						SendQueueItem{
							.Endpoint = pendpoint,
							.Data = std::move(data)
						});
				}
			}
			catch (const std::exception& e)
			{
				LogErr(L"UDP listenermanager failed to send a cookie to peer %s due to an exception - %s",
					   pendpoint.GetString().c_str(), Util::ToStringW(e.what()).c_str());
			}
		}
		else
		{
			LogErr(L"UDP listenermanager failed to send a cookie to peer %s; a cookie could not be created",
				   pendpoint.GetString().c_str());
		}
	}

	bool Manager::CanAcceptConnection(const IPAddress& ipaddr) const noexcept
	{
		// Increase connection attempts for this IP; if attempts get too high
		// for a given interval the IP will get a bad reputation and this will fail
		if (m_AccessManager.AddIPConnectionAttempt(ipaddr))
		{
			// Check if IP is allowed through filters/limits and if it has acceptable reputation
			if (const auto result = m_AccessManager.GetIPConnectionAllowed(ipaddr, Access::CheckType::All); result.Succeeded())
			{
				return *result;
			}
		}

		// If anything goes wrong we always deny access
		return false;
	}
}