// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Peer.h"
#include "..\Core\Peer\Peer.h"

namespace QuantumGate::API
{
	using Peer_ThS = QuantumGate::Implementation::Core::Peer::Peer_ThS;
	using PeerSharedPtr = std::shared_ptr<Peer_ThS>;

	using PeerData_ThS = QuantumGate::Implementation::Core::Peer::Data_ThS;

	inline static PeerSharedPtr* PeerSharedPtrCast(void* peer_storage) noexcept
	{
		return static_cast<PeerSharedPtr*>(peer_storage);
	}

	inline static const PeerSharedPtr* PeerSharedPtrCast(const void* peer_storage) noexcept
	{
		return static_cast<const PeerSharedPtr*>(peer_storage);
	}

	inline static const PeerData_ThS* PeerDataCast(const std::uintptr_t* peer_data_ptr) noexcept
	{
		return static_cast<const PeerData_ThS*>(reinterpret_cast<const void*>(*peer_data_ptr));
	}

	inline static bool IsPeerConnected(const PeerData_ThS::SharedLockedConstType& peer_data) noexcept
	{
		return (peer_data->Status == QuantumGate::Implementation::Core::Peer::Status::Ready);
	}

	template<typename T, typename F>
	inline static Result<T> GetPeerDataItem(const PeerData_ThS* peer_data_ptr, F&& func) noexcept
	{
		auto peer_data = peer_data_ptr->WithSharedLock();
		if (IsPeerConnected(peer_data))
		{
			return func(peer_data);
		}

		return ResultCode::PeerNotReady;
	}

	Peer::Peer() noexcept
	{}

	Peer::Peer(const PeerLUID pluid, const void* peer) noexcept
	{
		static_assert(sizeof(m_PeerStorage) >= MinimumPeerStorageSize,
					  "Storage size is too small; increase size of m_PeerStorage in header file");

		Create(pluid, peer);
	}

	Peer::Peer(const Peer& other) noexcept
	{
		if (other.HasPeer())
		{
			Create(other.GetLUID(), other.GetPeerSharedPtrStorage());
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
			Create(other.GetLUID(), other.GetPeerSharedPtrStorage());
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

	Peer::operator bool() const noexcept
	{
		return HasPeer();
	}

	inline void Peer::Create(const PeerLUID pluid, const void* peer) noexcept
	{
		auto peer_ptr = PeerSharedPtrCast(peer);

		// Our own copy of shared_ptr keeps the peer alive
		new (GetPeerSharedPtrStorage()) PeerSharedPtr(*peer_ptr);

		// Also store a pointer to peer data for quicker access
		auto peer_data_ptr = &(*PeerSharedPtrCast(GetPeerSharedPtrStorage()))->WithSharedLock()->GetPeerData();
		auto peer_data_storage = GetPeerDataStorage();
		*peer_data_storage = reinterpret_cast<std::uintptr_t>(peer_data_ptr);

		SetLUID(pluid);
		SetHasPeer(true);
	}

	inline void* Peer::GetPeerSharedPtrStorage() noexcept
	{
		return const_cast<void*>(const_cast<const Peer*>(this)->GetPeerSharedPtrStorage());
	}

	inline const void* Peer::GetPeerSharedPtrStorage() const noexcept
	{
		return reinterpret_cast<const void*>(reinterpret_cast<const Byte*>(&m_PeerStorage) + 1 + sizeof(PeerLUID) + sizeof(std::uintptr_t));
	}

	inline std::uintptr_t* Peer::GetPeerDataStorage() noexcept
	{
		return const_cast<std::uintptr_t*>(const_cast<const Peer*>(this)->GetPeerDataStorage());
	}

	inline const std::uintptr_t* Peer::GetPeerDataStorage() const noexcept
	{
		return reinterpret_cast<const std::uintptr_t*>(reinterpret_cast<const Byte*>(&m_PeerStorage) + 1 + sizeof(PeerLUID));
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
			PeerSharedPtrCast(GetPeerSharedPtrStorage())->~shared_ptr();

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
		return PeerDataCast(GetPeerDataStorage())->WithSharedLock()->GetDetails();
	}

	bool Peer::IsConnected() const noexcept
	{
		assert(HasPeer());
		auto peer_data = PeerDataCast(GetPeerDataStorage())->WithSharedLock();
		return IsPeerConnected(peer_data);
	}

	Result<PeerUUID> Peer::GetUUID() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<UUID>(PeerDataCast(GetPeerDataStorage()), [](auto& peer_data) { return peer_data->PeerUUID; });
	}

	Result<API::Peer::ConnectionType> Peer::GetConnectionType() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<API::Peer::ConnectionType>(PeerDataCast(GetPeerDataStorage()),
														  [](auto& peer_data) { return peer_data->Type; });
	}

