// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

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
		class Export Environment final
		{
			friend class Local;

		public:
			Environment() = delete;
			Environment(const Environment&) = delete;
			Environment(Environment&&) = default;
			~Environment() = default;
			Environment& operator=(const Environment&) = delete;
			Environment& operator=(Environment&&) = default;

			Result<String> GetHostname() const noexcept;
			Result<String> GetUsername() const noexcept;
			Result<Vector<IPAddressDetails>> GetIPAddresses() const noexcept;
			Result<Vector<EthernetInterface>> GetEthernetInterfaces() const noexcept;

		private:
			Environment(const void* localenv) noexcept;

		private:
			const void* m_LocalEnvironment{ nullptr };
		};

		Local();
		Local(const Local&) = delete;
		Local(Local&&) = default;
		virtual ~Local() = default;
		Local& operator=(const Local&) = delete;
		Local& operator=(Local&&) = default;

		Result<> Startup(const StartupParameters& params) noexcept;
		Result<> Shutdown() noexcept;
		[[nodiscard]] bool IsRunning() const noexcept;

		Result<> EnableListeners() noexcept;
		Result<> DisableListeners() noexcept;
		[[nodiscard]] bool AreListenersEnabled() const noexcept;

		Result<> EnableExtenders() noexcept;
		Result<> DisableExtenders() noexcept;
		[[nodiscard]] bool AreExtendersEnabled() const noexcept;

		Result<> EnableRelays() noexcept;
		Result<> DisableRelays() noexcept;
		[[nodiscard]] bool AreRelaysEnabled() const noexcept;

		[[nodiscard]] const Environment GetEnvironment() const noexcept;

		[[nodiscard]] Access::Manager& GetAccessManager() noexcept;

		[[nodiscard]] std::tuple<UInt, UInt, UInt, UInt> GetVersion() const noexcept;
		[[nodiscard]] String GetVersionString() const noexcept;
		[[nodiscard]] std::pair<UInt, UInt> GetProtocolVersion() const noexcept;
		[[nodiscard]] String GetProtocolVersionString() const noexcept;

		Result<PeerUUID> GetUUID() const noexcept;
		Result<PeerDetails> GetPeerDetails(const PeerLUID pluid) const noexcept;

		Result<Vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;
		Result<> QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& peers) const noexcept;

		Result<bool> AddExtender(const std::shared_ptr<Extender>& extender) noexcept;
		Result<> RemoveExtender(const std::shared_ptr<Extender>& extender) noexcept;

		Result<> AddExtenderModule(const Path& module_path) noexcept;
		Result<> RemoveExtenderModule(const Path& module_path) noexcept;

		[[nodiscard]] bool HasExtender(const ExtenderUUID& extuuid) const noexcept;
		[[nodiscard]] std::weak_ptr<Extender> GetExtender(const ExtenderUUID& extuuid) const noexcept;

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
		Access::Manager m_AccessManager;
	};
}