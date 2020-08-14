// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Access.h"
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
			struct PublicIPAddressDetails
			{
				bool ReportedByPeers{ false };
				bool ReportedByTrustedPeers{ false };
				Size NumReportingNetworks{ 0 };
				bool Verified{ false };
			};

			struct IPAddressDetails
			{
				IPAddress IPAddress;
				bool BoundToLocalEthernetInterface{ false };
				std::optional<PublicIPAddressDetails> PublicDetails;
			};

			struct EthernetInterface
			{
				String Name;
				String Description;
				String MACAddress;
				bool Operational{ false };
				Vector<IPAddress> IPAddresses;
			};

			Environment() = delete;
			Environment(const Environment&) = delete;
			Environment(Environment&&) noexcept = default;
			~Environment() = default;
			Environment& operator=(const Environment&) = delete;
			Environment& operator=(Environment&&) noexcept = default;

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
		Local(Local&&) noexcept = default;
		virtual ~Local() = default;
		Local& operator=(const Local&) = delete;
		Local& operator=(Local&&) noexcept = default;

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

		[[nodiscard]] Environment GetEnvironment() const noexcept;

		[[nodiscard]] Access::Manager& GetAccessManager() noexcept;

		[[nodiscard]] std::tuple<UInt, UInt, UInt, UInt> GetVersion() const noexcept;
		[[nodiscard]] String GetVersionString() const noexcept;
		[[nodiscard]] std::pair<UInt, UInt> GetProtocolVersion() const noexcept;
		[[nodiscard]] String GetProtocolVersionString() const noexcept;

		Result<PeerUUID> GetUUID() const noexcept;

		Result<Peer> GetPeer(const PeerLUID pluid) const noexcept;

		Result<Vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;
		Result<> QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& peers) const noexcept;

		Result<bool> AddExtender(const std::shared_ptr<Extender>& extender) noexcept;
		Result<> RemoveExtender(const std::shared_ptr<Extender>& extender) noexcept;

		Result<> AddExtenderModule(const Path& module_path) noexcept;
		Result<> RemoveExtenderModule(const Path& module_path) noexcept;

		[[nodiscard]] bool HasExtender(const ExtenderUUID& extuuid) const noexcept;
		[[nodiscard]] std::weak_ptr<Extender> GetExtender(const ExtenderUUID& extuuid) const noexcept;

		Result<Peer> ConnectTo(ConnectParameters&& params) noexcept;
		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params,
													ConnectCallback&& function) noexcept;

		Result<> DisconnectFrom(const PeerLUID pluid) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept;
		Result<> DisconnectFrom(Peer& peer) noexcept;
		Result<> DisconnectFrom(Peer& peer, DisconnectCallback&& function) noexcept;

		Result<> SetSecurityLevel(const SecurityLevel level,
								  const std::optional<SecurityParameters>& params = std::nullopt) noexcept;
		[[nodiscard]] SecurityLevel GetSecurityLevel() const noexcept;
		[[nodiscard]] SecurityParameters GetSecurityParameters() const noexcept;

		void FreeUnusedMemory() noexcept;

	private:
		std::shared_ptr<QuantumGate::Implementation::Core::Local> m_Local{ nullptr };
		Access::Manager m_AccessManager;
	};
}