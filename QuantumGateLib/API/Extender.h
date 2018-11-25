// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core
{
	class Local;
}

namespace QuantumGate::Implementation::Core::Extender
{
	class Extender;
	class Manager;
	class Control;
}

namespace QuantumGate::API
{
	class Export Extender
	{
		friend class QuantumGate::Implementation::Core::Local;
		friend class QuantumGate::Implementation::Core::Extender::Manager;
		friend class QuantumGate::Implementation::Core::Extender::Control;

	public:
		Extender() = delete;
		Extender(const Extender&) = delete;
		Extender(Extender&&) = default;
		virtual ~Extender() = default;
		Extender& operator=(const Extender&) = delete;
		Extender& operator=(Extender&&) = default;

		[[nodiscard]] const ExtenderUUID& GetUUID() const noexcept;
		[[nodiscard]] const String& GetName() const noexcept;

		Result<std::tuple<UInt, UInt, UInt, UInt>> GetLocalVersion() const noexcept;
		Result<std::pair<UInt, UInt>> GetLocalProtocolVersion() const noexcept;
		Result<PeerUUID> GetLocalUUID() const noexcept;

		[[nodiscard]] const bool IsRunning() const noexcept;

		Result<ConnectDetails> ConnectTo(ConnectParameters&& params) noexcept;
		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params,
													ConnectCallback&& function) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept;

		Result<> SendMessageTo(const PeerLUID pluid, Buffer&& buffer, const bool compress = true) const;

		[[nodiscard]] const Size GetMaximumMessageDataSize() const noexcept;

		Result<PeerDetails> GetPeerDetails(const PeerLUID pluid) const noexcept;
		Result<Vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;
		Result<> QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept;

		Result<> SetStartupCallback(ExtenderStartupCallback&& function) noexcept;
		Result<> SetPostStartupCallback(ExtenderPostStartupCallback&& function) noexcept;
		Result<> SetPreShutdownCallback(ExtenderPreShutdownCallback&& function) noexcept;
		Result<> SetShutdownCallback(ExtenderShutdownCallback&& function) noexcept;
		Result<> SetPeerEventCallback(ExtenderPeerEventCallback&& function) noexcept;
		Result<> SetPeerMessageCallback(ExtenderPeerMessageCallback&& function) noexcept;

	protected:
		Extender(const ExtenderUUID& uuid, const String& name);

	private:
		std::shared_ptr<QuantumGate::Implementation::Core::Extender::Extender> m_Extender{ nullptr };
	};
}