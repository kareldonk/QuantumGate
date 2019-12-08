// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Peer.h"
#include "..\Core\Peer\Peer.h"

namespace QuantumGate::API
{
	using Peer_ThS = QuantumGate::Implementation::Core::Peer::Peer_ThS;
	using SharedPeerPtr = std::shared_ptr<Peer_ThS>;

	// Size of peer shared pointer plus one byte to use as a flag
	static constexpr int MinimumPeerStorageSize{ sizeof(SharedPeerPtr) + 1 + sizeof(PeerLUID) };

	inline SharedPeerPtr* GetSharedPeerPtr(void* peer_storage)
	{
		return reinterpret_cast<SharedPeerPtr*>(peer_storage);
	}

	inline const SharedPeerPtr* GetSharedPeerPtr(const void* peer_storage)
	{
		return reinterpret_cast<const SharedPeerPtr*>(peer_storage);
	}

	Peer::Peer() noexcept
	{}

	Peer::Peer(const PeerLUID pluid, const void* peer) noexcept
	{
		static_assert(sizeof(m_PeerStorage) >= MinimumPeerStorageSize,
					  "Storage size is too small; increase size of m_Peer in header file");

		Create(pluid, peer);
	}

	Peer::Peer(const Peer& other) noexcept
	{
		if (other.HasPeer())
		{
			Create(other.GetLUID(), other.GetPeerStorage());
		}
	}

	Peer::Peer(Peer&& other) noexcept :
		m_PeerStorage(other.m_PeerStorage)
	{
		other.SetHasPeer(false);
	}

	Peer::~Peer()
	{
		Reset();
	}

	Peer& Peer::operator=(const Peer& other) noexcept
	{
		Reset();

		if (other.HasPeer())
		{
			Create(other.GetLUID(), other.GetPeerStorage());
		}

		return *this;
	}

	Peer& Peer::operator=(Peer&& other) noexcept
	{
		Reset();

		if (other.HasPeer())
		{
			m_PeerStorage = other.m_PeerStorage;
			other.SetHasPeer(false);
		}

		return *this;
	}

	inline void Peer::Create(const PeerLUID pluid, const void* peer) noexcept
	{
		auto peer_ptr = static_cast<const SharedPeerPtr*>(peer);

		new (GetPeerStorage()) SharedPeerPtr(*peer_ptr);

		SetHasPeer(true);
		SetLUID(pluid);
	}

	inline void* Peer::GetPeerStorage() noexcept
	{
		return const_cast<void*>(const_cast<const Peer*>(this)->GetPeerStorage());
	}

	inline const void* Peer::GetPeerStorage() const noexcept
	{
		return reinterpret_cast<const void*>(reinterpret_cast<const Byte*>(&m_PeerStorage) + 1 + sizeof(PeerLUID));
	}

	inline void Peer::SetHasPeer(const bool flag) noexcept
	{
		reinterpret_cast<Byte*>(&m_PeerStorage)[0] = flag ? Byte{ 1 } : Byte{ 0 };
	}

	inline bool Peer::HasPeer() const noexcept
	{
		return (reinterpret_cast<const Byte*>(&m_PeerStorage)[0] == Byte{ 1 });
	}

	inline void Peer::SetLUID(const PeerLUID pluid) noexcept
	{
		auto pluidptr = reinterpret_cast<PeerLUID*>(reinterpret_cast<Byte*>(&m_PeerStorage) + 1);
		*pluidptr = pluid;
	}

	inline void Peer::Reset() noexcept
	{
		if (HasPeer())
		{
			GetSharedPeerPtr(GetPeerStorage())->~shared_ptr();

			SetHasPeer(false);
		}
	}

	inline PeerLUID Peer::GetLUID() const noexcept
	{
		assert(HasPeer());

		return *reinterpret_cast<const PeerLUID*>(reinterpret_cast<const Byte*>(&m_PeerStorage) + 1);
	}

	Result<Peer::Details> Peer::GetDetails() const noexcept
	{
		assert(HasPeer());

		return (*GetSharedPeerPtr(GetPeerStorage()))->WithSharedLock()->GetPeerDetails();
	}
}