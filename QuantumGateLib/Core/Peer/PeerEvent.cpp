// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerEvent.h"
#include "Peer.h"

namespace QuantumGate::Implementation::Core::Peer
{
	Event::Event(const Type type, const PeerLUID pluid, const PeerUUID puuid, const PeerWeakPointer& peerptr) noexcept :
		m_Type(type), m_PeerLUID(pluid), m_PeerUUID(puuid), m_PeerPointer(peerptr)
	{}

	Event::Event(const Type type, const PeerLUID pluid, const PeerUUID puuid, const PeerWeakPointer& peerptr, MessageDetails&& msg) noexcept :
		m_Type(type), m_PeerLUID(pluid), m_PeerUUID(puuid), m_PeerPointer(peerptr), m_Message(std::move(msg))
	{}

	Event::Event(const Event& other) noexcept
	{
		// Copy should only be used for events with no message
		assert(other.m_Type != Type::Message);

		m_Type = other.m_Type;
		m_PeerLUID = other.m_PeerLUID;
		m_PeerUUID = other.m_PeerUUID;
		m_PeerPointer = other.m_PeerPointer;
	}

	Event::Event(Event&& other) noexcept
	{
		*this = std::move(other);
	}

	Event::~Event()
	{}

	Event& Event::operator=(Event&& other) noexcept
	{
		// Check for same object
		if (this == &other) return *this;

		m_Type = std::exchange(other.m_Type, Type::Unknown);
		m_PeerLUID = std::exchange(other.m_PeerLUID, 0);
		m_PeerUUID = std::move(other.m_PeerUUID);
		m_PeerPointer = std::move(other.m_PeerPointer);
		m_Message = std::move(other.m_Message);

		return *this;
	}

	Event::operator bool() const noexcept
	{
		return (m_Type != Type::Unknown);
	}

	Result<API::Peer> Event::GetPeer() const noexcept
	{
		const auto peerptr = m_PeerPointer.lock();
		if (peerptr)
		{
			return API::Peer(m_PeerLUID, &peerptr);
		}

		return ResultCode::PeerNotFound;
	}

	const ExtenderUUID* Event::GetExtenderUUID() const noexcept
	{
		assert(m_Message);

		if (m_Message)
		{
			return &m_Message->GetExtenderUUID();
		}

		return nullptr;
	}

	const Buffer* Event::GetMessageData() const noexcept
	{
		assert(m_Message);

		if (m_Message)
		{
			return &m_Message->GetMessageData();
		}

		return nullptr;
	}
}
