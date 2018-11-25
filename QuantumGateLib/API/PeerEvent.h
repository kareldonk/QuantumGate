// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core::Peer
{
	class Event;
}

namespace QuantumGate::Implementation::Core::Extender
{
	class Control;
}

namespace QuantumGate::API
{
	class Export PeerEvent final
	{
		friend class QuantumGate::Implementation::Core::Extender::Control;

	public:
		PeerEvent() = delete;
		PeerEvent(const PeerEvent&) = delete;
		PeerEvent(PeerEvent&& other) noexcept;
		~PeerEvent();
		PeerEvent& operator=(const PeerEvent&) = delete;
		PeerEvent& operator=(PeerEvent&& other) noexcept;

		explicit operator bool() const noexcept;

		[[nodiscard]] const PeerEventType GetType() const noexcept;
		[[nodiscard]] const PeerLUID GetPeerLUID() const noexcept;
		[[nodiscard]] const PeerUUID& GetPeerUUID() const noexcept;
		[[nodiscard]] const Buffer* GetMessageData() const noexcept;

	private:
		PeerEvent(QuantumGate::Implementation::Core::Peer::Event&& event) noexcept;

		[[nodiscard]] QuantumGate::Implementation::Core::Peer::Event* GetEvent() noexcept;
		[[nodiscard]] const QuantumGate::Implementation::Core::Peer::Event* GetEvent() const noexcept;
		
		[[nodiscard]] const bool HasEvent() const noexcept;
		void SetHasEvent(const bool flag) noexcept;

		void Reset() noexcept;

	private:
		typename std::aligned_storage<128>::type m_PeerEvent{ 0 };
	};
}