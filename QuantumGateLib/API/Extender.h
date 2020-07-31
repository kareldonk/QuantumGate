// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Peer.h"

namespace QuantumGate::Implementation::Core
{
	class Local;
}

namespace QuantumGate::Implementation::Core::Peer
{
	class Event;
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
		class Export PeerEvent
		{
			friend class QuantumGate::Implementation::Core::Extender::Control;

		public:
			enum class Type : UInt16
			{
				Unknown, Connected, Disconnected, Message
			};

			struct Result
			{
				bool Handled{ false };
				bool Success{ false };
			};

			PeerEvent() = delete;
			PeerEvent(const PeerEvent&) = delete;
			PeerEvent(PeerEvent&& other) noexcept;
			~PeerEvent();
			PeerEvent& operator=(const PeerEvent&) = delete;
			PeerEvent& operator=(PeerEvent&& other) noexcept;

			explicit operator bool() const noexcept;

			[[nodiscard]] Type GetType() const noexcept;
			[[nodiscard]] PeerLUID GetPeerLUID() const noexcept;
			[[nodiscard]] const PeerUUID& GetPeerUUID() const noexcept;
			[[nodiscard]] const Buffer* GetMessageData() const noexcept;

		private:
			PeerEvent(QuantumGate::Implementation::Core::Peer::Event&& event) noexcept;

			[[nodiscard]] QuantumGate::Implementation::Core::Peer::Event* GetEvent() noexcept;
			[[nodiscard]] const QuantumGate::Implementation::Core::Peer::Event* GetEvent() const noexcept;

			[[nodiscard]] bool HasEvent() const noexcept;
			void SetHasEvent(const bool flag) noexcept;

			void Reset() noexcept;

		private:
			typename std::aligned_storage<144>::type m_PeerEvent{ 0 };
		};

		using StartupCallback = Callback<bool(void)>;
		using PostStartupCallback = Callback<void(void)>;
		using PreShutdownCallback = Callback<void(void)>;
		using ShutdownCallback = Callback<void(void)>;
		using PeerEventCallback = Callback<void(PeerEvent&&)>;
		using PeerMessageCallback = Callback<PeerEvent::Result(PeerEvent&&)>;

		Extender() = delete;
		Extender(const Extender&) = delete;
		Extender(Extender&&) noexcept = default;
		virtual ~Extender() = default;
		Extender& operator=(const Extender&) = delete;
		Extender& operator=(Extender&&) noexcept = default;

		[[nodiscard]] const ExtenderUUID& GetUUID() const noexcept;
		[[nodiscard]] const String& GetName() const noexcept;

		Result<std::tuple<UInt, UInt, UInt, UInt>> GetLocalVersion() const noexcept;
		Result<std::pair<UInt, UInt>> GetLocalProtocolVersion() const noexcept;
		Result<PeerUUID> GetLocalUUID() const noexcept;

		[[nodiscard]] bool IsRunning() const noexcept;

		Result<ConnectDetails> ConnectTo(ConnectParameters&& params) noexcept;
		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params, ConnectCallback&& function) noexcept;

		Result<> DisconnectFrom(const PeerLUID pluid) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept;
		Result<> DisconnectFrom(Peer& peer) noexcept;
		Result<> DisconnectFrom(Peer& peer, DisconnectCallback&& function) noexcept;

		Result<Size> SendMessage(const PeerLUID pluid, const BufferView& buffer,
								 const SendParameters& params, SendCallback&& callback = nullptr) const noexcept;
		Result<Size> SendMessage(Peer& peer, const BufferView& buffer,
								 const SendParameters& params, SendCallback&& callback = nullptr) const noexcept;

		Result<> SendMessageTo(const PeerLUID pluid, Buffer&& buffer,
							   const SendParameters& params, SendCallback&& callback = nullptr) const noexcept;
		Result<> SendMessageTo(Peer& peer, Buffer&& buffer,
							   const SendParameters& params, SendCallback&& callback = nullptr) const noexcept;

		[[nodiscard]] static Size GetMaximumMessageDataSize() noexcept;

		Result<Peer> GetPeer(const PeerLUID pluid) const noexcept;

		Result<Vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;
		Result<> QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept;

		Result<> SetStartupCallback(StartupCallback&& function) noexcept;
		Result<> SetPostStartupCallback(PostStartupCallback&& function) noexcept;
		Result<> SetPreShutdownCallback(PreShutdownCallback&& function) noexcept;
		Result<> SetShutdownCallback(ShutdownCallback&& function) noexcept;
		Result<> SetPeerEventCallback(PeerEventCallback&& function) noexcept;
		Result<> SetPeerMessageCallback(PeerMessageCallback&& function) noexcept;

	protected:
		Extender(const ExtenderUUID& uuid, const String& name);

	private:
		std::shared_ptr<QuantumGate::Implementation::Core::Extender::Extender> m_Extender{ nullptr };
	};
}