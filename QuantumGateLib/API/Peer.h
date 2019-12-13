// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core::Peer
{
	class Manager;
}

namespace QuantumGate::API
{
	class Export Peer
	{
		friend class QuantumGate::Implementation::Core::Peer::Manager;

	public:
		using ConnectionType = QuantumGate::Implementation::PeerConnectionType;

		struct Details
		{
			PeerUUID PeerUUID;
			ConnectionType ConnectionType{ ConnectionType::Unknown };
			bool IsAuthenticated{ false };
			bool IsRelayed{ false };
			bool IsUsingGlobalSharedSecret{ false };
			IPEndpoint LocalIPEndpoint;
			IPEndpoint PeerIPEndpoint;
			std::pair<UInt8, UInt8> PeerProtocolVersion{ 0, 0 };
			UInt64 LocalSessionID{ 0 };
			UInt64 PeerSessionID{ 0 };
			std::chrono::milliseconds ConnectedTime{ 0 };
			Size BytesReceived{ 0 };
			Size BytesSent{ 0 };
			Size ExtendersBytesReceived{ 0 };
			Size ExtendersBytesSent{ 0 };
		};

		Peer() noexcept;
		Peer(const Peer&) noexcept;
		Peer(Peer&&) noexcept;
		virtual ~Peer();
		Peer& operator=(const Peer&) noexcept;
		Peer& operator=(Peer&&) noexcept;

		explicit operator bool() const noexcept;

		[[nodiscard]] inline bool HasPeer() const noexcept;

		[[nodiscard]] PeerLUID GetLUID() const noexcept;
		[[nodiscard]] bool IsConnected() const noexcept;

		Result<PeerUUID> GetUUID() const noexcept;

		Result<ConnectionType> GetConnectionType() const noexcept;
		Result<bool> IsAuthenticated() const noexcept;
		Result<bool> IsRelayed() const noexcept;
		Result<bool> IsUsingGlobalSharedSecret() const noexcept;

		Result<IPEndpoint> GetLocalIPEndpoint() const noexcept;
		Result<IPEndpoint> GetPeerIPEndpoint() const noexcept;

		Result<std::pair<UInt8, UInt8>> GetPeerProtocolVersion() const noexcept;

		Result<UInt64> GetLocalSessionID() const noexcept;
		Result<UInt64> GetPeerSessionID() const noexcept;

		Result<std::chrono::milliseconds> GetConnectedTime() const noexcept;

		Result<Size> GetBytesReceived() const noexcept;
		Result<Size> GetBytesSent() const noexcept;

		Result<Size> GetExtendersBytesReceived() const noexcept;
		Result<Size> GetExtendersBytesSent() const noexcept;

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