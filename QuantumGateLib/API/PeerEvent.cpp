// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerEvent.h"
#include "..\Core\Peer\PeerEvent.h"

namespace QuantumGate::API
{
	// Size of event object plus one byte to use as a flag
	static constexpr int MinimumPeerEventStorageSize{ sizeof(QuantumGate::Implementation::Core::Peer::Event) + 1 };

	PeerEvent::PeerEvent(QuantumGate::Implementation::Core::Peer::Event&& event) noexcept
	{
#ifdef _DEBUG
		static_assert(sizeof(QuantumGate::Implementation::Core::Peer::Event) == 104,
					  "Size of event object changed; check size of m_PeerEvent and update the size here");
#else
		static_assert(sizeof(QuantumGate::Implementation::Core::Peer::Event) == 96,
					  "Size of event object changed; check size of m_PeerEvent and update the size here");
#endif
		static_assert(sizeof(m_PeerEvent) >= MinimumPeerEventStorageSize,
					  "Storage size is too small; increase size of m_PeerEvent in header file");

		new (GetEvent()) QuantumGate::Implementation::Core::Peer::Event(std::move(event));
		SetHasEvent(true);
	}

	PeerEvent::PeerEvent(PeerEvent&& other) noexcept :
		m_PeerEvent(other.m_PeerEvent)
	{
		other.SetHasEvent(false);
	}

	PeerEvent::~PeerEvent()
	{
		Reset();
	}

	inline void PeerEvent::SetHasEvent(const bool flag) noexcept
	{
		reinterpret_cast<Byte*>(&m_PeerEvent)[0] = flag ? Byte{ 1 } : Byte{ 0 };
	}

	inline const bool PeerEvent::HasEvent() const noexcept
	{
		return (reinterpret_cast<const Byte*>(&m_PeerEvent)[0] == Byte{ 1 });
	}

	inline QuantumGate::Implementation::Core::Peer::Event* PeerEvent::GetEvent() noexcept
	{
		return const_cast<QuantumGate::Implementation::Core::Peer::Event*>(const_cast<const PeerEvent*>(this)->GetEvent());
	}

	inline const QuantumGate::Implementation::Core::Peer::Event* PeerEvent::GetEvent() const noexcept
	{
		return reinterpret_cast<const QuantumGate::Implementation::Core::Peer::Event*>(
			reinterpret_cast<const Byte*>(&m_PeerEvent) + 1);
	}

	inline void PeerEvent::Reset() noexcept
	{
		if (HasEvent())
		{
			if constexpr (!std::is_trivially_destructible_v<QuantumGate::Implementation::Core::Peer::Event>)
			{
				GetEvent()->~Event();
			}

			SetHasEvent(false);
		}
	}

	PeerEvent& PeerEvent::operator=(PeerEvent&& other) noexcept
	{
		Reset();

		if (other.HasEvent())
		{
			m_PeerEvent = other.m_PeerEvent;
			other.SetHasEvent(false);
		}

		return *this;
	}

	PeerEvent::operator bool() const noexcept
	{
		assert(HasEvent());

		return GetEvent()->operator bool();
	}

	const PeerEventType PeerEvent::GetType() const noexcept
	{
		assert(HasEvent());

		return GetEvent()->GetType();
	}

	const PeerLUID PeerEvent::GetPeerLUID() const noexcept
	{
		assert(HasEvent());

		return GetEvent()->GetPeerLUID();
	}

	const PeerUUID& PeerEvent::GetPeerUUID() const noexcept
	{
		assert(HasEvent());

		return GetEvent()->GetPeerUUID();
	}

	const Buffer* PeerEvent::GetMessageData() const noexcept
	{
		assert(HasEvent());

		return GetEvent()->GetMessageData();
	}
}