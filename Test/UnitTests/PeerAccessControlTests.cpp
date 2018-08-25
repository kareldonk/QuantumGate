// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"
#include "Common\Util.h"
#include "Settings.h"
#include "Core\Access\PeerAccessControl.h"
#include "Crypto\Crypto.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Core::Access;

namespace UnitTests
{
	TEST_CLASS(PeerAccessControlTests)
	{
	public:
		TEST_METHOD(General)
		{
			Settings_CThS settings;
			settings.UpdateValue([](Settings& set)
			{
				set.Local.RequireAuthentication = true;
			});

			PeerAccessControl pac(settings);

			// Set access default
			pac.SetAccessDefault(PeerAccessDefault::Allowed);
			Assert::AreEqual(true, pac.GetAccessDefault() == PeerAccessDefault::Allowed);

			pac.SetAccessDefault(PeerAccessDefault::NotAllowed);
			Assert::AreEqual(true, pac.GetAccessDefault() == PeerAccessDefault::NotAllowed);

			// Peer not allowed
			Assert::AreEqual(false,
							 pac.IsAllowed(QuantumGate::UUID(L"3c0c4c02-5ebc-f99a-0b5e-acdd238b1e54")).Value());

			// Add peer
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"3c0c4c02-5ebc-f99a-0b5e-acdd238b1e54");
				pas.AccessAllowed = true;
				String pub_key_b64 = L"LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUZrd0V3WUhLb1pJemowQ0FRWUlLb1pJemowREFRY0RRZ0FFV01PK3NIWGJDM3pmV0ZNbGYwUXB5TjlkejEvUwpZM3hlRFJFR24xN3F5OGJYbDk1YU9hbzk5Mzh5QzRETmxXSkMxL1crMnVaSGRxWGpWVExUZEJQNkFRPT0KLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg==";
				const auto buffer = Util::FromBase64(pub_key_b64);
				Assert::AreEqual(true, buffer.has_value());
				pas.PublicKey = BufferView(*buffer);

				Assert::AreEqual(true, pac.AddPeer(std::move(pas)).Succeeded());
			}

			// Peer allowed
			Assert::AreEqual(true, pac.IsAllowed(QuantumGate::UUID(L"3c0c4c02-5ebc-f99a-0b5e-acdd238b1e54")).Value());

			// Add peer invalid/unset UUID
			{
				PeerAccessSettings pas;
				pas.AccessAllowed = true;
				String pub_key_b64 = L"LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUZrd0V3WUhLb1pJemowQ0FRWUlLb1pJemowREFRY0RRZ0FFYmlreXZ1a2JXVzBHcWhXRU0wUzdyOXR5Mk5zegphUjl3TGlDd1RackNPbGlMSVoxc1poU3ZJMUxXRU1XbGd4dHhMYlRibHdCckxKRlZZcmU1ZDhNaGZnPT0KLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg==";
				const auto buffer = Util::FromBase64(pub_key_b64);
				Assert::AreEqual(true, buffer.has_value());
				pas.PublicKey = BufferView(*buffer);

				Assert::AreEqual(true, pac.AddPeer(std::move(pas)) == ResultCode::InvalidArgument);
			}

