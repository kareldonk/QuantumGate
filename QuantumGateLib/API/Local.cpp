// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Local.h"
#include "..\Core\Local.h"
#include "..\Core\LocalEnvironment.h"

namespace QuantumGate::API
{
	Local::Environment::Environment(const void* localenv) noexcept :
		m_LocalEnvironment(localenv)
	{
		assert(m_LocalEnvironment != nullptr);
	}

	Result<String> Local::Environment::GetHostname() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		try
		{
			using namespace QuantumGate::Implementation::Core;
			const auto local_env = static_cast<const LocalEnvironment_ThS*>(m_LocalEnvironment)->WithSharedLock();
			if (local_env->IsInitialized()) return local_env->GetHostname();
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<String> Local::Environment::GetUsername() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		try
		{
			using namespace QuantumGate::Implementation::Core;
			const auto local_env = static_cast<const LocalEnvironment_ThS*>(m_LocalEnvironment)->WithSharedLock();
			if (local_env->IsInitialized()) return local_env->GetUsername();
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<Vector<API::Local::Environment::IPAddressDetails>> Local::Environment::GetIPAddresses() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		try
		{
			using namespace QuantumGate::Implementation::Core;
			const auto local_env = static_cast<const LocalEnvironment_ThS*>(m_LocalEnvironment)->WithSharedLock();
			if (local_env->IsInitialized())
			{
				return local_env->GetIPAddresses();
			}
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<Vector<API::Local::Environment::EthernetInterface>> Local::Environment::GetEthernetInterfaces() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		try
		{
			using namespace QuantumGate::Implementation::Core;
			const auto local_env = static_cast<const LocalEnvironment_ThS*>(m_LocalEnvironment)->WithSharedLock();
			if (local_env->IsInitialized())
			{
				// This is making a copy
				return local_env->GetEthernetInterfaces();
			}
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Local::Local() :
		m_Local(std::make_shared<QuantumGate::Implementation::Core::Local>()),
		m_AccessManager(&m_Local->GetAccessManager())
	{}

	Result<> Local::Startup(const StartupParameters& params) noexcept
	{
		return m_Local->Startup(params);
	}

	Result<> Local::Shutdown() noexcept
	{
		return m_Local->Shutdown();
	}

	bool Local::IsRunning() const noexcept
	{
		return m_Local->IsRunning();
	}

	Result<> Local::EnableListeners() noexcept
	{
		return m_Local->EnableListeners();
	}

	Result<> Local::DisableListeners() noexcept
	{
		return m_Local->DisableListeners();
	}

	bool Local::AreListenersEnabled() const noexcept
	{
		return m_Local->AreListenersEnabled();
	}

	Result<> Local::EnableExtenders() noexcept
	{
		return m_Local->EnableExtenders();
	}

	Result<> Local::DisableExtenders() noexcept
	{
		return m_Local->DisableExtenders();
	}

	bool Local::AreExtendersEnabled() const noexcept
	{
		return m_Local->AreExtendersEnabled();
	}

	Result<> Local::EnableRelays() noexcept
	{
		return m_Local->EnableRelays();
	}

	Result<> Local::DisableRelays() noexcept
	{
		return m_Local->DisableRelays();
	}

	bool Local::AreRelaysEnabled() const noexcept
	{
		return m_Local->AreRelaysEnabled();
	}

	Local::Environment Local::GetEnvironment() const noexcept
	{
		return Local::Environment(&m_Local->GetEnvironment());
	}

	Access::Manager& Local::GetAccessManager() noexcept
	{
		return m_AccessManager;
	}

	Result<bool> Local::AddExtender(const std::shared_ptr<Extender>& extender) noexcept
	{
		return m_Local->AddExtender(extender);
	}

	Result<> Local::RemoveExtender(const std::shared_ptr<Extender>& extender) noexcept
	{
		return m_Local->RemoveExtender(extender);
	}

	Result<> Local::AddExtenderModule(const Path& module_path) noexcept
	{
		return m_Local->AddExtenderModule(module_path);
	}

	Result<> Local::RemoveExtenderModule(const Path& module_path) noexcept
	{
		return m_Local->RemoveExtenderModule(module_path);
	}

	bool Local::HasExtender(const ExtenderUUID& extuuid) const noexcept
	{
		return m_Local->HasExtender(extuuid);
	}

	std::weak_ptr<Extender> Local::GetExtender(const ExtenderUUID& extuuid) const noexcept
	{
		return m_Local->GetExtender(extuuid);
	}

	Result<ConnectDetails> Local::ConnectTo(ConnectParameters&& params) noexcept
	{
		return m_Local->ConnectTo(std::move(params));
	}

	Result<std::pair<PeerLUID, bool>> Local::ConnectTo(ConnectParameters&& params,
													   ConnectCallback&& function) noexcept
	{
		return m_Local->ConnectTo(std::move(params), std::move(function));
	}

	Result<> Local::DisconnectFrom(const PeerLUID pluid) noexcept
	{
		return m_Local->DisconnectFrom(pluid);
	}

	Result<> Local::DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept
	{
		return m_Local->DisconnectFrom(pluid, std::move(function));
	}

	Result<> Local::DisconnectFrom(Peer& peer) noexcept
	{
		return m_Local->DisconnectFrom(peer);
	}

	Result<> Local::DisconnectFrom(Peer& peer, DisconnectCallback&& function) noexcept
	{
		return m_Local->DisconnectFrom(peer, std::move(function));
	}

	std::tuple<UInt, UInt, UInt, UInt> Local::GetVersion() const noexcept
	{
		return m_Local->GetVersion();
	}

	String Local::GetVersionString() const noexcept
	{
		return m_Local->GetVersionString();
	}

	std::pair<UInt, UInt> Local::GetProtocolVersion() const noexcept
	{
		return m_Local->GetProtocolVersion();
	}

	String Local::GetProtocolVersionString() const noexcept
	{
		return m_Local->GetProtocolVersionString();
	}

	Result<PeerUUID> Local::GetUUID() const noexcept
	{
		return m_Local->GetUUID();
	}

	Result<Peer> Local::GetPeer(const PeerLUID pluid) const noexcept
	{
		return m_Local->GetPeer(pluid);
	}

	Result<Vector<PeerLUID>> Local::QueryPeers(const PeerQueryParameters& params) const noexcept
	{
		return m_Local->QueryPeers(params);
	}

	Result<> Local::QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept
	{
		return m_Local->QueryPeers(params, pluids);
	}

	Result<> Local::SetSecurityLevel(const SecurityLevel level,
									 const std::optional<SecurityParameters>& params) noexcept
	{
		return m_Local->SetSecurityLevel(level, params);
	}

	SecurityLevel Local::GetSecurityLevel() const noexcept
	{
		return m_Local->GetSecurityLevel();
	}

	SecurityParameters Local::GetSecurityParameters() const noexcept
	{
		return m_Local->GetSecurityParameters();
	}

	void Local::FreeUnusedMemory() noexcept
	{
		return m_Local->FreeUnusedMemory();
	}
}