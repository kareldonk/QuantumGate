// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BTHListenerManager.h"

using namespace std::literals;
using namespace QuantumGate::Implementation::Network;

namespace QuantumGate::Implementation::Core::BTH::Listener
{
	Manager::Manager(const Settings_CThS& settings, Access::Manager& accessmgr, Peer::Manager& peers) noexcept :
		m_Settings(settings), m_AccessManager(accessmgr), m_PeerManager(peers)
	{}

	// Starts listening on the default interfaces
	bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"BTH listenermanager starting up...");

		PreStartup();

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.Listeners.BTH.Ports;
		const auto require_auth = settings.Local.Listeners.BTH.RequireAuthentication;
		const auto discoverable = settings.Local.Listeners.BTH.Discoverable;
		const auto& service_details = settings.Local.Listeners.BTH.Service;

		// Should have at least one port
		if (listener_ports.empty())
		{
			LogErr(L"BTH listenermanager startup failed; no ports given");
			return false;
		}

		DiscardReturnValue(AddListenerThreads(BTHAddress::AnyBTH(), listener_ports, require_auth, service_details));

		if (m_ThreadPool.Startup())
		{
			if (discoverable) EnableDiscovery();

			m_Running = true;
			m_ListeningOnAnyAddresses = true;

			LogSys(L"BTH listenermanager startup successful");
		}
		else LogErr(L"BTH listenermanager startup failed");

		return m_Running;
	}

	// Starts listening on all active interfaces
	bool Manager::Startup(const Vector<API::Local::Environment::BluetoothRadio>& radios) noexcept
	{
		if (m_Running) return true;

		LogSys(L"BTH listenermanager starting...");

		PreStartup();

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.Listeners.BTH.Ports;
		const auto require_auth = settings.Local.Listeners.BTH.RequireAuthentication;
		const auto discoverable = settings.Local.Listeners.BTH.Discoverable;
		const auto& service_details = settings.Local.Listeners.BTH.Service;

		// Should have at least one port
		if (listener_ports.empty())
		{
			LogErr(L"BTH listenermanager startup failed; no ports given");
			return false;
		}

		// Create a listening socket for each radio that's online
		for (const auto& radio : radios)
		{
			DiscardReturnValue(AddListenerThreads(radio.Address, listener_ports, require_auth, service_details));
		}

		if (m_ThreadPool.Startup())
		{
			if (discoverable) EnableDiscovery();

			m_Running = true;
			m_ListeningOnAnyAddresses = false;

			LogSys(L"BTH listenermanager startup successful");
		}
		else LogErr(L"BTH listenermanager startup failed");

		return m_Running;
	}

	bool Manager::AddListenerThreads(const BTHAddress& address, const Vector<UInt16> ports, const bool require_auth,
									 const BluetoothServiceDetails& service_details) noexcept
	{
		// Separate listener for every port
		for (const auto port : ports)
		{
			try
			{
				const auto endpoint = BTHEndpoint(BTHEndpoint::Protocol::RFCOMM, address, port);

				// Create and start the listenersocket
				ThreadData ltd;
				ltd.Socket = Network::Socket(endpoint.GetBTHAddress().GetFamily(), Network::Socket::Type::Stream,
											 Network::BTH::Protocol::RFCOMM);

				if (require_auth)
				{
					if (!ltd.Socket.SetBluetoothAuthentication(true))
					{
						continue;
					}
				}

				if (ltd.Socket.Listen(endpoint))
				{
					if (ltd.Socket.SetService(service_details.Name.c_str(), service_details.Comment.c_str(),
											  service_details.ID, Socket::ServiceOperation::Register))
					{
						if (m_ThreadPool.AddThread(L"QuantumGate Listener Thread " + endpoint.GetString(),
												   std::move(ltd), MakeCallback(this, &Manager::WorkerThreadProcessor)))
						{
							const auto thread = m_ThreadPool.GetLastThread();

							LogSys(L"Listening on endpoint %s, Service Class ID %s",
								   thread->GetData().Socket.GetLocalEndpoint().GetString().c_str(),
								   Util::ToString(service_details.ID).c_str());
						}
						else
						{
							LogErr(L"Could not add listener thread for endpoint %s", endpoint.GetString().c_str());
						}
					}
					else
					{
						LogErr(L"Could not register Bluetooth service for endpoint %s", endpoint.GetString().c_str());
					}
				}
			}
			catch (const std::exception& e)
			{
				LogErr(L"Could not add listener thread for Bluetooth address %s due to exception: %s",
					   address.GetString().c_str(), Util::ToStringW(e.what()).c_str());
			}
			catch (...) {}
		}

		return true;
	}

	std::optional<Manager::ThreadPool::ThreadType> Manager::RemoveListenerThread(Manager::ThreadPool::ThreadType&& thread,
																				 const BluetoothServiceDetails& service_details) noexcept
	{
		const Endpoint endpoint = thread.GetData().Socket.GetLocalEndpoint();

		if (!thread.GetData().Socket.SetService(service_details.Name.c_str(), service_details.Comment.c_str(),
												service_details.ID, Socket::ServiceOperation::Delete))
		{
			LogErr(L"Could not delete Bluetooth service for endpoint %s", endpoint.GetString().c_str());
		}

		const auto& [success, next_thread] = m_ThreadPool.RemoveThread(std::move(thread));
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

	bool Manager::Update(const Vector<API::Local::Environment::BluetoothRadio>& radios) noexcept
	{
		if (!m_Running) return false;

		// No need to update in this case
		if (m_ListeningOnAnyAddresses) return true;

		LogSys(L"Updating BTH listenermanager...");

		const auto& settings = m_Settings.GetCache();
		const auto& listener_ports = settings.Local.Listeners.BTH.Ports;
		const auto require_auth = settings.Local.Listeners.BTH.RequireAuthentication;
		const auto& service_details = settings.Local.Listeners.BTH.Service;

		// Check for radio/BTH addresses that were added for which
		// there are no listeners; we add listeners for those
		for (const auto& radio : radios)
		{
			if (radio.Address.GetFamily() == BTHAddress::Family::BTH)
			{
				auto found = false;

				auto thread = m_ThreadPool.GetFirstThread();

				while (thread.has_value())
				{
					if (thread->GetData().Socket.GetLocalEndpoint().GetBTHEndpoint().GetBTHAddress() == radio.Address)
					{
						found = true;
						break;
					}
					else thread = m_ThreadPool.GetNextThread(*thread);
				}

				if (!found)
				{
					DiscardReturnValue(AddListenerThreads(radio.Address, listener_ports, require_auth, service_details));
				}
			}
		}

		// Check for radio/BTH addresses that were removed for which
		// there are still listeners; we remove listeners for those
		auto thread = m_ThreadPool.GetFirstThread();

		while (thread.has_value())
		{
			auto found = false;

			for (const auto& radio : radios)
			{
				if (thread->GetData().Socket.GetLocalEndpoint().GetBTHEndpoint().GetBTHAddress() == radio.Address)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				thread = RemoveListenerThread(std::move(*thread), service_details);
			}
			else thread = m_ThreadPool.GetNextThread(*thread);
		}

		return true;
	}

	void Manager::Shutdown() noexcept
	{
		if (!m_Running) return;

		m_Running = false;

		LogSys(L"BTH listenermanager shutting down...");

		const auto& settings = m_Settings.GetCache();
		const auto& service_details = settings.Local.Listeners.BTH.Service;

		DisableDiscovery();

		m_ThreadPool.Shutdown();

		// Remove all threads
		auto thread = m_ThreadPool.GetFirstThread();
		while (thread.has_value())
		{
			thread = RemoveListenerThread(std::move(*thread), service_details);
		}

		ResetState();

		LogSys(L"BTH listenermanager shut down");
	}

	void Manager::PreStartup() noexcept
	{
		ResetState();
	}

	void Manager::ResetState() noexcept
	{
		m_Discoverable = false;
		m_ListeningOnAnyAddresses = false;
		m_ThreadPool.Clear();
	}

	void Manager::EnableDiscovery() noexcept
	{
		if (BluetoothEnableDiscovery(nullptr, true))
		{
			m_Discoverable = true;

			LogSys(L"Bluetooth discovery enabled");
		}
		else LogErr(L"Could not enable Bluetooth discovery; BluetoothEnableDiscovery() failed (%s)",
					GetLastSysErrorString().c_str());
	}

	void Manager::DisableDiscovery() noexcept
	{
		if (m_Discoverable)
		{
			if (BluetoothEnableDiscovery(nullptr, false))
			{
				m_Discoverable = false;

				LogSys(L"Bluetooth discovery disabled");
			}
			else LogErr(L"Could not disable Bluetooth discovery; BluetoothEnableDiscovery() failed (%s)",
						GetLastSysErrorString().c_str());
		}
	}

	void Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, ThreadData& thdata, const Concurrency::Event& shutdown_event)
	{
		while (!shutdown_event.IsSet())
		{
			// Check if we have a read event waiting for us
			if (thdata.Socket.UpdateIOStatus(1ms))
			{
				if (thdata.Socket.GetIOStatus().CanRead())
				{
					// Probably have a connection waiting to accept
					LogInfo(L"Accepting new connection on endpoint %s",
							thdata.Socket.GetLocalEndpoint().GetString().c_str());

					AcceptConnection(thdata.Socket);
				}
				else if (thdata.Socket.GetIOStatus().HasException())
				{
					LogErr(L"Exception on listener socket for endpoint %s (%s); will exit thread",
						   thdata.Socket.GetLocalEndpoint().GetString().c_str(),
						   GetSysErrorString(thdata.Socket.GetIOStatus().GetErrorCode()).c_str());
					break;
				}
			}
			else
			{
				LogErr(L"Could not get status of listener socket for endpoint %s; will exit thread",
					   thdata.Socket.GetLocalEndpoint().GetString().c_str());
				break;
			}
		}
	}

	void Manager::AcceptConnection(Network::Socket& listener_socket) noexcept
	{
		auto peerths = m_PeerManager.CreateBTH(listener_socket.GetAddressFamily(), PeerConnectionType::Inbound, std::nullopt);
		if (peerths != nullptr)
		{
			peerths->WithUniqueLock([&](Peer::Peer& peer) noexcept
			{
				if (listener_socket.Accept(peer.GetSocket<BTH::Socket>(), false, nullptr, nullptr))
				{
					// Check if the Bluetooth address is allowed
					if (!CanAcceptConnection(peer.GetPeerEndpoint()))
					{
						peer.Close();
						LogWarn(L"Incoming connection from peer %s was rejected; Bluetooth address is not allowed by access configuration",
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

	bool Manager::CanAcceptConnection(const Address& addr) const noexcept
	{
		// Increase connection attempts for this address; if attempts get too high
		// for a given interval the address will get a bad reputation and this will fail
		if (m_AccessManager.AddConnectionAttempt(addr))
		{
			// Check if address has acceptable reputation
			if (const auto result = m_AccessManager.GetConnectionFromAddressAllowed(addr, Access::CheckType::AddressReputations); result.Succeeded())
			{
				return *result;
			}
		}

		// If anything goes wrong we always deny access
		return false;
	}
}