	Result<bool> Peer::IsAuthenticated() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<bool>(PeerDataCast(GetPeerDataStorage()),
									 [](auto& peer_data) { return peer_data->IsAuthenticated; });
	}

	Result<bool> Peer::IsRelayed() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<bool>(PeerDataCast(GetPeerDataStorage()), [](auto& peer_data) { return peer_data->IsRelayed; });
	}

	Result<bool> Peer::IsUsingGlobalSharedSecret() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<bool>(PeerDataCast(GetPeerDataStorage()),
									 [](auto& peer_data) { return peer_data->IsUsingGlobalSharedSecret; });
	}

	Result<IPEndpoint> Peer::GetLocalIPEndpoint() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<IPEndpoint>(PeerDataCast(GetPeerDataStorage()),
										   [](auto& peer_data) { return peer_data->Cached.LocalEndpoint; });
	}

	Result<IPEndpoint> Peer::GetPeerIPEndpoint() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<IPEndpoint>(PeerDataCast(GetPeerDataStorage()),
										   [](auto& peer_data) { return peer_data->Cached.PeerEndpoint; });
	}

	Result<std::pair<UInt8, UInt8>> Peer::GetPeerProtocolVersion() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<std::pair<UInt8, UInt8>>(PeerDataCast(GetPeerDataStorage()),
														[](auto& peer_data) { return peer_data->PeerProtocolVersion; });
	}

	Result<UInt64> Peer::GetLocalSessionID() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<UInt64>(PeerDataCast(GetPeerDataStorage()),
									   [](auto& peer_data) { return peer_data->LocalSessionID; });
	}

	Result<UInt64> Peer::GetPeerSessionID() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<UInt64>(PeerDataCast(GetPeerDataStorage()),
									   [](auto& peer_data) { return peer_data->PeerSessionID; });
	}

	Result<std::chrono::milliseconds> Peer::GetConnectedTime() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<std::chrono::milliseconds>(PeerDataCast(GetPeerDataStorage()),
														  [](auto& peer_data) { return peer_data->GetConnectedTime(); });
	}

	Result<Size> Peer::GetBytesReceived() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<Size>(PeerDataCast(GetPeerDataStorage()),
									 [](auto& peer_data) { return peer_data->Cached.BytesReceived; });
	}

	Result<Size> Peer::GetBytesSent() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<Size>(PeerDataCast(GetPeerDataStorage()),
									 [](auto& peer_data) { return peer_data->Cached.BytesSent; });
	}

	Result<Size> Peer::GetExtendersBytesReceived() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<Size>(PeerDataCast(GetPeerDataStorage()),
									 [](auto& peer_data) { return peer_data->ExtendersBytesReceived; });
	}

	Result<Size> Peer::GetExtendersBytesSent() const noexcept
	{
		assert(HasPeer());
		return GetPeerDataItem<Size>(PeerDataCast(GetPeerDataStorage()),
									 [](auto& peer_data) { return peer_data->ExtendersBytesSent; });
	}
}