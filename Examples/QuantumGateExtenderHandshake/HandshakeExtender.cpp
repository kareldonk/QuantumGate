// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "HandshakeExtender.h"

#include <iostream>

// We use monocypher for the Diffie-Hellman key
// exchange and encryption for this extender
// https://monocypher.org
extern "C"
{
#include <monocypher.h>
#include <monocypher.c>
}

HandshakeExtender::HandshakeExtender() :
	QuantumGate::Extender(QuantumGate::ExtenderUUID(L"3ddd4019-e6d1-09a5-2ec7-9c51af0304cb"),
						  QuantumGate::String(L"QuantumGate Handshake Extender"))
{
	// Add the callback functions for this extender; this can also be done
	// in another function instead of the constructor, as long as you set the callbacks
	// before adding the extender to the local instance
	if (!SetStartupCallback(QuantumGate::MakeCallback(this, &HandshakeExtender::OnStartup)) ||
		!SetPostStartupCallback(QuantumGate::MakeCallback(this, &HandshakeExtender::OnPostStartup)) ||
		!SetPreShutdownCallback(QuantumGate::MakeCallback(this, &HandshakeExtender::OnPreShutdown)) ||
		!SetShutdownCallback(QuantumGate::MakeCallback(this, &HandshakeExtender::OnShutdown)) ||
		!SetPeerEventCallback(QuantumGate::MakeCallback(this, &HandshakeExtender::OnPeerEvent)) ||
		!SetPeerMessageCallback(QuantumGate::MakeCallback(this, &HandshakeExtender::OnPeerMessage)))
	{
		throw std::exception("Failed to set one or more extender callbacks");
	}
}

HandshakeExtender::~HandshakeExtender()
{}

bool HandshakeExtender::OnStartup()
{
	// This function gets called by the QuantumGate instance to notify
	// an extender to initialize and startup

	std::wcout << L"HandshakeExtender::OnStartup() called...\r\n";

	try
	{
		// Start the main thread
		m_ShutdownEvent = false;
		m_MainThread = std::thread(HandshakeExtender::MainThreadFunction, this);
	}
	catch (...)
	{
		std::wcout << L"HandshakeExtender failed to start main thread...\r\n";
		return false;
	}

	// Return true if initialization was successful, otherwise return false and
	// QuantumGate won't be sending this extender any notifications
	return true;
}

void HandshakeExtender::OnPostStartup()
{
	// This function gets called by the QuantumGate instance to notify
	// an extender of the fact that the startup procedure for this extender has
	// been completed successfully and the extender can now interact with the instance

	std::wcout << L"HandshakeExtender::OnPostStartup() called...\r\n";
}

void HandshakeExtender::OnPreShutdown()
{
	// This callback function gets called by the QuantumGate instance to notify
	// an extender that the shut down procedure has been initiated for this extender.
	// The extender should stop all activity and prepare for deinitialization before
	// returning from this function.

	std::wcout << L"HandshakeExtender::OnPreShutdown() called...\r\n";

	// Set the shutdown event so that the
	// main thread will exit
	m_ShutdownEvent = true;

	// Wait for the main thread to exit
	m_MainThread.join();
}

void HandshakeExtender::OnShutdown()
{
	// This callback function gets called by the QuantumGate instance to notify an
	// extender that it has been shut down completely and should now deinitialize and
	// free resources

	std::wcout << L"HandshakeExtender::OnShutdown() called...\r\n";
}

