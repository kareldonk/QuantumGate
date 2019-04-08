// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

// Include the QuantumGate main header with API definitions
#include <QuantumGate.h>

#include <atomic>

class HandshakeExtender final : public QuantumGate::Extender
{
	enum class MessageType : std::uint16_t { Unknown, PublicKey, Ready, Chat };

#pragma pack(push, 1) // Disable padding bytes
	struct PublicKeyMessage final
	{
		MessageType Type{ MessageType::PublicKey };
		std::byte PublicKey[32]{ std::byte{ 0 } };
	};

	struct ReadyMessage final
	{
		MessageType Type{ MessageType::Ready };
	};

	struct ChatMessage final
	{
		MessageType Type{ MessageType::Chat };
		std::byte Mac[16]{ std::byte{ 0 } };
		std::byte Nonce[24]{ std::byte{ 0 } };
	};
#pragma pack(pop)

	struct Peer final
	{
		enum class Status { Unknown, Connected, PubKeySent, Ready, Exception };

		QuantumGate::PeerLUID LUID{ 0 };
		QuantumGate::PeerConnectionType ConnectionType{ QuantumGate::PeerConnectionType::Unknown };
		Status Status{ Status::Unknown };
		std::chrono::time_point<std::chrono::steady_clock> ConnectedSteadyTime;

		// Since multiple threads may access the peer
		// data we'll need synchronization through a mutex
		std::shared_mutex Mutex;

		// Our local key-pair for this peer
		std::array<std::byte, 32> PublicKey{ std::byte{ 0 } };
		std::array<std::byte, 32> PrivateKey{ std::byte{ 0 } };

		// The peer's public key for DH key exchange
		std::array<std::byte, 32> PeerPublicKey{ std::byte{ 0 } };

		// The shared secret for this peer
		std::array<std::byte, 32> SharedSecretKey{ std::byte{ 0 } };
	};

public:
	HandshakeExtender();
	virtual ~HandshakeExtender();

	bool BroadcastToConnectedPeers(const std::wstring& msg);

protected:
	bool OnStartup();
	void OnPostStartup();
	void OnPreShutdown();
	void OnShutdown();
	void OnPeerEvent(QuantumGate::PeerEvent&& event);
	const std::pair<bool, bool> OnPeerMessage(QuantumGate::PeerEvent&& event);

private:
	bool GetRandomBytes(std::byte* buffer, const std::size_t buffer_len);

	bool SendPublicKey(Peer& peer);
	bool SendReady(Peer& peer);

	bool SendChatMessage(const Peer& peer, const std::wstring& msg);
	bool DisplayChatMessage(const Peer& peer, const QuantumGate::Buffer* msgdata);

	bool GenerateDHKeyPair(Peer& peer);
	bool GenerateSharedKey(Peer& peer);

	static void MainThreadFunction(HandshakeExtender* extender);

	bool ProcessPublicKeyMessage(const QuantumGate::PeerLUID pluid, const QuantumGate::Buffer* msgdata);
	bool ProcessReadyMessage(const QuantumGate::PeerLUID pluid, const QuantumGate::Buffer* msgdata);
	bool ProcessChatMessage(const QuantumGate::PeerLUID pluid, const QuantumGate::Buffer* msgdata);

	// Since data sent by this extender is encrypted (random looking)
	// we let QuantumGate know that it shouldn't (try to) compress
	// data that we send because it won't compress well. This variable
	// is used whenever we use the SendMessageTo function.
	static constexpr bool NoCompression{ false };

	static constexpr std::chrono::seconds MaxHandshakeDuration{ 10 };

private:
	std::thread m_MainThread;
	std::atomic_bool m_ShutdownEvent{ false };

	// For this extender we'll maintain a local list of peers, and because
	// the list of peers will be accessed through multiple threads we'll
	// need a mutex for synchronization
	std::unordered_map<QuantumGate::PeerLUID, std::unique_ptr<Peer>> m_Peers;
	std::shared_mutex m_PeersMutex;
};

