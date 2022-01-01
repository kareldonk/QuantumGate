// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core::Peer
{
	class Manager;
	class Peer;
	class Event;
}

namespace QuantumGate::API
{
	class Export Peer
	{
		friend class QuantumGate::Implementation::Core::Peer::Manager;
		friend class QuantumGate::Implementation::Core::Peer::Peer;
		friend class QuantumGate::Implementation::Core::Peer::Event;

	public:
		using ConnectionType = QuantumGate::Implementation::PeerConnectionType;
		using ConnectionAlgorithms = QuantumGate::Implementation::PeerConnectionAlgorithms;

		struct Details
		{
			PeerUUID PeerUUID;
			ConnectionType ConnectionType{ ConnectionType::Unknown };
			ConnectionAlgorithms ConnectionAlgorithms;
			bool IsAuthenticated{ false };
			bool IsRelayed{ false };
			bool IsUsingGlobalSharedSecret{ false };
			Endpoint LocalEndpoint;
			Endpoint PeerEndpoint;
			std::pair<UInt8, UInt8> PeerProtocolVersion{ 0, 0 };
			UInt64 LocalSessionID{ 0 };
			UInt64 PeerSessionID{ 0 };
			std::chrono::milliseconds ConnectedTime{ 0 };
			Size BytesReceived{ 0 };
			Size BytesSent{ 0 };
			Size ExtendersBytesReceived{ 0 };
			Size ExtendersBytesSent{ 0 };
			bool IsSuspended{ false };
		};

		Peer() noexcept;
		Peer(const Peer& other) noexcept;
		Peer(Peer&& other) noexcept;
		virtual ~Peer();
		Peer& operator=(const Peer& other) noexcept;
		Peer& operator=(Peer&& other) noexcept;

		explicit operator bool() const noexcept;

		[[nodiscard]] inline bool HasPeer() const noexcept;

		[[nodiscard]] PeerLUID GetLUID() const noexcept;
		[[nodiscard]] bool IsConnected() const noexcept;

		Result<PeerUUID> GetUUID() const noexcept;

		Result<ConnectionType> GetConnectionType() const noexcept;
		Result<ConnectionAlgorithms> GetConnectionAlgorithms() const noexcept;
		Result<bool> GetAuthenticated() const noexcept;
		Result<bool> GetRelayed() const noexcept;
		Result<bool> GetUsingGlobalSharedSecret() const noexcept;

		Result<Endpoint> GetLocalEndpoint() const noexcept;
		Result<Endpoint> GetPeerEndpoint() const noexcept;

		Result<std::pair<UInt8, UInt8>> GetPeerProtocolVersion() const noexcept;

		Result<UInt64> GetLocalSessionID() const noexcept;
		Result<UInt64> GetPeerSessionID() const noexcept;

		Result<std::chrono::milliseconds> GetConnectedTime() const noexcept;

		Result<Size> GetBytesReceived() const noexcept;
		Result<Size> GetBytesSent() const noexcept;

		Result<Size> GetExtendersBytesReceived() const noexcept;
		Result<Size> GetExtendersBytesSent() const noexcept;

		Result<bool> GetSuspended() const noexcept;

		Result<Details> GetDetails() const noexcept;

	private:
		Peer(const PeerLUID pluid, const void* peer) noexcept;

		inline void Create(const PeerLUID pluid, const void* peer) noexcept;

		inline void* GetPeerSharedPtrStorage() noexcept;
		inline const void* GetPeerSharedPtrStorage() const noexcept;

		inline std::uintptr_t* GetPeerDataStorage() noexcept;
		inline const std::uintptr_t* GetPeerDataStorage() const noexcept;

		inline void SetHasPeer(const bool flag) noexcept;

		inline void SetLUID(const PeerLUID pluid) noexcept;

		inline void Reset() noexcept;

	private:
		static constexpr int MinimumPeerStorageSize{ 1 + sizeof(PeerLUID) + sizeof(std::uintptr_t) + sizeof(std::shared_ptr<void>) };

		typename std::aligned_storage<MinimumPeerStorageSize>::type m_PeerStorage{ 0 };
	};
}