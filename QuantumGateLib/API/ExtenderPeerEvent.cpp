// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Extender.h"
#include "..\Core\Peer\PeerEvent.h"

namespace QuantumGate::API
{
#ifdef _DEBUG
#if !defined(_WIN64)
	static constexpr int PeerEventSize{ 112 };
#else
	static constexpr int PeerEventSize{ 152 };
#endif
#else
#if !defined(_WIN64)
	static constexpr int PeerEventSize{ 104 };
#else
	static constexpr int PeerEventSize{ 144 };
#endif
#endif

	static_assert(sizeof(QuantumGate::Implementation::Core::Peer::Event) == PeerEventSize,
				  "Size of event object changed; check size of m_PeerEvent (should be large enough) and update the PeerEventSize");
	
	// Size of event object plus one byte to use as a flag
	static constexpr int MinimumPeerEventStorageSize{ sizeof(QuantumGate::Implementation::Core::Peer::Event) + 1 };

	Extender::PeerEvent::PeerEvent(QuantumGate::Implementation::Core::Peer::Event&& event) noexcept
	{
		static_assert(sizeof(m_PeerEvent) >= MinimumPeerEventStorageSize,
					  "Storage size is too small; increase size of m_PeerEvent in header file");

		new (GetEvent()) QuantumGate::Implementation::Core::Peer::Event(std::move(event));
		SetHasEvent(true);
	}

	Extender::PeerEvent::PeerEvent(PeerEvent&& other) noexcept :
		m_PeerEvent(other.m_PeerEvent)
	{
		other.SetHasEvent(false);
	}

	Extender::PeerEvent::~PeerEvent()
	{
		Reset();
	}

	inline void Extender::PeerEvent::SetHasEvent(const bool flag) noexcept
	{
		reinterpret_cast<Byte*>(&m_PeerEvent)[0] = flag ? Byte{ 1 } : Byte{ 0 };
	}

	inline bool Extender::PeerEvent::HasEvent() const noexcept
	{
		return (reinterpret_cast<const Byte*>(&m_PeerEvent)[0] == Byte{ 1 });
	}

	inline QuantumGate::Implementation::Core::Peer::Event* Extender::PeerEvent::GetEvent() noexcept
	{
		return const_cast<QuantumGate::Implementation::Core::Peer::Event*>(const_cast<const PeerEvent*>(this)->GetEvent());
	}

	inline const QuantumGate::Implementation::Core::Peer::Event* Extender::PeerEvent::GetEvent() const noexcept
	{
		return reinterpret_cast<const QuantumGate::Implementation::Core::Peer::Event*>(
			reinterpret_cast<const Byte*>(&m_PeerEvent) + 1);
	}

	inline void Extender::PeerEvent::Reset() noexcept
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

	Extender::PeerEvent& Extender::PeerEvent::operator=(PeerEvent&& other) noexcept
	{
		Reset();

		if (other.HasEvent())
		{
			m_PeerEvent = other.m_PeerEvent;
			other.SetHasEvent(false);
		}

		return *this;
	}

	Extender::PeerEvent::operator bool() const noexcept
	{
		if (HasEvent()) return GetEvent()->operator bool();

		return false;
	}

	Extender::PeerEvent::Type Extender::PeerEvent::GetType() const noexcept
	{
		assert(HasEvent());

		return GetEvent()->GetType();
	}

	PeerLUID Extender::PeerEvent::GetPeerLUID() const noexcept
	{
		assert(HasEvent());

		return GetEvent()->GetPeerLUID();
	}

	const PeerUUID& Extender::PeerEvent::GetPeerUUID() const noexcept
	{
		assert(HasEvent());

		return GetEvent()->GetPeerUUID();
	}

	const Buffer* Extender::PeerEvent::GetMessageData() const noexcept
	{
		assert(HasEvent());

		return GetEvent()->GetMessageData();
	}
}