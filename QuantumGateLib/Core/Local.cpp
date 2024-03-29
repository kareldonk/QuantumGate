// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Local.h"
#include "..\Version.h"
#include "..\Common\ScopeGuard.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core
{
	Local::Local()
	{
		// Initialize Winsock
		WSADATA wsaData{ 0 };
		const auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (result != 0)
		{
			LogErr(L"Couldn't initialize Windows Sockets; WSAStartup() failed");
			throw std::exception("Couldn't initialize Windows Sockets; WSAStartup() failed");
		}

		// Upon failure shut down winsock
		auto sg = MakeScopeGuard([]() noexcept { WSACleanup(); });

		// Initialize security settings
		SetSecurityLevel(SecurityLevel::One, std::nullopt, true).Failed([]()
		{
			LogErr(L"Couldn't set QuantumGate security level");
			throw std::exception("Couldn't set QuantumGate security level");
		});

		sg.Deactivate();
	}

	Local::~Local()
	{
		if (IsRunning())
		{
			if (!Shutdown())
			{
				LogErr(L"Couldn't shut down QuantumGate");
			}
		}

		// May have been initialized before Startup() or after Shutdown()
		m_LocalEnvironment.WithUniqueLock([](auto& local_env) noexcept
		{
			if (local_env.IsInitialized()) local_env.Deinitialize();
		});

		// Deinit Winsock
		WSACleanup();
	}

	bool Local::ValidateInitParameters(const StartupParameters& params) const noexcept
	{
		if (params.UUID.GetType() != UUID::Type::Peer)
		{
			LogErr(L"Invalid UUID specified in the initialization parameters");
			return false;
		}
		else
		{
			if (params.Keys.has_value() && !params.Keys->PublicKey.IsEmpty())
			{
				if (!params.UUID.Verify(params.Keys->PublicKey))
				{
					LogErr(L"The UUID and public key specified in the initialization parameters don't match");
					return false;
				}
			}
		}

		if (params.GlobalSharedSecret.has_value())
		{
			if (params.GlobalSharedSecret->GetSize() < 64 || !Crypto::ValidateBuffer(*params.GlobalSharedSecret))
			{
				LogErr(L"The Global Shared Secret specified in the initialization parameters isn't valid");
				return false;
			}
		}

		if (!ValidateSupportedAlgorithms(params.SupportedAlgorithms))
		{
			return false;
		}

		if (params.RequireAuthentication && !(params.Keys.has_value() && !params.Keys->PrivateKey.IsEmpty()))
		{
			LogErr(L"No private key is specified in the initialization parameters while authentication is required");
			return false;
		}

		if (params.Relays.IPv4ExcludedNetworksCIDRLeadingBits > 32 ||
			params.Relays.IPv6ExcludedNetworksCIDRLeadingBits > 128)
		{
			LogErr(L"Invalid excluded network CIDR leading bits specified in relay parameters");
			return false;
		}

		return true;
	}

	bool Local::ValidateSupportedAlgorithms(const Algorithms& algorithms) const noexcept
	{
		try
		{
			if (algorithms.Hash.empty())
			{
				LogErr(L"No hashing algorithm specified in the initialization parameters");
				return false;
			}
			else
			{
				for (const auto ha : algorithms.Hash)
				{
					if (!Crypto::HasAlgorithm({
						Algorithm::Hash::SHA256,
						Algorithm::Hash::SHA512,
						Algorithm::Hash::BLAKE2S256,
						Algorithm::Hash::BLAKE2B512 }, ha))
					{
						LogErr(L"Unsupported hash algorithm specified in the initialization parameters");
						return false;
					}
				}
			}

			if (algorithms.PrimaryAsymmetric.empty())
			{
				LogErr(L"No primary asymmetric algorithm specified in the initialization parameters");
				return false;
			}
			else
			{
				for (const auto paa : algorithms.PrimaryAsymmetric)
				{
					if (!Crypto::HasAlgorithm({
						Algorithm::Asymmetric::ECDH_SECP521R1,
						Algorithm::Asymmetric::ECDH_X25519,
						Algorithm::Asymmetric::ECDH_X448,
						Algorithm::Asymmetric::KEM_NTRUPRIME,
						Algorithm::Asymmetric::KEM_NEWHOPE,
						Algorithm::Asymmetric::KEM_CLASSIC_MCELIECE }, paa))
					{
						LogErr(L"Unsupported primary asymmetric algorithm specified in the initialization parameters");
						return false;
					}
				}
			}

			if (algorithms.SecondaryAsymmetric.empty())
			{
				LogErr(L"No secondary asymmetric algorithm specified in the initialization parameters");
				return false;
			}
			else
			{
				for (const auto saa : algorithms.SecondaryAsymmetric)
				{
					if (!Crypto::HasAlgorithm({
						Algorithm::Asymmetric::ECDH_SECP521R1,
						Algorithm::Asymmetric::ECDH_X25519,
						Algorithm::Asymmetric::ECDH_X448,
						Algorithm::Asymmetric::KEM_NTRUPRIME,
						Algorithm::Asymmetric::KEM_NEWHOPE,
						Algorithm::Asymmetric::KEM_CLASSIC_MCELIECE }, saa))
					{
						LogErr(L"Unsupported secondary asymmetric algorithm specified in the initialization parameters");
						return false;
					}
				}
			}

			if (algorithms.Symmetric.empty())
			{
				LogErr(L"No symmetric algorithm specified in the initialization parameters");
				return false;
			}
			else
			{
				for (const auto sa : algorithms.Symmetric)
				{
					if (!Crypto::HasAlgorithm({
						Algorithm::Symmetric::AES256_GCM,
						Algorithm::Symmetric::CHACHA20_POLY1305 }, sa))
					{
						LogErr(L"Unsupported symmetric algorithm specified in the initialization parameters");
						return false;
					}
				}
			}

			if (algorithms.Compression.empty())
			{
				LogErr(L"No compression algorithm specified in the initialization parameters");
				return false;
			}
			else
			{
				for (const auto ca : algorithms.Compression)
				{
					if (!Crypto::HasAlgorithm({
						Algorithm::Compression::DEFLATE,
						Algorithm::Compression::ZSTANDARD }, ca))
					{
						LogErr(L"Unsupported compression algorithm specified in the initialization parameters");
						return false;
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Exception while validating supported algorithms specified in the initialization parameters - %s",
				   Util::ToStringW(e.what()).c_str());
			return false;
		}

		return true;
	}

	Result<> Local::Startup(const StartupParameters& params) noexcept
	{
		assert(!IsRunning());

		if (IsRunning()) return ResultCode::Succeeded;

		if (!ValidateInitParameters(params)) return ResultCode::InvalidArgument;

		std::unique_lock<std::shared_mutex> lock(m_Mutex);

		LogSys(L"QuantumGate starting...");
		LogSys(L"Version %s, protocol version %s", GetVersionString().c_str(), GetProtocolVersionString().c_str());

		m_ShutdownEvent.Reset();

		try
		{
			m_Settings.UpdateValue([&](Settings& settings)
			{
				settings.Local.UUID = params.UUID;

				if (params.Keys.has_value()) settings.Local.Keys = *params.Keys;
				else
				{
					settings.Local.Keys.PrivateKey.Clear();
					settings.Local.Keys.PublicKey.Clear();
				}

				if (params.GlobalSharedSecret.has_value())
				{
					settings.Local.GlobalSharedSecret = *params.GlobalSharedSecret;
				}
				else settings.Local.GlobalSharedSecret.Clear();

				settings.Local.RequireAuthentication = params.RequireAuthentication;

				{
					settings.Local.SupportedAlgorithms.PrimaryAsymmetric = Util::SetToVector(params.SupportedAlgorithms.PrimaryAsymmetric);
					settings.Local.SupportedAlgorithms.SecondaryAsymmetric = Util::SetToVector(params.SupportedAlgorithms.SecondaryAsymmetric);
					settings.Local.SupportedAlgorithms.Symmetric = Util::SetToVector(params.SupportedAlgorithms.Symmetric);
					settings.Local.SupportedAlgorithms.Hash = Util::SetToVector(params.SupportedAlgorithms.Hash);
					settings.Local.SupportedAlgorithms.Compression = Util::SetToVector(params.SupportedAlgorithms.Compression);
				}

				settings.Local.Listeners.TCP.Ports = Util::SetToVector(params.Listeners.TCP.Ports);
				settings.Local.Listeners.TCP.NATTraversal = params.Listeners.TCP.NATTraversal;
				settings.Local.Listeners.TCP.UseConditionalAcceptFunction = params.Listeners.TCP.UseConditionalAcceptFunction;
				
				settings.Local.Listeners.UDP.Ports = Util::SetToVector(params.Listeners.UDP.Ports);
				settings.Local.Listeners.UDP.NATTraversal = params.Listeners.UDP.NATTraversal;
				
				settings.Local.Listeners.BTH.Ports = Util::SetToVector(params.Listeners.BTH.Ports);
				settings.Local.Listeners.BTH.RequireAuthentication = params.Listeners.BTH.RequireAuthentication;
				settings.Local.Listeners.BTH.Discoverable = params.Listeners.BTH.Discoverable;
				
				if (params.Listeners.BTH.Service.has_value())
				{
					settings.Local.Listeners.BTH.Service = *params.Listeners.BTH.Service;
				}
				else
				{
					// Use defaults
					settings.Local.Listeners.BTH.Service.Name = BTH::Listener::Manager::DefaultServiceName;
					settings.Local.Listeners.BTH.Service.Comment = BTH::Listener::Manager::DefaultServiceComment;
					settings.Local.Listeners.BTH.Service.ID = BTHEndpoint::GetQuantumGateServiceClassID();
				}
				
				settings.Local.NumPreGeneratedKeysPerAlgorithm = params.NumPreGeneratedKeysPerAlgorithm;
				
				settings.Relay.IPv4ExcludedNetworksCIDRLeadingBits = params.Relays.IPv4ExcludedNetworksCIDRLeadingBits;
				settings.Relay.IPv6ExcludedNetworksCIDRLeadingBits = params.Relays.IPv6ExcludedNetworksCIDRLeadingBits;
			});
		}
		catch (const std::exception& e)
		{
			LogErr(L"Exception while updating settings with initialization parameters - %s",
				   Util::ToStringW(e.what()).c_str());
			return ResultCode::Failed;
		}

		if (!InitializeLocalEnvironment())
		{
			return ResultCode::Failed;
		}

		{
			const auto local_env = m_LocalEnvironment.WithSharedLock();
			LogSys(L"Localhost %s (%s)", local_env->GetHostname().c_str(), local_env->GetIPAddressesString().c_str());
			LogSys(L"Running as user %s", local_env->GetUsername().c_str());
		}

		LogSys(L"Local UUID %s", m_Settings.GetCache().Local.UUID.GetString().c_str());

		if (!m_Settings.GetCache().Local.RequireAuthentication)
		{
			LogWarn(L"QuantumGate is configured to not require peer authentication");
		}

		if (!StartupThreadPool())
		{
			return ResultCode::Failed;
		}

		// Upon failure shut down threadpool when we return
		auto sg0 = MakeScopeGuard([&]() noexcept { ShutdownThreadPool(); });

		if (params.NumPreGeneratedKeysPerAlgorithm > 0 &&
			!m_KeyGenerationManager.Startup())
		{
			return ResultCode::FailedKeyGenerationManagerStartup;
		}

		// Upon failure shut down key manager when we return
		auto sg1 = MakeScopeGuard([&]() noexcept { m_KeyGenerationManager.Shutdown(); });

		if (!m_UDPConnectionManager.Startup())
		{
			return ResultCode::FailedUDPConnectionManagerStartup;
		}

		// Upon failure shut down UDP connection manager when we return
		auto sg2 = MakeScopeGuard([&]() noexcept { m_UDPConnectionManager.Shutdown(); });

		if (!m_PeerManager.Startup())
		{
			return ResultCode::FailedPeerManagerStartup;
		}

		// Upon failure shut down peer manager when we return
		auto sg3 = MakeScopeGuard([&]() noexcept { m_PeerManager.Shutdown(); });

		if (params.Relays.Enable && !m_PeerManager.StartupRelays())
		{
			return ResultCode::FailedRelayManagerStartup;
		}

		// Upon failure shut down relay manager when we return
		auto sg4 = MakeScopeGuard([&]() noexcept { m_PeerManager.ShutdownRelays(); });

		if (params.Listeners.TCP.Enable &&
			!m_TCPListenerManager.Startup(m_LocalEnvironment.WithSharedLock()->GetEthernetInterfaces()))
		{
			return ResultCode::FailedTCPListenerManagerStartup;
		}

		// Upon failure shut down TCP listener manager when we return
		auto sg5 = MakeScopeGuard([&]() noexcept { m_TCPListenerManager.Shutdown(); });

		if (params.Listeners.UDP.Enable &&
			!m_UDPListenerManager.Startup(m_LocalEnvironment.WithSharedLock()->GetEthernetInterfaces()))
		{
			return ResultCode::FailedUDPListenerManagerStartup;
		}

		// Upon failure shut down UDP listener manager when we return
		auto sg6 = MakeScopeGuard([&]() noexcept { m_UDPListenerManager.Shutdown(); });

		if (params.Listeners.BTH.Enable &&
			!m_BTHListenerManager.Startup(m_LocalEnvironment.WithSharedLock()->GetBluetoothRadios()))
		{
			return ResultCode::FailedBluetoothListenerManagerStartup;
		}

		// Upon failure shut down BTH listener manager when we return
		auto sg7 = MakeScopeGuard([&]() noexcept { m_BTHListenerManager.Shutdown(); });

		// Enter running state; important for extenders
		m_Running = true;

		// Upon failure exit running state when we return
		auto sg8 = MakeScopeGuard([&]() noexcept { m_Running = false; });

		if (params.EnableExtenders && !m_ExtenderManager.Startup())
		{
			return ResultCode::FailedExtenderManagerStartup;
		}

		sg0.Deactivate();
		sg1.Deactivate();
		sg2.Deactivate();
		sg3.Deactivate();
		sg4.Deactivate();
		sg5.Deactivate();
		sg6.Deactivate();
		sg7.Deactivate();
		sg8.Deactivate();

		LogSys(L"QuantumGate startup successful");

		return ResultCode::Succeeded;
	}

	Result<> Local::Shutdown() noexcept
	{
		assert(IsRunning());

		if (!IsRunning()) return ResultCode::Succeeded;

		std::unique_lock<std::shared_mutex> lock(m_Mutex);

		LogSys(L"QuantumGate shutting down...");

		m_Running = false;

		m_ShutdownEvent.Set();

		// Stop accepting connections
		m_TCPListenerManager.Shutdown();
		m_UDPListenerManager.Shutdown();
		m_BTHListenerManager.Shutdown();

		// Shut down extenders
		m_ExtenderManager.Shutdown();

		// Close all connections
		m_PeerManager.ShutdownRelays();
		m_PeerManager.Shutdown();

		m_UDPConnectionManager.Shutdown();

		m_KeyGenerationManager.Shutdown();

		DeinitializeLocalEnvironment();

		ShutdownThreadPool();

		LogSys(L"QuantumGate shut down");

		return ResultCode::Succeeded;
	}

	bool Local::StartupThreadPool() noexcept
	{
		LogSys(L"Creating local threadpool with 1 worker thread");

		if (m_ThreadPool.AddThread(L"QuantumGate Local Thread",
								   MakeCallback(this, &Local::WorkerThreadProcessor),
								   MakeCallback(this, &Local::WorkerThreadWait),
								   MakeCallback(this, &Local::WorkerThreadWaitInterrupt)))
		{
			if (m_ThreadPool.Startup())
			{
				return true;
			}
		}

		LogErr(L"Couldn't start local threadpool");

		return false;
	}

	void Local::ShutdownThreadPool() noexcept
	{
		m_ThreadPool.Shutdown();
		m_ThreadPool.Clear();
		
		m_ThreadPool.GetData().EventQueue.Clear();
	}

	void Local::WorkerThreadWait(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event)
	{
		thpdata.EventQueue.Wait(shutdown_event);
	}

	void Local::WorkerThreadWaitInterrupt(ThreadPoolData& thpdata)
	{
		thpdata.EventQueue.InterruptWait();
	}

	void Local::WorkerThreadProcessor(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event)
	{
		std::optional<Event> event;

		thpdata.EventQueue.PopFrontIf([&](auto& fevent) noexcept -> bool
		{
			event = std::move(fevent);
			return true;
		});

		if (event.has_value())
		{
			if (std::holds_alternative<Events::LocalEnvironmentChange>(*event))
			{
				ProcessEvent(std::get<Events::LocalEnvironmentChange>(*event));
			}
			else if (std::holds_alternative<Events::UnhandledExtenderException>(*event))
			{
				ProcessEvent(std::get<Events::UnhandledExtenderException>(*event));
			}
			else assert(false);
		}
	}

	void Local::ProcessEvent(const Events::LocalEnvironmentChange& event) noexcept
	{
		if (m_LocalEnvironment.WithUniqueLock()->Update())
		{
			if (IsRunning())
			{
				if (m_TCPListenerManager.IsRunning() || m_UDPListenerManager.IsRunning() ||
					m_BTHListenerManager.IsRunning())
				{
					LogDbg(L"Updating listeners because of local environment change");

					if (UpdateListeners().Failed())
					{
						LogErr(L"Failed to update listeners after local environment change");
					}
				}

				if (m_UDPConnectionManager.IsRunning())
				{
					LogDbg(L"Updating UDP connection manager because of local environment change");

					m_UDPConnectionManager.OnLocalIPInterfaceChanged();
				}
			}
		}
		else
		{
			LogErr(L"Failed to update local environment information after change notification");
		}
	}

	void Local::ProcessEvent(const Events::UnhandledExtenderException& event) noexcept
	{
		if (IsRunning())
		{
			std::unique_lock<std::shared_mutex> lock(m_Mutex);

			const auto extender = GetExtender(event.UUID).lock();
			if (extender && extender->IsRunning())
			{
				LogWarn(L"Attempting to shut down extender with UUID %s due to unhandled exception",
						event.UUID.GetString().c_str());

				DiscardReturnValue(m_ExtenderManager.ShutdownExtender(event.UUID));
			}
		}
	}

	Result<> Local::EnableListeners(const API::Local::ListenerType type) noexcept
	{
		if (IsRunning())
		{
			std::unique_lock<std::shared_mutex> lock(m_Mutex);

			auto local_env = m_LocalEnvironment.WithSharedLock();

			auto result = ResultCode::Failed;

			switch (type)
			{
				case API::Local::ListenerType::TCP:
					if (m_TCPListenerManager.Startup(local_env->GetEthernetInterfaces()))
					{
						result = ResultCode::Succeeded;
					}
					break;
				case API::Local::ListenerType::UDP:
					if (m_UDPListenerManager.Startup(local_env->GetEthernetInterfaces()))
					{
						result = ResultCode::Succeeded;
					}
					break;
				case API::Local::ListenerType::BTH:
					if (m_BTHListenerManager.Startup(local_env->GetBluetoothRadios()))
					{
						result = ResultCode::Succeeded;
					}
					break;
				default:
					assert(false);
					result = ResultCode::InvalidArgument;
					break;
			}

			return result;
		}

		return ResultCode::NotRunning;
	}

	Result<> Local::DisableListeners(const API::Local::ListenerType type) noexcept
	{
		if (IsRunning())
		{
			std::unique_lock<std::shared_mutex> lock(m_Mutex);

			auto result = ResultCode::Succeeded;

			switch (type)
			{
				case API::Local::ListenerType::TCP:
					m_TCPListenerManager.Shutdown();
					break;
				case API::Local::ListenerType::UDP:
					m_UDPListenerManager.Shutdown();
					break;
				case API::Local::ListenerType::BTH:
					m_BTHListenerManager.Shutdown();
					break;
				default:
					assert(false);
					result = ResultCode::InvalidArgument;
					break;
			}

			return result;
		}

		return ResultCode::NotRunning;
	}

	bool Local::AreListenersEnabled(const API::Local::ListenerType type) const noexcept
	{
		switch (type)
		{
			case API::Local::ListenerType::TCP:
				return m_TCPListenerManager.IsRunning();
			case API::Local::ListenerType::UDP:
				return m_UDPListenerManager.IsRunning();
			case API::Local::ListenerType::BTH:
				return m_BTHListenerManager.IsRunning();
			default:
				assert(false);
				break;
		}

		return false;
	}

	Result<> Local::UpdateListeners() noexcept
	{
		if (IsRunning())
		{
			std::unique_lock<std::shared_mutex> lock(m_Mutex);

			auto local_env = m_LocalEnvironment.WithSharedLock();

			auto result = ResultCode::Succeeded;

			if (m_TCPListenerManager.IsRunning())
			{
				if (!m_TCPListenerManager.Update(local_env->GetEthernetInterfaces()))
				{
					result = ResultCode::Failed;
				}
			}

			if (m_UDPListenerManager.IsRunning())
			{
				if (!m_UDPListenerManager.Update(local_env->GetEthernetInterfaces()))
				{
					result = ResultCode::Failed;
				}
			}

			if (m_BTHListenerManager.IsRunning())
			{
				if (!m_BTHListenerManager.Update(local_env->GetBluetoothRadios()))
				{
					result = ResultCode::Failed;
				}
			}

			return result;
		}

		return ResultCode::NotRunning;
	}

	Result<> Local::EnableExtenders() noexcept
	{
		if (IsRunning())
		{
			std::unique_lock<std::shared_mutex> lock(m_Mutex);

			if (m_ExtenderManager.Startup())
			{
				return ResultCode::Succeeded;
			}
			else return ResultCode::Failed;
		}

		return ResultCode::NotRunning;
	}

	Result<> Local::DisableExtenders() noexcept
	{
		if (IsRunning())
		{
			std::unique_lock<std::shared_mutex> lock(m_Mutex);

			m_ExtenderManager.Shutdown();
			return ResultCode::Succeeded;
		}

		return ResultCode::NotRunning;
	}

	bool Local::AreExtendersEnabled() const noexcept
	{
		return m_ExtenderManager.IsRunning();
	}

	Result<> Local::EnableRelays() noexcept
	{
		if (IsRunning())
		{
			std::unique_lock<std::shared_mutex> lock(m_Mutex);

			if (m_PeerManager.StartupRelays())
			{
				return ResultCode::Succeeded;
			}
			else return ResultCode::Failed;
		}

		return ResultCode::NotRunning;
	}

	Result<> Local::DisableRelays() noexcept
	{
		if (IsRunning())
		{
			std::unique_lock<std::shared_mutex> lock(m_Mutex);

			m_PeerManager.ShutdownRelays();
			return ResultCode::Succeeded;
		}

		return ResultCode::NotRunning;
	}

	bool Local::AreRelaysEnabled() const noexcept
	{
		return m_PeerManager.AreRelaysRunning();
	}

	std::tuple<UInt, UInt, UInt, UInt> Local::GetVersion() const noexcept
	{
		return std::make_tuple(VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_BUILD);
	}

	String Local::GetVersionString() const noexcept
	{
		return Util::FormatString(L"%u.%u.%u build %u", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_BUILD);
	}

	std::pair<UInt, UInt> Local::GetProtocolVersion() const noexcept
	{
		return std::make_pair(ProtocolVersion::Major, ProtocolVersion::Minor);
	}

	String Local::GetProtocolVersionString() const noexcept
	{
		return Util::FormatString(L"%u.%u", ProtocolVersion::Major, ProtocolVersion::Minor);
	}

	bool Local::InitializeLocalEnvironment() noexcept
	{
		auto local_env = m_LocalEnvironment.WithUniqueLock();

		if (!local_env->IsInitialized())
		{
			if (!local_env->Initialize(MakeCallback(this, &Local::OnLocalEnvironmentChanged)))
			{
				LogErr(L"Couldn't initialize local environment");
				return false;
			}
		}
		else
		{
			if (!local_env->Update())
			{
				LogErr(L"Couldn't update local environment");
				return false;
			}
		}

		return true;
	}

	void Local::DeinitializeLocalEnvironment() noexcept
	{
		m_LocalEnvironment.WithUniqueLock()->Deinitialize();
	}

	const LocalEnvironment_ThS& Local::GetEnvironment(const bool refresh) noexcept
	{
		{
			auto env = m_LocalEnvironment.WithUniqueLock();
			if (!env->IsInitialized())
			{
				if (!env->Initialize(MakeCallback(this, &Local::OnLocalEnvironmentChanged)))
				{
					LogErr(L"Couldn't initialize local environment");
				}
			}
			else
			{
				if (!env->Update(refresh))
				{
					LogErr(L"Couldn't update local environment");
				}
			}
		}

		return m_LocalEnvironment;
	}

	void Local::OnLocalEnvironmentChanged() noexcept
	{
		if (IsRunning())
		{
			try
			{
				m_ThreadPool.GetData().EventQueue.Push(Events::LocalEnvironmentChange{});
			}
			catch (...) {}
		}
	}

	void Local::OnUnhandledExtenderException(const ExtenderUUID extuuid) noexcept
	{
		try
		{
			m_ThreadPool.GetData().EventQueue.Push(Events::UnhandledExtenderException{ extuuid });
		}
		catch (...) {}
	}

	Result<bool> Local::AddExtenderImpl(const std::shared_ptr<API::Extender>& extender,
										const Extender::ExtenderModuleID moduleid) noexcept
	{
		assert(extender);

		if (extender)
		{
			// Extender needs pointer to local
			extender->m_Extender->SetLocal(this);

			auto result = m_ExtenderManager.AddExtender(extender, moduleid);
			if (result.Failed())
			{
				// Reset pointer to local
				extender->m_Extender->ResetLocal();
			}

			return result;
		}

		return ResultCode::Failed;
	}

	Result<> Local::RemoveExtenderImpl(const std::shared_ptr<API::Extender>& extender,
									   const Extender::ExtenderModuleID moduleid) noexcept
	{
		assert(extender);

		if (extender)
		{
			auto result = m_ExtenderManager.RemoveExtender(extender, moduleid);
			if (result.Succeeded())
			{
				// Reset pointer to local
				extender->m_Extender->ResetLocal();
			}

			return result;
		}

		return ResultCode::Failed;
	}

	Result<bool> Local::AddExtender(const std::shared_ptr<API::Extender>& extender) noexcept
	{
		std::unique_lock<std::shared_mutex> lock(m_Mutex);

		return AddExtenderImpl(extender);
	}

	Result<> Local::RemoveExtender(const std::shared_ptr<API::Extender>& extender) noexcept
	{
		std::unique_lock<std::shared_mutex> lock(m_Mutex);

		return RemoveExtenderImpl(extender);
	}

	Result<> Local::AddExtenderModule(const Path& module_path) noexcept
	{
		std::unique_lock<std::shared_mutex> lock(m_Mutex);

		auto result_code = ResultCode::Failed;

		Extender::Module module(module_path);
		if (module.IsLoaded())
		{
			if (const auto it = m_ExtenderModules.find(module.GetID()); it == m_ExtenderModules.end())
			{
				const auto& extenders = module.GetExtenders();
				if (extenders.size() > 0)
				{
					LogSys(L"Adding extender(s) from module %s...", module_path.c_str());

					auto success = true;
					Extender::ExtendersVector added_extenders;

					try
					{
						added_extenders.reserve(extenders.size());

						for (auto& extender : extenders)
						{
							if (AddExtenderImpl(extender, module.GetID()).Succeeded())
							{
								added_extenders.emplace_back(extender);
							}
							else
							{
								success = false;
								break;
							}
						}

						if (success)
						{
							[[maybe_unused]] const auto [mit, inserted] =
								m_ExtenderModules.insert({ module.GetID(), std::move(module) });

							assert(inserted);

							if (inserted)
							{
								result_code = ResultCode::Succeeded;

								LogSys(L"Finished adding extender(s) from module %s", module_path.c_str());
							}
							else success = false;
						}
					}
					catch (...) {}

					if (!success)
					{
						LogErr(L"Failed to add extender(s) from module %s", module_path.c_str());

						// Remove all extenders that were successfully added
						for (auto& extender : added_extenders)
						{
							RemoveExtenderImpl(extender, module.GetID()).Failed([&](const auto& result) noexcept
							{
								LogErr(L"Failed to remove extender '%s' : %s",
									   extender->GetName().c_str(), result.GetErrorDescription().c_str());
							});
						}
					}
				}
			}
			else
			{
				result_code = ResultCode::ExtenderModuleAlreadyPresent;

				LogErr(L"Attempt to add extenders from module %s which is already loaded", module_path.c_str());
			}
		}
		else result_code = ResultCode::ExtenderModuleLoadFailure;

		return result_code;
	}

	Result<> Local::RemoveExtenderModule(const Path& module_path) noexcept
	{
		std::unique_lock<std::shared_mutex> lock(m_Mutex);

		Extender::ExtenderModuleID moduleid{ 0 };

		{
			// First get the ID of the module
			Extender::Module module(module_path);
			if (module.IsLoaded())
			{
				moduleid = module.GetID();
			}
			else return ResultCode::ExtenderModuleLoadFailure;
		}

		auto result_code = ResultCode::Failed;

		// If we have a module ID, check if we had it loaded
		// then remove all extenders that came from it
		if (const auto it = m_ExtenderModules.find(moduleid); it != m_ExtenderModules.end())
		{
			LogSys(L"Removing extender(s) from module %s...", module_path.c_str());

			// Following block is needed to release the extenders
			// before unloading (erasing) the module
			{
				auto success = true;
				const auto& extenders = it->second.GetExtenders();

				for (auto& extender : extenders)
				{
					RemoveExtenderImpl(extender, moduleid).Failed([&](const auto& result) noexcept
					{
						LogErr(L"Failed to remove extender '%s' : %s",
							   extender->GetName().c_str(), result.GetErrorDescription().c_str());
						success = false;
					});
				}

				if (success)
				{
					result_code = ResultCode::Succeeded;
				}
				else LogErr(L"Could not successfully remove all extender(s) from module %s", module_path.c_str());
			}

			if (result_code == ResultCode::Succeeded)
			{
				// The following will unload the extender module; we do this
				// after releasing all pointers to extenders above, otherwise
				// things explode
				m_ExtenderModules.erase(it);

				LogSys(L"Finished removing extender(s) from module %s", module_path.c_str());
			}
		}
		else
		{
			result_code = ResultCode::ExtenderModuleNotFound;

			LogErr(L"Attempt to remove extenders from module %s which is not loaded", module_path.c_str());
		}

		return result_code;
	}

	bool Local::HasExtender(const ExtenderUUID& extuuid) const noexcept
	{
		return m_ExtenderManager.HasExtender(extuuid);
	}

	std::weak_ptr<QuantumGate::API::Extender> Local::GetExtender(const ExtenderUUID& extuuid) const noexcept
	{
		return m_ExtenderManager.GetExtender(extuuid);
	}

	Result<API::Peer> Local::ConnectTo(ConnectParameters&& params) noexcept
	{
		if (IsRunning())
		{
			Concurrency::Event cevent;
			Result<API::Peer> final_result{ ResultCode::Failed };

			const auto result = m_PeerManager.ConnectTo(std::move(params),
														[&](PeerLUID pluid, Result<API::Peer> connect_result) mutable noexcept
			{
				final_result = std::move(connect_result);

				cevent.Set();
			});

			if (result.Succeeded())
			{
				if (!result->second)
				{
					// New connection; wait for completion event
					cevent.Wait();

					return final_result;
				}
				else
				{
					// Reused connection; get connection details and return them
					auto result1 = GetPeer(result->first);
					if (result1.Succeeded())
					{
						return result1;
					}

					return ResultCode::FailedRetry;
				}
			}

			return result.GetErrorCode();
		}

		return ResultCode::NotRunning;
	}

	Result<std::pair<PeerLUID, bool>> Local::ConnectTo(ConnectParameters&& params, ConnectCallback&& function) noexcept
	{
		if (IsRunning())
		{
			return m_PeerManager.ConnectTo(std::move(params), std::move(function));
		}

		return ResultCode::NotRunning;
	}

	Result<> Local::DisconnectFrom(const PeerLUID pluid) noexcept
	{
		if (IsRunning())
		{
			auto result = m_PeerManager.GetPeer(pluid);
			if (result.Succeeded())
			{
				return DisconnectFromImpl(*result);
			}

			return result.GetErrorCode();
		}

		return ResultCode::NotRunning;
	}

	Result<> Local::DisconnectFrom(API::Peer& peer) noexcept
	{
		if (IsRunning())
		{
			return DisconnectFromImpl(peer);
		}

		return ResultCode::NotRunning;
	}

	Result<> Local::DisconnectFromImpl(API::Peer& peer) noexcept
	{
		Concurrency::Event cevent;

		// Initiate disconnect from peer
		auto result = m_PeerManager.DisconnectFrom(peer,
												   [&](PeerLUID pluid, PeerUUID puuid) mutable noexcept
		{
			cevent.Set();
		});

		if (result.Succeeded())
		{
			// Wait for completion event
			cevent.Wait();
		}

		return result;
	}

	Result<> Local::DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept
	{
		if (IsRunning()) return m_PeerManager.DisconnectFrom(pluid, std::move(function));

		return ResultCode::NotRunning;
	}

	Result<> Local::DisconnectFrom(API::Peer& peer, DisconnectCallback&& function) noexcept
	{
		if (IsRunning()) return m_PeerManager.DisconnectFrom(peer, std::move(function));

		return ResultCode::NotRunning;
	}

	Result<PeerUUID> Local::GetUUID() const noexcept
	{
		if (IsRunning()) return m_Settings.GetCache().Local.UUID;

		return ResultCode::NotRunning;
	}

	Result<API::Peer> Local::GetPeer(const PeerLUID pluid) const noexcept
	{
		if (IsRunning()) return m_PeerManager.GetPeer(pluid);

		return ResultCode::NotRunning;
	}

	Result<Vector<PeerLUID>> Local::QueryPeers(const PeerQueryParameters& params) const noexcept
	{
		if (IsRunning())
		{
			try
			{
				Vector<PeerLUID> pluids;
				const auto result = QueryPeers(params, pluids);
				if (result.Succeeded())
				{
					return std::move(pluids);
				}
				else return result.GetErrorCode();
			}
			catch (...)
			{
				return ResultCode::Failed;
			}
		}

		return ResultCode::NotRunning;
	}

	Result<> Local::QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept
	{
		if (IsRunning()) return m_PeerManager.QueryPeers(params, pluids);

		return ResultCode::NotRunning;
	}

	std::pair<bool, const WChar*> Local::ValidateSecurityParameters(const SecurityParameters& params) const noexcept
	{
		if (params.General.ConnectTimeout < 0s) return { false, L"General.ConnectTimeout should be at least 0 seconds" };

		if (params.General.SuspendTimeout < 60s) return { false, L"General.SuspendTimeout should be at least 60 seconds" };
		if (params.General.MaxSuspendDuration < 0s) return { false, L"General.MaxSuspendDuration should be at least 0 seconds" };

		if (params.General.MaxHandshakeDelay < 0ms) return { false, L"General.MaxHandshakeDelay should be at least 0 milliseconds" };
		if (params.General.MaxHandshakeDuration < 0s) return { false, L"General.MaxHandshakeDuration should be at least 0 seconds" };

		// If maximum handshake delay is greater than the maximum duration,
		// the handshake will often fail, which is bad
		if (params.General.MaxHandshakeDelay > params.General.MaxHandshakeDuration)
			return { false, L"General.MaxHandshakeDelay should be greater than General.MaxHandshakeDuration" };

		if (params.General.AddressReputationImprovementInterval < 0s) return { false, L"General.AddressReputationImprovementInterval should be at least 0 seconds" };
		if (params.General.ConnectionAttempts.Interval < 0s) return { false, L"General.ConnectionAttempts.Interval should be at least 0 seconds" };

		if (params.KeyUpdate.MinInterval < 0s) return { false, L"KeyUpdate.MinInterval should be at least 0 seconds" };
		if (params.KeyUpdate.MaxInterval < 0s) return { false, L"KeyUpdate.MaxInterval should be at least 0 seconds" };
		if (params.KeyUpdate.MaxDuration < 0s) return { false, L"KeyUpdate.MaxDuration should be at least 0 seconds" };

		// Minimum should not be greater than maximum
		if (params.KeyUpdate.MinInterval > params.KeyUpdate.MaxInterval)
			return { false, L"KeyUpdate.MaxInterval should be greater than KeyUpdate.MinInterval" };

		// Should be at least 10MB
		if (params.KeyUpdate.RequireAfterNumProcessedBytes < 10'485'760) return { false, L"KeyUpdate.RequireAfterNumProcessedBytes should be at least 10.485.760 bytes" };

		if (params.Relay.ConnectTimeout < 0s) return { false, L"Relay.ConnectTimeout should be at least 0 seconds" };
		if (params.Relay.GracePeriod < 0s) return { false, L"Relay.GracePeriod should be at least 0 seconds" };
		if (params.Relay.MaxSuspendDuration < 0s) return { false, L"Relay.MaxSuspendDuration should be at least 0 seconds" };

		if (params.Relay.ConnectionAttempts.Interval < 0s) return { false, L"Relay.ConnectionAttempts.Interval should be at least 0 seconds" };

		if (params.UDP.MaxMTUDiscoveryDelay < 0ms) return { false, L"UDP.MaxMTUDiscoveryDelay should be at least 0 milliseconds" };
		if (params.UDP.MaxDecoyMessageInterval < 0ms) return { false, L"UDP.MaxDecoyMessageInterval should be at least 0 milliseconds" };
		if (params.UDP.CookieExpirationInterval < 30s) return { false, L"UDP.CookieExpirationInterval should be at least 30 seconds" };

		if (params.Message.AgeTolerance < 0s) return { false, L"Message.AgeTolerance should be at least 0 seconds" };
		if (params.Message.ExtenderGracePeriod < 0s) return { false, L"Message.ExtenderGracePeriod should be at least 0 seconds" };
		if (params.Noise.TimeInterval < 0s) return { false, L"Noise.TimeInterval should be at least 0 seconds" };

		// Minimum should not be greater than maximum
		if (params.Message.MinRandomDataPrefixSize > params.Message.MaxRandomDataPrefixSize)
			return { false, L"Message.MaxRandomDataPrefixSize should be greater than Message.MinRandomDataPrefixSize" };

		// Only supports random data prefix size up to UInt16 (2^16)
		if (params.Message.MaxRandomDataPrefixSize > std::numeric_limits<UInt16>::max())
			return { false, L"Message.MaxRandomDataPrefixSize should not be greater than 65.535 bytes" };

		if (params.Message.MinInternalRandomDataSize > params.Message.MaxInternalRandomDataSize)
			return { false, L"Message.MaxInternalRandomDataSize should be greater than Message.MinInternalRandomDataSize" };

		// Only supports random data size up to UInt16 (2^16)
		if (params.Message.MaxInternalRandomDataSize > std::numeric_limits<UInt16>::max())
			return { false, L"Message.MaxInternalRandomDataSize should not be greater than 65.535 bytes" };

		if (params.Noise.MinMessagesPerInterval > params.Noise.MaxMessagesPerInterval)
			return { false, L"Noise.MaxMessagesPerInterval should be greater than Noise.MinMessagesPerInterval" };

		if (params.Noise.MinMessageSize > params.Noise.MaxMessageSize)
			return { false, L"Noise.MaxMessageSize should be greater than Noise.MinMessageSize" };
		
		if (params.Noise.MaxMessageSize > Message::MaxMessageDataSize)
			return { false, L"Noise.MaxMessageSize should not be greater than 1.048.000 bytes" };

		return { true, L"" };
	}

	Result<Size> Local::Send(const ExtenderUUID& uuid, const std::atomic_bool& running, const std::atomic_bool& ready,
							 const PeerLUID id, const BufferView& buffer, const SendParameters& params,
							 SendCallback&& callback) noexcept
	{
		if (IsRunning()) return m_PeerManager.Send(uuid, running, ready, id, buffer, params, std::move(callback));

		return ResultCode::NotRunning;
	}

	Result<Size> Local::Send(const ExtenderUUID& uuid, const std::atomic_bool& running, const std::atomic_bool& ready,
							 API::Peer& peer, const BufferView& buffer, const SendParameters& params,
							 SendCallback&& callback) noexcept
	{
		if (IsRunning()) return m_PeerManager.Send(uuid, running, ready, peer, buffer, params, std::move(callback));

		return ResultCode::NotRunning;
	}

	Result<> Local::SendTo(const ExtenderUUID& uuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						   const PeerLUID id, Buffer&& buffer, const SendParameters& params, SendCallback&& callback) noexcept
	{
		if (IsRunning()) return m_PeerManager.SendTo(uuid, running, ready, id, std::move(buffer), params, std::move(callback));

		return ResultCode::NotRunning;
	}

	Result<> Local::SendTo(const ExtenderUUID& uuid, const std::atomic_bool& running, const std::atomic_bool& ready,
						   API::Peer& peer, Buffer&& buffer, const SendParameters& params, SendCallback&& callback) noexcept
	{
		if (IsRunning()) return m_PeerManager.SendTo(uuid, running, ready, peer, std::move(buffer), params, std::move(callback));

		return ResultCode::NotRunning;
	}

	Result<> Local::SetSecurityLevel(const SecurityLevel level,
									 const std::optional<SecurityParameters>& params, const bool silent) noexcept
	{
		auto result_code = ResultCode::Succeeded;

		m_Settings.UpdateValue([&](Settings& settings) noexcept
		{
			switch (level)
			{
				case SecurityLevel::One:
				{
					if (!silent)
					{
						LogWarn(L"Setting security level to 1");
					}

					SetDefaultSecuritySettings(settings);

					break;
				}
				case SecurityLevel::Two:
				{
					if (!silent)
					{
						LogWarn(L"Setting security level to 2");
					}

					SetDefaultSecuritySettings(settings);

					settings.Local.MaxHandshakeDelay = 3000ms;
					settings.Local.MaxHandshakeDuration = 20s;

					settings.Message.AgeTolerance = 600s;
					settings.Message.ExtenderGracePeriod = 60s;
					settings.Message.MinRandomDataPrefixSize = 0;
					settings.Message.MaxRandomDataPrefixSize = 64;
					settings.Message.MinInternalRandomDataSize = 0;
					settings.Message.MaxInternalRandomDataSize = 64;

					settings.Noise.Enabled = true;
					settings.Noise.TimeInterval = 60s;
					settings.Noise.MinMessagesPerInterval = 0;
					settings.Noise.MaxMessagesPerInterval = 30;
					settings.Noise.MinMessageSize = 0;
					settings.Noise.MaxMessageSize = 256;

					settings.UDP.ConnectCookieRequirementThreshold = 10;
					settings.UDP.CookieExpirationInterval = 120s;
					settings.UDP.MaxMTUDiscoveryDelay = 2000ms;
					settings.UDP.MaxNumDecoyMessages = 12;
					settings.UDP.MaxDecoyMessageInterval = 2000ms;
					break;
				}
				case SecurityLevel::Three:
				{
					if (!silent)
					{
						LogWarn(L"Setting security level to 3");
					}

					SetDefaultSecuritySettings(settings);

					settings.Local.MaxHandshakeDelay = 4500ms;
					settings.Local.MaxHandshakeDuration = 20s;

					settings.Local.KeyUpdate.MinInterval = 300s;
					settings.Local.KeyUpdate.MaxInterval = 600s;
					settings.Local.KeyUpdate.MaxDuration = 120s;
					settings.Local.KeyUpdate.RequireAfterNumProcessedBytes = 4'200'000'000;

					settings.Relay.ConnectTimeout = 60s;
					settings.Relay.GracePeriod = 60s;
					settings.Relay.ConnectionAttempts.MaxPerInterval = 10;
					settings.Relay.ConnectionAttempts.Interval = 10s;

					settings.Message.AgeTolerance = 300s;
					settings.Message.ExtenderGracePeriod = 60s;
					settings.Message.MinRandomDataPrefixSize = 32;
					settings.Message.MaxRandomDataPrefixSize = 64;
					settings.Message.MinInternalRandomDataSize = 0;
					settings.Message.MaxInternalRandomDataSize = 128;

					settings.Noise.Enabled = true;
					settings.Noise.TimeInterval = 60s;
					settings.Noise.MinMessagesPerInterval = 0;
					settings.Noise.MaxMessagesPerInterval = 60;
					settings.Noise.MinMessageSize = 0;
					settings.Noise.MaxMessageSize = 512;

					settings.UDP.ConnectCookieRequirementThreshold = 10;
					settings.UDP.CookieExpirationInterval = 120s;
					settings.UDP.MaxMTUDiscoveryDelay = 4000ms;
					settings.UDP.MaxNumDecoyMessages = 24;
					settings.UDP.MaxDecoyMessageInterval = 4000ms;
					break;
				}
				case SecurityLevel::Four:
				{
					if (!silent)
					{
						LogWarn(L"Setting security level to 4");
					}

					SetDefaultSecuritySettings(settings);

					settings.Local.MaxHandshakeDelay = 6000ms;
					settings.Local.MaxHandshakeDuration = 20s;

					settings.Local.KeyUpdate.MinInterval = 300s;
					settings.Local.KeyUpdate.MaxInterval = 600s;
					settings.Local.KeyUpdate.MaxDuration = 120s;
					settings.Local.KeyUpdate.RequireAfterNumProcessedBytes = 2'000'000'000;

					settings.Relay.ConnectTimeout = 60s;
					settings.Relay.GracePeriod = 60s;
					settings.Relay.ConnectionAttempts.MaxPerInterval = 10;
					settings.Relay.ConnectionAttempts.Interval = 10s;

					settings.Message.AgeTolerance = 300s;
					settings.Message.ExtenderGracePeriod = 60s;
					settings.Message.MinRandomDataPrefixSize = 32;
					settings.Message.MaxRandomDataPrefixSize = 128;
					settings.Message.MinInternalRandomDataSize = 0;
					settings.Message.MaxInternalRandomDataSize = 256;

					settings.Noise.Enabled = true;
					settings.Noise.TimeInterval = 60s;
					settings.Noise.MinMessagesPerInterval = 0;
					settings.Noise.MaxMessagesPerInterval = 120;
					settings.Noise.MinMessageSize = 0;
					settings.Noise.MaxMessageSize = 1024;

					settings.UDP.ConnectCookieRequirementThreshold = 10;
					settings.UDP.CookieExpirationInterval = 120s;
					settings.UDP.MaxMTUDiscoveryDelay = 8000ms;
					settings.UDP.MaxNumDecoyMessages = 48;
					settings.UDP.MaxDecoyMessageInterval = 8000ms;
					break;
				}
				case SecurityLevel::Five:
				{
					if (!silent)
					{
						LogWarn(L"Setting security level to 5");
					}

					SetDefaultSecuritySettings(settings);

					settings.Local.MaxHandshakeDelay = 8000ms;
					settings.Local.MaxHandshakeDuration = 20s;

					settings.Local.KeyUpdate.MinInterval = 300s;
					settings.Local.KeyUpdate.MaxInterval = 600s;
					settings.Local.KeyUpdate.MaxDuration = 120s;
					settings.Local.KeyUpdate.RequireAfterNumProcessedBytes = 1'000'000'000;

					settings.Relay.ConnectTimeout = 60s;
					settings.Relay.GracePeriod = 60s;
					settings.Relay.ConnectionAttempts.MaxPerInterval = 10;
					settings.Relay.ConnectionAttempts.Interval = 10s;

					settings.Message.AgeTolerance = 300s;
					settings.Message.ExtenderGracePeriod = 60s;
					settings.Message.MinRandomDataPrefixSize = 32;
					settings.Message.MaxRandomDataPrefixSize = 256;
					settings.Message.MinInternalRandomDataSize = 0;
					settings.Message.MaxInternalRandomDataSize = 512;

					settings.Noise.Enabled = true;
					settings.Noise.TimeInterval = 60s;
					settings.Noise.MinMessagesPerInterval = 0;
					settings.Noise.MaxMessagesPerInterval = 240;
					settings.Noise.MinMessageSize = 0;
					settings.Noise.MaxMessageSize = 2048;

					settings.UDP.ConnectCookieRequirementThreshold = 10;
					settings.UDP.CookieExpirationInterval = 120s;
					settings.UDP.MaxMTUDiscoveryDelay = 16000ms;
					settings.UDP.MaxNumDecoyMessages = 96;
					settings.UDP.MaxDecoyMessageInterval = 16000ms;
					break;
				}
				case SecurityLevel::Custom:
				{
					if (params)
					{
						const auto [success, error_msg] = ValidateSecurityParameters(*params);
						if (success)
						{
							if (!silent)
							{
								LogWarn(L"Setting security level to Custom");
							}

							settings.Local.ConnectTimeout = params->General.ConnectTimeout;
							
							settings.Local.SuspendTimeout = params->General.SuspendTimeout;
							settings.Local.MaxSuspendDuration = params->General.MaxSuspendDuration;
							
							settings.Local.MaxHandshakeDelay = params->General.MaxHandshakeDelay;
							settings.Local.MaxHandshakeDuration = params->General.MaxHandshakeDuration;
							settings.Local.AddressReputationImprovementInterval = params->General.AddressReputationImprovementInterval;
							settings.Local.ConnectionAttempts.MaxPerInterval = params->General.ConnectionAttempts.MaxPerInterval;
							settings.Local.ConnectionAttempts.Interval = params->General.ConnectionAttempts.Interval;

							settings.Local.KeyUpdate.MinInterval = params->KeyUpdate.MinInterval;
							settings.Local.KeyUpdate.MaxInterval = params->KeyUpdate.MaxInterval;
							settings.Local.KeyUpdate.MaxDuration = params->KeyUpdate.MaxDuration;
							settings.Local.KeyUpdate.RequireAfterNumProcessedBytes = params->KeyUpdate.RequireAfterNumProcessedBytes;

							settings.Relay.ConnectTimeout = params->Relay.ConnectTimeout;
							settings.Relay.GracePeriod = params->Relay.GracePeriod;
							settings.Relay.MaxSuspendDuration = params->Relay.MaxSuspendDuration;
							settings.Relay.ConnectionAttempts.MaxPerInterval = params->Relay.ConnectionAttempts.MaxPerInterval;
							settings.Relay.ConnectionAttempts.Interval = params->Relay.ConnectionAttempts.Interval;

							settings.Message.AgeTolerance = params->Message.AgeTolerance;
							settings.Message.ExtenderGracePeriod = params->Message.ExtenderGracePeriod;
							settings.Message.MinRandomDataPrefixSize = params->Message.MinRandomDataPrefixSize;
							settings.Message.MaxRandomDataPrefixSize = params->Message.MaxRandomDataPrefixSize;
							settings.Message.MinInternalRandomDataSize = params->Message.MinInternalRandomDataSize;
							settings.Message.MaxInternalRandomDataSize = params->Message.MaxInternalRandomDataSize;

							settings.Noise.Enabled = params->Noise.Enabled;
							settings.Noise.TimeInterval = params->Noise.TimeInterval;
							settings.Noise.MinMessagesPerInterval = params->Noise.MinMessagesPerInterval;
							settings.Noise.MaxMessagesPerInterval = params->Noise.MaxMessagesPerInterval;
							settings.Noise.MinMessageSize = params->Noise.MinMessageSize;
							settings.Noise.MaxMessageSize = params->Noise.MaxMessageSize;

							settings.UDP.ConnectCookieRequirementThreshold = params->UDP.ConnectCookieRequirementThreshold;
							settings.UDP.CookieExpirationInterval = params->UDP.CookieExpirationInterval;
							settings.UDP.MaxMTUDiscoveryDelay = params->UDP.MaxMTUDiscoveryDelay;
							settings.UDP.MaxNumDecoyMessages = params->UDP.MaxNumDecoyMessages;
							settings.UDP.MaxDecoyMessageInterval = params->UDP.MaxDecoyMessageInterval;
						}
						else
						{
							if (!silent)
							{
								LogErr(L"Invalid parameters passed for Custom security level (%s)", error_msg);
							}

							result_code = ResultCode::InvalidArgument;
						}
					}
					else
					{
						if (!silent)
						{
							LogErr(L"No parameters passed for Custom security level");
						}

						result_code = ResultCode::InvalidArgument;
					}

					break;
				}
				default:
				{
					if (!silent)
					{
						LogErr(L"Invalid security level");
					}

					result_code = ResultCode::InvalidArgument;
					break;
				}
			}

			if (result_code == ResultCode::Succeeded) m_SecurityLevel = level;
		});

		return result_code;
	}

	SecurityLevel Local::GetSecurityLevel() const noexcept
	{
		return m_SecurityLevel;
	}

	SecurityParameters Local::GetSecurityParameters() const noexcept
	{
		SecurityParameters params;

		const auto& settings = m_Settings.GetCache();

		params.General.ConnectTimeout = settings.Local.ConnectTimeout;
		params.General.SuspendTimeout = settings.Local.SuspendTimeout;
		params.General.MaxSuspendDuration = settings.Local.MaxSuspendDuration;
		params.General.MaxHandshakeDelay = settings.Local.MaxHandshakeDelay;
		params.General.MaxHandshakeDuration = settings.Local.MaxHandshakeDuration;

		params.General.AddressReputationImprovementInterval = settings.Local.AddressReputationImprovementInterval;

		params.General.ConnectionAttempts.MaxPerInterval = settings.Local.ConnectionAttempts.MaxPerInterval;
		params.General.ConnectionAttempts.Interval = settings.Local.ConnectionAttempts.Interval;

		params.KeyUpdate.MinInterval = settings.Local.KeyUpdate.MinInterval;
		params.KeyUpdate.MaxInterval = settings.Local.KeyUpdate.MaxInterval;
		params.KeyUpdate.MaxDuration = settings.Local.KeyUpdate.MaxDuration;
		params.KeyUpdate.RequireAfterNumProcessedBytes = settings.Local.KeyUpdate.RequireAfterNumProcessedBytes;

		params.Relay.ConnectTimeout = settings.Relay.ConnectTimeout;
		params.Relay.GracePeriod = settings.Relay.GracePeriod;
		params.Relay.MaxSuspendDuration = settings.Relay.MaxSuspendDuration;
		params.Relay.ConnectionAttempts.MaxPerInterval = settings.Relay.ConnectionAttempts.MaxPerInterval;
		params.Relay.ConnectionAttempts.Interval = settings.Relay.ConnectionAttempts.Interval;

		params.Message.AgeTolerance = settings.Message.AgeTolerance;
		params.Message.ExtenderGracePeriod = settings.Message.ExtenderGracePeriod;
		params.Message.MinRandomDataPrefixSize = settings.Message.MinRandomDataPrefixSize;
		params.Message.MaxRandomDataPrefixSize = settings.Message.MaxRandomDataPrefixSize;
		params.Message.MinInternalRandomDataSize = settings.Message.MinInternalRandomDataSize;
		params.Message.MaxInternalRandomDataSize = settings.Message.MaxInternalRandomDataSize;

		params.Noise.Enabled = settings.Noise.Enabled;
		params.Noise.TimeInterval = settings.Noise.TimeInterval;
		params.Noise.MinMessagesPerInterval = settings.Noise.MinMessagesPerInterval;
		params.Noise.MaxMessagesPerInterval = settings.Noise.MaxMessagesPerInterval;
		params.Noise.MinMessageSize = settings.Noise.MinMessageSize;
		params.Noise.MaxMessageSize = settings.Noise.MaxMessageSize;

		params.UDP.ConnectCookieRequirementThreshold = settings.UDP.ConnectCookieRequirementThreshold;
		params.UDP.CookieExpirationInterval = settings.UDP.CookieExpirationInterval;
		params.UDP.MaxMTUDiscoveryDelay = settings.UDP.MaxMTUDiscoveryDelay;
		params.UDP.MaxNumDecoyMessages = settings.UDP.MaxNumDecoyMessages;
		params.UDP.MaxDecoyMessageInterval = settings.UDP.MaxDecoyMessageInterval;

		return params;
	}

	void Local::SetDefaultSecuritySettings(Settings& settings) noexcept
	{
		settings.Local.ConnectTimeout = 60s;
		settings.Local.SuspendTimeout = 60s;
		settings.Local.MaxSuspendDuration = 60s;
		settings.Local.MaxHandshakeDelay = 0ms;
		settings.Local.MaxHandshakeDuration = 30s;

		settings.Local.AddressReputationImprovementInterval = 600s;

		settings.Local.ConnectionAttempts.MaxPerInterval = 2;
		settings.Local.ConnectionAttempts.Interval = 10s;

		settings.Local.KeyUpdate.MinInterval = 300s;
		settings.Local.KeyUpdate.MaxInterval = 1200s;
		settings.Local.KeyUpdate.MaxDuration = 240s;
		settings.Local.KeyUpdate.RequireAfterNumProcessedBytes = 4'200'000'000;

		settings.Relay.ConnectTimeout = 60s;
		settings.Relay.GracePeriod = 60s;
		settings.Relay.MaxSuspendDuration = 60s;
		settings.Relay.ConnectionAttempts.MaxPerInterval = 10;
		settings.Relay.ConnectionAttempts.Interval = 10s;

		settings.Message.AgeTolerance = 600s;
		settings.Message.ExtenderGracePeriod = 60s;
		settings.Message.MinRandomDataPrefixSize = 0;
		settings.Message.MaxRandomDataPrefixSize = 0;
		settings.Message.MinInternalRandomDataSize = 0;
		settings.Message.MaxInternalRandomDataSize = 64;

		settings.Noise.Enabled = false;
		settings.Noise.TimeInterval = 0s;
		settings.Noise.MinMessagesPerInterval = 0;
		settings.Noise.MaxMessagesPerInterval = 0;
		settings.Noise.MinMessageSize = 0;
		settings.Noise.MaxMessageSize = 0;

		settings.UDP.ConnectCookieRequirementThreshold = 10;
		settings.UDP.CookieExpirationInterval = 120s;
		settings.UDP.MaxMTUDiscoveryDelay = 0ms;
		settings.UDP.MaxNumDecoyMessages = 0;
		settings.UDP.MaxDecoyMessageInterval = 1000ms;
	}

	void Local::FreeUnusedMemory() noexcept
	{
		LogDbg(L"Freeing unused memory...");

		Memory::PoolAllocator::Allocator<void>::FreeUnused();
		Memory::PoolAllocator::ProtectedAllocator<void>::FreeUnused();

		LogSys(L"Freed unused memory");
	}
}