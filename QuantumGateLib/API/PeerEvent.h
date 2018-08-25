// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core::Peer
{
	class Event;
}

namespace QuantumGate::API
{
	class Export PeerEvent
	{
	public:
		PeerEvent(QuantumGate::Implementation::Core::Peer::Event&& event) noexcept;
		PeerEvent(const PeerEvent&) = delete;
		PeerEvent(PeerEvent&& other) noexcept;
		virtual ~PeerEvent();
		PeerEvent& operator=(const PeerEvent&) = delete;
		PeerEvent& operator=(PeerEvent&& other) noexcept;

		explicit operator bool() const noexcept;

		const PeerEventType GetType() const noexcept;
		const PeerLUID GetPeerLUID() const noexcept;
		const PeerUUID& GetPeerUUID() const noexcept;
		const Buffer* GetMessageData() const noexcept;

	private:
		void Reset() noexcept;

	private:
		QuantumGate::Implementation::Core::Peer::Event* m_PeerEvent{ nullptr };
	};
}