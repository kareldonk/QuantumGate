// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerEvent.h"
#include "..\Core\Peer\PeerEvent.h"

namespace QuantumGate::API
{
	PeerEvent::PeerEvent(QuantumGate::Implementation::Core::Peer::Event&& event) noexcept
	{
		m_PeerEvent = new QuantumGate::Implementation::Core::Peer::Event(std::move(event));
	}

	PeerEvent::PeerEvent(PeerEvent&& other) noexcept
	{
		assert(other.m_PeerEvent != nullptr);

		m_PeerEvent = other.m_PeerEvent;
		other.m_PeerEvent = nullptr;
	}

	PeerEvent::~PeerEvent()
	{
		Reset();
	}

	void PeerEvent::Reset() noexcept
	{
		if (m_PeerEvent != nullptr)
		{
			delete m_PeerEvent;
			m_PeerEvent = nullptr;
		}
	}

	PeerEvent& PeerEvent::operator=(PeerEvent&& other) noexcept
	{
		assert(other.m_PeerEvent != nullptr);

		Reset();

		m_PeerEvent = other.m_PeerEvent;
		other.m_PeerEvent = nullptr;

		return *this;
	}

	PeerEvent::operator bool() const noexcept
	{
		assert(m_PeerEvent != nullptr);

		return m_PeerEvent->operator bool();
	}
	
	const PeerEventType PeerEvent::GetType() const noexcept
	{
		assert(m_PeerEvent != nullptr);

		return m_PeerEvent->GetType();
	}
	
	const PeerLUID PeerEvent::GetPeerLUID() const noexcept
	{
		assert(m_PeerEvent != nullptr);

		return m_PeerEvent->GetPeerLUID();
	}

	const PeerUUID& PeerEvent::GetPeerUUID() const noexcept
	{
		assert(m_PeerEvent != nullptr);

		return m_PeerEvent->GetPeerUUID();
	}
	
	const Buffer* PeerEvent::GetMessageData() const noexcept
	{
		assert(m_PeerEvent != nullptr);

		return m_PeerEvent->GetMessageData();
	}
}