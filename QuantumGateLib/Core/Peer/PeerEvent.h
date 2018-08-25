// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\MessageDetails.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Event
	{
	public:
		Event() = default;
		Event(PeerEventType type, PeerLUID pluid, PeerUUID puuid) noexcept;
		Event(PeerEventType type, PeerLUID pluid, PeerUUID puuid, MessageDetails&& msg) noexcept;
		Event(const Event& other) noexcept;
		Event(Event&& other) noexcept;
		virtual ~Event() = default;
		Event& operator=(const Event&) = default;
		Event& operator=(Event&& other) noexcept;

		void swap(Event& other) noexcept;

		explicit operator bool() const noexcept;

		inline const PeerEventType GetType() const noexcept { return m_Type; }
		inline const PeerLUID GetPeerLUID() const noexcept { return m_PeerLUID; }
		inline const PeerUUID& GetPeerUUID() const noexcept { return m_PeerUUID; }

		const ExtenderUUID* GetExtenderUUID() const noexcept;
		const Buffer* GetMessageData() const noexcept;

	private:
		PeerEventType m_Type{ PeerEventType::Unknown };
		PeerLUID m_PeerLUID{ 0 };
		PeerUUID m_PeerUUID;
		std::optional<MessageDetails> m_Message;
	};
}