			// Add peer UUID and public key mismatch
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"e938194b-52c1-69d4-0b84-75d3d11dbfad");
				pas.AccessAllowed = true;
				String pub_key_b64 = L"LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUZrd0V3WUhLb1pJemowQ0FRWUlLb1pJemowREFRY0RRZ0FFYmlreXZ1a2JXVzBHcWhXRU0wUzdyOXR5Mk5zegphUjl3TGlDd1RackNPbGlMSVoxc1poU3ZJMUxXRU1XbGd4dHhMYlRibHdCckxKRlZZcmU1ZDhNaGZnPT0KLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg==";
				const auto buffer = Util::FromBase64(pub_key_b64);
				Assert::AreEqual(true, buffer.has_value());
				pas.PublicKey = BufferView(*buffer);

				Assert::AreEqual(true, pac.AddPeer(std::move(pas)) == ResultCode::InvalidArgument);
			}

			// Add peer
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"e938164b-52c1-69d4-0b84-75d3d11dbfad");
				pas.AccessAllowed = true;
				String pub_key_b64 = L"LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUZrd0V3WUhLb1pJemowQ0FRWUlLb1pJemowREFRY0RRZ0FFYmlreXZ1a2JXVzBHcWhXRU0wUzdyOXR5Mk5zegphUjl3TGlDd1RackNPbGlMSVoxc1poU3ZJMUxXRU1XbGd4dHhMYlRibHdCckxKRlZZcmU1ZDhNaGZnPT0KLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg==";
				const auto buffer = Util::FromBase64(pub_key_b64);
				Assert::AreEqual(true, buffer.has_value());
				pas.PublicKey = BufferView(*buffer);

				Assert::AreEqual(true, pac.AddPeer(std::move(pas)).Succeeded());
			}

			// Add peer that already exists
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"e938164b-52c1-69d4-0b84-75d3d11dbfad");
				pas.AccessAllowed = true;
				String pub_key_b64 = L"LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUZrd0V3WUhLb1pJemowQ0FRWUlLb1pJemowREFRY0RRZ0FFYmlreXZ1a2JXVzBHcWhXRU0wUzdyOXR5Mk5zegphUjl3TGlDd1RackNPbGlMSVoxc1poU3ZJMUxXRU1XbGd4dHhMYlRibHdCckxKRlZZcmU1ZDhNaGZnPT0KLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg==";
				const auto buffer = Util::FromBase64(pub_key_b64);
				Assert::AreEqual(true, buffer.has_value());
				pas.PublicKey = BufferView(*buffer);

				Assert::AreEqual(true, pac.AddPeer(std::move(pas)) == ResultCode::PeerAlreadyExists);
			}

			// Peer allowed
			Assert::AreEqual(true,
							 pac.IsAllowed(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Value());

			// Update peer that already exists
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"e938164b-52c1-69d4-0b84-75d3d11dbfad");
				pas.AccessAllowed = false;

				Assert::AreEqual(true, pac.UpdatePeer(std::move(pas)).Succeeded());
			}

			// Peer not allowed anymore
			Assert::AreEqual(false,
							 pac.IsAllowed(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Value());

			// Update peer that doesn't exists
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"e938194b-52c1-69d4-0b84-75d3d11dbfad");
				pas.AccessAllowed = false;

				Assert::AreEqual(true, pac.UpdatePeer(std::move(pas)) == ResultCode::PeerNotFound);
			}

			// Should have two peers
			Assert::AreEqual(true, pac.GetPeers().Value().size() == 2);

			// Remove peers
			Assert::AreEqual(true,
							 pac.RemovePeer(QuantumGate::UUID(L"3c0c4c02-5ebc-f99a-0b5e-acdd238b1e54")).Succeeded());
			Assert::AreEqual(true,
							 pac.RemovePeer(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Succeeded());

			// Remove peer that does not exist
			Assert::AreEqual(true,
							 pac.RemovePeer(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")) ==
							 ResultCode::PeerNotFound);

			// Should be empty
			Assert::AreEqual(true, pac.GetPeers().Value().empty());

			// Add peer again
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"e938164b-52c1-69d4-0b84-75d3d11dbfad");
				pas.AccessAllowed = true;
				String pub_key_b64 = L"LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUZrd0V3WUhLb1pJemowQ0FRWUlLb1pJemowREFRY0RRZ0FFYmlreXZ1a2JXVzBHcWhXRU0wUzdyOXR5Mk5zegphUjl3TGlDd1RackNPbGlMSVoxc1poU3ZJMUxXRU1XbGd4dHhMYlRibHdCckxKRlZZcmU1ZDhNaGZnPT0KLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg==";
				const auto buffer = Util::FromBase64(pub_key_b64);
				Assert::AreEqual(true, buffer.has_value());
				pas.PublicKey = BufferView(*buffer);

				Assert::AreEqual(true, pac.AddPeer(std::move(pas)).Succeeded());
				Assert::AreEqual(true, Crypto::CompareBuffers(*buffer,
															  *pac.GetPublicKey(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad"))));
			}

			// Get public key of known peer
			Assert::AreEqual(true, pac.GetPublicKey(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")) != nullptr);

			// Get public key of unknown peer
			Assert::AreEqual(true, pac.GetPublicKey(QuantumGate::UUID(L"e938194b-52c1-69d4-0b84-75d3d11dbfad")) == nullptr);

			pac.Clear();

			// Should be empty
			Assert::AreEqual(true, pac.GetPeers().Value().empty());
		}

		TEST_METHOD(Access)
		{
			Settings_CThS settings;
			settings.UpdateValue([](Settings& set)
			{
				set.Local.RequireAuthentication = false;
			});

			PeerAccessControl pac(settings);
			pac.SetAccessDefault(PeerAccessDefault::Allowed);

			// Unknown peer allowed because of default setting
			Assert::AreEqual(true,
							 pac.IsAllowed(QuantumGate::UUID(L"3c0c4c02-5ebc-f99a-0b5e-acdd238b1e54")).Value());

			pac.SetAccessDefault(PeerAccessDefault::NotAllowed);

			// Unknown peer not allowed because of default setting
			Assert::AreEqual(false,
							 pac.IsAllowed(QuantumGate::UUID(L"3c0c4c02-5ebc-f99a-0b5e-acdd238b1e54")).Value());

			// Add peer
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"3c0c4c02-5ebc-f99a-0b5e-acdd238b1e54");
				pas.AccessAllowed = true;
				String pub_key_b64 = L"LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUZrd0V3WUhLb1pJemowQ0FRWUlLb1pJemowREFRY0RRZ0FFV01PK3NIWGJDM3pmV0ZNbGYwUXB5TjlkejEvUwpZM3hlRFJFR24xN3F5OGJYbDk1YU9hbzk5Mzh5QzRETmxXSkMxL1crMnVaSGRxWGpWVExUZEJQNkFRPT0KLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg==";
				const auto buffer = Util::FromBase64(pub_key_b64);
				Assert::AreEqual(true, buffer.has_value());
				pas.PublicKey = BufferView(*buffer);

				Assert::AreEqual(true, pac.AddPeer(std::move(pas)).Succeeded());
			}

			// Peer allowed because it's now known
			Assert::AreEqual(true,
							 pac.IsAllowed(QuantumGate::UUID(L"3c0c4c02-5ebc-f99a-0b5e-acdd238b1e54")).Value());


			// Add peer that's not allowed
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"e938164b-52c1-69d4-0b84-75d3d11dbfad");
				pas.AccessAllowed = false;
				String pub_key_b64 = L"LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUZrd0V3WUhLb1pJemowQ0FRWUlLb1pJemowREFRY0RRZ0FFYmlreXZ1a2JXVzBHcWhXRU0wUzdyOXR5Mk5zegphUjl3TGlDd1RackNPbGlMSVoxc1poU3ZJMUxXRU1XbGd4dHhMYlRibHdCckxKRlZZcmU1ZDhNaGZnPT0KLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg==";
				const auto buffer = Util::FromBase64(pub_key_b64);
				Assert::AreEqual(true, buffer.has_value());
				pas.PublicKey = BufferView(*buffer);

				Assert::AreEqual(true, pac.AddPeer(std::move(pas)).Succeeded());
			}

			// Not allowed because of peer setting
			Assert::AreEqual(false,
							 pac.IsAllowed(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Value());

			pac.SetAccessDefault(PeerAccessDefault::Allowed);

			// Still not allowed
			Assert::AreEqual(false,
							 pac.IsAllowed(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Value());

			pac.SetAccessDefault(PeerAccessDefault::NotAllowed);

			// Update peer settings to allow but without public key
			{
				PeerAccessSettings pas;
				pas.UUID.Set(L"e938164b-52c1-69d4-0b84-75d3d11dbfad");
				pas.AccessAllowed = true;

				Assert::AreEqual(true, pac.UpdatePeer(std::move(pas)).Succeeded());

				// Get empty public key of known peer
				Assert::AreEqual(true,
								 pac.GetPublicKey(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")) == nullptr);
			}

			settings.UpdateValue([](Settings& set)
			{
				set.Local.RequireAuthentication = true;
			});

			// Peer not allowed due to authentication required setting while peer doesn't have a public key
			Assert::AreEqual(false,
							 pac.IsAllowed(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Value());

			settings.UpdateValue([](Settings& set)
			{
				set.Local.RequireAuthentication = false;
			});

			// Peer is now allowed
			Assert::AreEqual(true,
							 pac.IsAllowed(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Value());

			// Remove peer
			Assert::AreEqual(true,
							 pac.RemovePeer(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Succeeded());

			// Peer is not allowed due to access default
			Assert::AreEqual(false,
							 pac.IsAllowed(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Value());

			pac.SetAccessDefault(PeerAccessDefault::Allowed);

			// Peer is allowed due to access default
			Assert::AreEqual(true,
							 pac.IsAllowed(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Value());

			settings.UpdateValue([](Settings& set)
			{
				set.Local.RequireAuthentication = true;
			});

			// Peer is not allowed due to authentication requirements
			Assert::AreEqual(false,
							 pac.IsAllowed(QuantumGate::UUID(L"e938164b-52c1-69d4-0b84-75d3d11dbfad")).Value());
		}
	};
}