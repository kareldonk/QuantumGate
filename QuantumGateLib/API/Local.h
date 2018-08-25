// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "LocalEnvironment.h"
#include "AccessManager.h"
#include "Extender.h"

namespace QuantumGate::Implementation::Core
{
	class Local;
}

namespace QuantumGate::API
{
	class Export Local
	{
	public:
		Local() noexcept;
		Local(const Local&) = delete;
		Local(Local&&) = default;
		virtual ~Local() = default;
		Local& operator=(const Local&) = delete;
		Local& operator=(Local&&) = default;

		Result<> Startup(const StartupParameters& params) noexcept;
		Result<> Shutdown() noexcept;
		const bool IsRunning() const noexcept;

		Result<> EnableListeners() noexcept;
		Result<> DisableListeners() noexcept;
		const bool AreListenersEnabled() const noexcept;

		Result<> EnableExtenders() noexcept;
		Result<> DisableExtenders() noexcept;
		const bool AreExtendersEnabled() const noexcept;

		Result<> EnableRelays() noexcept;
		Result<> DisableRelays() noexcept;
		const bool AreRelaysEnabled() const noexcept;

		const LocalEnvironment GetEnvironment() const noexcept;

		AccessManager& GetAccessManager() noexcept;

		std::tuple<UInt, UInt, UInt, UInt> GetVersion() const noexcept;
		String GetVersionString() const noexcept;
		std::pair<UInt, UInt> GetProtocolVersion() const noexcept;
		String GetProtocolVersionString() const noexcept;

		Result<PeerUUID> GetUUID() const noexcept;
		Result<PeerDetails> GetPeerDetails(const PeerLUID pluid) const noexcept;
		Result<std::vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;

		Result<bool> AddExtender(const std::shared_ptr<Extender>& extender) noexcept;
		Result<> RemoveExtender(const std::shared_ptr<Extender>& extender) noexcept;

		Result<> AddExtenderModule(const Path& module_path) noexcept;
		Result<> RemoveExtenderModule(const Path& module_path) noexcept;

		const bool HasExtender(const ExtenderUUID& extuuid) const noexcept;
		std::weak_ptr<Extender> GetExtender(const ExtenderUUID& extuuid) const noexcept;

		Result<ConnectDetails> ConnectTo(ConnectParameters&& params) noexcept;
		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params,
													ConnectCallback&& function) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept;

		Result<> SetSecurityLevel(const SecurityLevel level,
								  const std::optional<SecurityParameters>& params = std::nullopt) noexcept;
		[[nodiscard]] const SecurityLevel GetSecurityLevel() const noexcept;
		[[nodiscard]] SecurityParameters GetSecurityParameters() const noexcept;

	private:
		std::shared_ptr<QuantumGate::Implementation::Core::Local> m_Local{ nullptr };
		AccessManager m_AccessManager;
	};
}