void HandshakeExtender::OnPeerEvent(QuantumGate::Extender::PeerEvent&& event)
{
	// This callback function gets called by the QuantumGate instance to notify an
	// extender of a peer event

	std::wstring ev(L"Unknown");

	if (event.GetType() == QuantumGate::Extender::PeerEvent::Type::Connected) ev = L"Connect";
	else if (event.GetType() == QuantumGate::Extender::PeerEvent::Type::Disconnected) ev = L"Disconnect";

	std::wcout << L"HandshakeExtender::OnPeerEvent() got peer event '" << ev <<
		L"' for peer LUID " << event.GetPeerLUID() << L"\r\n";

	if (event.GetType() == QuantumGate::Extender::PeerEvent::Type::Connected)
	{
		// Add connected peer
		GetPeer(event.GetPeerLUID()).Succeeded([&](auto& peer_result)
		{
			if (const auto result = peer_result->GetDetails(); result.Succeeded())
			{
				auto peer = std::make_unique<Peer>();
				peer->LUID = event.GetPeerLUID();
				peer->ConnectionType = result->ConnectionType;
				peer->ConnectedSteadyTime = std::chrono::steady_clock::now();

				if (GenerateDHKeyPair(*peer))
				{
					peer->Status = Peer::Status::Connected;
				}
				else peer->Status = Peer::Status::Exception;

				std::unique_lock<std::shared_mutex> lock(m_PeersMutex);
				m_Peers.insert({ event.GetPeerLUID(), std::move(peer) });
			}
		});
	}
	else if (event.GetType() == QuantumGate::Extender::PeerEvent::Type::Disconnected)
	{
		// Remove disconnected peer
		std::unique_lock<std::shared_mutex> lock(m_PeersMutex);

		const auto it = m_Peers.find(event.GetPeerLUID());
		if (it != m_Peers.end())
		{
			auto& peer = *it->second;

			std::unique_lock<std::shared_mutex> lock(peer.Mutex);

			// The NSA won't be needing these
			crypto_wipe(peer.PublicKey.data(), peer.PublicKey.size());
			crypto_wipe(peer.PrivateKey.data(), peer.PrivateKey.size());
			crypto_wipe(peer.PeerPublicKey.data(), peer.PeerPublicKey.size());
			crypto_wipe(peer.SharedSecretKey.data(), peer.SharedSecretKey.size());

			m_Peers.erase(it);
		}
	}
}

QuantumGate::Extender::PeerEvent::Result HandshakeExtender::OnPeerMessage(QuantumGate::Extender::PeerEvent&& event)
{
	// This callback function gets called by the QuantumGate instance to notify an
	// extender of a peer message event

	std::wcout << L"HandshakeExtender::OnPeerMessage() called...\r\n";

	QuantumGate::Extender::PeerEvent::Result result;

	// result.Handled should be true if message was recognized, otherwise false
	// result.Success should be true if message was handled successfully, otherwise false

	if (const auto msgdata = event.GetMessageData(); msgdata != nullptr)
	{
		if (msgdata->GetSize() >= sizeof(MessageType))
		{
			const auto msgtype = *(reinterpret_cast<const MessageType*>(msgdata->GetBytes()));
			switch (msgtype)
			{
				case MessageType::PublicKey:
				{
					result.Handled = true;
					result.Success = ProcessPublicKeyMessage(event.GetPeerLUID(), msgdata);
					break;
				}
				case MessageType::Ready:
				{
					result.Handled = true;
					result.Success = ProcessReadyMessage(event.GetPeerLUID(), msgdata);
					break;
				}
				case MessageType::Chat:
				{
					result.Handled = true;
					result.Success = ProcessChatMessage(event.GetPeerLUID(), msgdata);
					break;
				}
				default:
				{
					std::wcout << L"HandshakeExtender received unrecognized message from peer " <<
						event.GetPeerLUID() << L"\r\n";
					break;
				}
			}
		}
	}

	// If we return false for Handled and Success too often,
	// QuantumGate will disconnect the misbehaving peer eventually
	// as its reputation declines
	return result;
}

