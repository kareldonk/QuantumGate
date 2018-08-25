// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerEvent.h"

namespace QuantumGate::Implementation::Core::Peer
{
	Event::Event(PeerEventType type, PeerLUID pluid, PeerUUID puuid) noexcept :
		m_Type(type), m_PeerLUID(pluid), m_PeerUUID(puuid)
	{}

	Event::Event(PeerEventType type, PeerLUID pluid, PeerUUID puuid, MessageDetails&& msg) noexcept :
		m_Type(type), m_PeerLUID(pluid), m_PeerUUID(puuid), m_Message(std::move(msg))
	{}

	Event::Event(const Event& other) noexcept
	{
		// Copy should only be used for events with no message
		assert(other.m_Type != PeerEventType::Message);

		m_Type = other.m_Type;
		m_PeerLUID = other.m_PeerLUID;
		m_PeerUUID = other.m_PeerUUID;
	}

	Event::Event(Event&& other) noexcept
	{
		*this = std::move(other);
	}

	Event& Event::operator=(Event&& other) noexcept
	{
		m_Type = std::exchange(other.m_Type, PeerEventType::Unknown);
		m_PeerLUID = std::exchange(other.m_PeerLUID, 0);
		m_PeerUUID = std::move(other.m_PeerUUID);
		m_Message = std::move(other.m_Message);

		return *this;
	}

	void Event::swap(Event& other) noexcept
	{
		m_Type = std::exchange(other.m_Type, m_Type);
		m_PeerLUID = std::exchange(other.m_PeerLUID, m_PeerLUID);
		m_PeerUUID = std::exchange(other.m_PeerUUID, m_PeerUUID);
		m_Message.swap(other.m_Message);
	}

	Event::operator bool() const noexcept
	{
		return (m_Type != PeerEventType::Unknown);
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
