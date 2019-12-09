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
		struct Details
		{
			PeerUUID PeerUUID;
			PeerConnectionType ConnectionType{ PeerConnectionType::Unknown };
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

		[[nodiscard]] PeerLUID GetLUID() const noexcept;
		Result<Details> GetDetails() const noexcept;

	private:
		Peer(const PeerLUID pluid, const void* peer) noexcept;

		inline void Create(const PeerLUID pluid, const void* peer) noexcept;

		inline void* GetPeerStorage() noexcept;
		inline const void* GetPeerStorage() const noexcept;

		[[nodiscard]] inline bool HasPeer() const noexcept;
		inline void SetHasPeer(const bool flag) noexcept;

		inline void SetLUID(const PeerLUID pluid) noexcept;

		inline void Reset() noexcept;

	private:
		static constexpr int MinimumPeerStorageSize{ sizeof(std::shared_ptr<void>) + 1 + sizeof(PeerLUID) };

		typename std::aligned_storage<MinimumPeerStorageSize>::type m_PeerStorage{ 0 };
	};
}