bool HandshakeExtender::ProcessPublicKeyMessage(const QuantumGate::PeerLUID pluid, const QuantumGate::Buffer* msgdata)
{
	if (msgdata->GetSize() == sizeof(PublicKeyMessage))
	{
		std::wcout << L"HandshakeExtender received public key from peer " << pluid << L"\r\n";

		const auto pubmsg = reinterpret_cast<const PublicKeyMessage*>(msgdata->GetBytes());

		std::shared_lock<std::shared_mutex> lock(m_PeersMutex);

		const auto it = m_Peers.find(pluid);
		if (it != m_Peers.end())
		{
			auto& peer = *it->second;

			std::unique_lock<std::shared_mutex> lock(peer.Mutex);

			if (peer.Status == Peer::Status::Connected &&
				peer.ConnectionType == QuantumGate::PeerConnectionType::Outbound)
			{
				std::memcpy(peer.PeerPublicKey.data(), &pubmsg->PublicKey, peer.PeerPublicKey.size());

				if (GenerateSharedKey(peer))
				{
					SendPublicKey(peer);

					peer.Status = Peer::Status::PubKeySent;
					return true;
				}
			}
			else if (peer.Status == Peer::Status::PubKeySent &&
					 peer.ConnectionType == QuantumGate::PeerConnectionType::Inbound)
			{
				std::memcpy(peer.PeerPublicKey.data(), &pubmsg->PublicKey, peer.PeerPublicKey.size());

				if (GenerateSharedKey(peer))
				{
					if (SendReady(peer))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool HandshakeExtender::ProcessReadyMessage(const QuantumGate::PeerLUID pluid, const QuantumGate::Buffer* msgdata)
{
	if (msgdata->GetSize() == sizeof(ReadyMessage))
	{
		std::wcout << L"HandshakeExtender received ready from peer " << pluid << L"\r\n";

		std::shared_lock<std::shared_mutex> lock(m_PeersMutex);

		const auto it = m_Peers.find(pluid);
		if (it != m_Peers.end())
		{
			auto& peer = *it->second;

			std::unique_lock<std::shared_mutex> lock(peer.Mutex);

			if (peer.Status == Peer::Status::PubKeySent &&
				peer.ConnectionType == QuantumGate::PeerConnectionType::Outbound)
			{
				if (SendReady(peer))
				{
					peer.Status = Peer::Status::Ready;
				}
			}
			else if (peer.Status == Peer::Status::PubKeySent &&
					 peer.ConnectionType == QuantumGate::PeerConnectionType::Inbound)
			{
				peer.Status = Peer::Status::Ready;
			}

			if (peer.Status == Peer::Status::Ready)
			{
				// We don't need these anymore and neither does the CIA
				crypto_wipe(peer.PublicKey.data(), peer.PublicKey.size());
				crypto_wipe(peer.PrivateKey.data(), peer.PrivateKey.size());
				crypto_wipe(peer.PeerPublicKey.data(), peer.PeerPublicKey.size());

				return true;
			}
		}
	}

	return false;
}

bool HandshakeExtender::ProcessChatMessage(const QuantumGate::PeerLUID pluid, const QuantumGate::Buffer* msgdata)
{
	if (msgdata->GetSize() >= sizeof(ChatMessage))
	{
		std::wcout << L"HandshakeExtender received chat message from peer " << pluid << L"\r\n";

		std::shared_lock<std::shared_mutex> lock(m_PeersMutex);

		const auto it = m_Peers.find(pluid);
		if (it != m_Peers.end())
		{
			auto& peer = *it->second;

			std::shared_lock<std::shared_mutex> lock(peer.Mutex);

			if (peer.Status == Peer::Status::Ready)
			{
				return DisplayChatMessage(peer, msgdata);
			}
		}
	}

	return false;
}

void HandshakeExtender::MainThreadFunction(HandshakeExtender* extender)
{
	std::wcout << L"HandshakeExtender::MainThreadFunction() entry...\r\n";

	using namespace std::chrono_literals;

	while (!extender->m_ShutdownEvent.load())
	{
		{
			std::shared_lock<std::shared_mutex> lock(extender->m_PeersMutex);

			for (const auto& it : extender->m_Peers)
			{
				auto& peer = *it.second;

				std::unique_lock<std::shared_mutex> lock(peer.Mutex);

				if (peer.Status != Peer::Status::Exception)
				{
					if (peer.Status != Peer::Status::Ready &&
						(std::chrono::steady_clock::now() - peer.ConnectedSteadyTime > MaxHandshakeDuration))
					{
						// We get here if the handshake for this extender took too long
						// to complete; we change the peer status to 'Exception' so that
						// we'll ignore it from now on. And, perhaps a bit heavy handed
						// because this may affect other extenders that use this peer, we
						// disconnect the peer as well.

						std::wcout << L"Handshake timeout for peer " << peer.LUID << L"\r\n";
						peer.Status = Peer::Status::Exception;

						// The below call will block; supply a second parameter
						// for a callback function (may be nullptr) for async disconnect
						if (!extender->DisconnectFrom(peer.LUID).Succeeded())
						{
							std::wcout << L"Failed to disconnect from peer " << peer.LUID << L"\r\n";
						}
					}
					else if (peer.ConnectionType == QuantumGate::PeerConnectionType::Inbound &&
							 peer.Status == Peer::Status::Connected)
					{
						// We get here for newly connected peers;
						// initiate the handshake
						extender->SendPublicKey(peer);
						peer.Status = Peer::Status::PubKeySent;
					}
				}
			}
		}

		std::this_thread::sleep_for(1ms);
	}

	std::wcout << L"HandshakeExtender::MainThreadFunction() exit...\r\n";
}

bool HandshakeExtender::SendPublicKey(Peer& peer)
{
	PublicKeyMessage pubmsg;
	std::memcpy(&pubmsg.PublicKey, peer.PublicKey.data(), peer.PublicKey.size());

	QuantumGate::Buffer msg(reinterpret_cast<std::byte*>(&pubmsg), sizeof(PublicKeyMessage));

	const auto result = SendMessageTo(peer.LUID, std::move(msg), NoCompression);
	if (result.Succeeded())
	{
		std::wcout << L"Sent public key to peer " << peer.LUID << L"\r\n";
		return true;
	}
	else
	{
		std::wcout << L"Failed to send public key to peer " << peer.LUID << L": " << result << L"\r\n";
	}

	return false;
}

bool HandshakeExtender::SendReady(Peer& peer)
{
	ReadyMessage rmsg;
	QuantumGate::Buffer msg(reinterpret_cast<std::byte*>(&rmsg), sizeof(ReadyMessage));

	const auto result = SendMessageTo(peer.LUID, std::move(msg), NoCompression);
	if (result.Succeeded())
	{
		std::wcout << L"Sent ready to peer " << peer.LUID << L"\r\n";
		return true;
	}
	else
	{
		std::wcout << L"Failed to send ready to peer " << peer.LUID << L": " << result << L"\r\n";
	}

	return false;
}

bool HandshakeExtender::SendChatMessage(const Peer& peer, const std::wstring& msg)
{
	// Only if the peer is in the ready state
	if (peer.Status == Peer::Status::Ready && !msg.empty())
	{
		ChatMessage cmsg;

		if (GetRandomBytes(cmsg.Nonce, sizeof(cmsg.Nonce)))
		{
			const auto msglen = msg.size() * sizeof(std::wstring::value_type);
			QuantumGate::Buffer cipher_text(msglen);

			// Encrypt message
			crypto_lock(reinterpret_cast<std::uint8_t*>(cmsg.Mac),
						reinterpret_cast<std::uint8_t*>(cipher_text.GetBytes()),
						reinterpret_cast<const std::uint8_t*>(peer.SharedSecretKey.data()),
						reinterpret_cast<const std::uint8_t*>(cmsg.Nonce),
						reinterpret_cast<const std::uint8_t*>(msg.data()),
						msglen);

			QuantumGate::Buffer msg(reinterpret_cast<std::byte*>(&cmsg), sizeof(ChatMessage));
			msg += cipher_text;

			const auto result = SendMessageTo(peer.LUID, std::move(msg), NoCompression);
			if (result.Succeeded())
			{
				std::wcout << L"Sent chat message to peer " << peer.LUID << L"\r\n";
				return true;
			}
			else
			{
				std::wcout << L"Failed to send chat message to peer " << peer.LUID << L": " << result << L"\r\n";
			}
		}
	}

	return false;
}

bool HandshakeExtender::DisplayChatMessage(const Peer& peer, const QuantumGate::Buffer* msgdata)
{
	if (msgdata->GetSize() >= sizeof(ChatMessage))
	{
		const auto cmsg = reinterpret_cast<const ChatMessage*>(msgdata->GetBytes());
		const auto msgsize = msgdata->GetSize() - sizeof(ChatMessage);
		const auto msgptr = msgdata->GetBytes() + sizeof(ChatMessage);

		if (msgsize > 0)
		{
			// Should be exact multiple
			if (msgsize % sizeof(std::wstring::value_type) == 0)
			{
				std::wstring plain_text;
				plain_text.resize(msgsize / sizeof(std::wstring::value_type));

				// Decrypt message
				const auto result = crypto_unlock(reinterpret_cast<std::uint8_t*>(plain_text.data()),
												  reinterpret_cast<const std::uint8_t*>(peer.SharedSecretKey.data()),
												  reinterpret_cast<const std::uint8_t*>(cmsg->Nonce),
												  reinterpret_cast<const std::uint8_t*>(cmsg->Mac),
												  reinterpret_cast<const std::uint8_t*>(msgptr),
												  msgsize);
				if (result == 0)
				{
					std::wcout << L"\r\n";
					std::wcout << L"\x1b[96m" << L"Peer " << peer.LUID << L" >> " << plain_text << L"\x1b[39m" << L"\r\n";
					return true;
				}
				else
				{
					std::wcout << L"Received corrupted chat message from peer " << peer.LUID << L"\r\n";
				}
			}
		}
	}

	return false;
}

bool HandshakeExtender::BroadcastToConnectedPeers(const std::wstring& msg)
{
	std::shared_lock<std::shared_mutex> lock(m_PeersMutex);

	for (const auto& it : m_Peers)
	{
		auto& peer = *it.second;

		std::shared_lock<std::shared_mutex> lock(peer.Mutex);

		if (peer.Status == Peer::Status::Ready)
		{
			SendChatMessage(peer, msg);
		}
	}

	return true;
}

bool HandshakeExtender::GenerateDHKeyPair(Peer& peer)
{
	// First get random bytes to serve as private key
	if (GetRandomBytes(peer.PrivateKey.data(), peer.PrivateKey.size()))
	{
		// Derive the public key from the private key
		crypto_key_exchange_public_key(reinterpret_cast<std::uint8_t*>(peer.PublicKey.data()),
									   reinterpret_cast<std::uint8_t*>(peer.PrivateKey.data()));

		std::wcout << L"Successfully generated DH key for peer " << peer.LUID << L"\r\n";
		return true;
	}
	else
	{
		std::wcout << L"Failed to generate DH key for peer " << peer.LUID << L"\r\n";
	}

	return false;
}

bool HandshakeExtender::GenerateSharedKey(Peer& peer)
{
	const auto result = crypto_key_exchange(reinterpret_cast<std::uint8_t*>(peer.SharedSecretKey.data()),
											reinterpret_cast<std::uint8_t*>(peer.PrivateKey.data()),
											reinterpret_cast<std::uint8_t*>(peer.PeerPublicKey.data()));

	if (result == 0)
	{
		std::wcout << L"Successfully generated shared key for peer " << peer.LUID << L"\r\n";
		return true;
	}
	else
	{
		std::wcout << L"Failed to generate shared key for peer " << peer.LUID << L"\r\n";
	}

	return false;
}

bool HandshakeExtender::GetRandomBytes(std::byte* buffer, const std::size_t buffer_len)
{
	auto success = false;

	HCRYPTPROV ctx{ 0 };
	if (CryptAcquireContext(&ctx, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		if (CryptGenRandom(ctx, static_cast<DWORD>(buffer_len), reinterpret_cast<BYTE*>(buffer)))
		{
			success = true;
		}

		CryptReleaseContext(ctx, 0);
	}

	return success;
}
