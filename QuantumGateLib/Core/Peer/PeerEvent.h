// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerTypes.h"
#include "PeerMessageDetails.h"
#include "..\..\API\Extender.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Event final
	{
	public:
		using Type = QuantumGate::API::Extender::PeerEvent::Type;

		Event() noexcept = default;
		Event(const Type type, const PeerLUID pluid, const PeerUUID puuid, const PeerWeakPointer& peerptr) noexcept;
		Event(const Type type, const PeerLUID pluid, const PeerUUID puuid, const PeerWeakPointer& peerptr, MessageDetails&& msg) noexcept;
		Event(const Event& other) noexcept;
		Event(Event&& other) noexcept;
		~Event();
		Event& operator=(const Event&) = delete;
		Event& operator=(Event&& other) noexcept;

		explicit operator bool() const noexcept;

		inline Type GetType() const noexcept { return m_Type; }
		inline PeerLUID GetPeerLUID() const noexcept { return m_PeerLUID; }
		inline const PeerUUID& GetPeerUUID() const noexcept { return m_PeerUUID; }
		inline PeerWeakPointer GetPeerWeakPointer() const noexcept { return m_PeerPointer; }
		const ExtenderUUID* GetExtenderUUID() const noexcept;
		const Buffer* GetMessageData() const noexcept;

	private:
		Type m_Type{ Type::Unknown };
		PeerLUID m_PeerLUID{ 0 };
		PeerUUID m_PeerUUID;
		PeerWeakPointer m_PeerPointer;
		std::optional<MessageDetails> m_Message;
	};
}