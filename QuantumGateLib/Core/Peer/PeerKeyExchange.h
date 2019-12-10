// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PeerKeys.h"
#include "..\KeyGeneration\KeyGenerationManager.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class KeyExchange final
	{
	public:
		KeyExchange() = delete;
		KeyExchange(KeyGeneration::Manager& keymgr) noexcept : m_KeyManager(keymgr) {}
		KeyExchange(const KeyExchange&) = delete;
		KeyExchange(KeyExchange&&) noexcept = default;
		~KeyExchange() = default;
		KeyExchange& operator=(const KeyExchange&) = delete;
		KeyExchange& operator=(KeyExchange&&) noexcept = default;

		[[nodiscard]] inline bool GeneratePrimaryAsymmetricKeys(const Algorithms& algorithms,
																const Crypto::AsymmetricKeyOwner type) noexcept
		{
			return GenerateAsymmetricKeys(m_PrimaryAsymmetricKeys, algorithms.PrimaryAsymmetric, type);
		}

		inline void SetPeerPrimaryHandshakeData(ProtectedBuffer&& buffer) noexcept
		{
			// Asymmetric keys should already have been created
			assert(m_PrimaryAsymmetricKeys != nullptr);

			if (m_PrimaryAsymmetricKeys->GetKeyExchangeType() == Crypto::KeyExchangeType::KeyEncapsulation &&
				m_PrimaryAsymmetricKeys->GetOwner() == Crypto::AsymmetricKeyOwner::Alice)
			{
				m_PrimaryAsymmetricKeys->EncryptedSharedSecret = std::move(buffer);
			}

			m_PrimaryAsymmetricKeys->PeerPublicKey = std::move(buffer);
		}

		inline const ProtectedBuffer& GetPrimaryHandshakeData() const noexcept
		{
			// Asymmetric keys should already have been created
			assert(m_PrimaryAsymmetricKeys != nullptr);

			if (m_PrimaryAsymmetricKeys->GetKeyExchangeType() == Crypto::KeyExchangeType::KeyEncapsulation &&
				m_PrimaryAsymmetricKeys->GetOwner() == Crypto::AsymmetricKeyOwner::Bob)
			{
				return m_PrimaryAsymmetricKeys->EncryptedSharedSecret;
			}

			return m_PrimaryAsymmetricKeys->LocalPublicKey;
		}

		[[nodiscard]] bool GenerateSecondaryAsymmetricKeys(const Algorithms& algorithms,
														   const Crypto::AsymmetricKeyOwner owner) noexcept
		{
			return GenerateAsymmetricKeys(m_SecondaryAsymmetricKeys, algorithms.SecondaryAsymmetric, owner);
		}

		inline void SetPeerSecondaryHandshakeData(ProtectedBuffer&& buffer) noexcept
		{
			// Asymmetric keys should already have been created
			assert(m_SecondaryAsymmetricKeys != nullptr);

			if (m_SecondaryAsymmetricKeys->GetKeyExchangeType() == Crypto::KeyExchangeType::KeyEncapsulation &&
				m_SecondaryAsymmetricKeys->GetOwner() == Crypto::AsymmetricKeyOwner::Alice)
			{
				m_SecondaryAsymmetricKeys->EncryptedSharedSecret = std::move(buffer);
			}

			m_SecondaryAsymmetricKeys->PeerPublicKey = std::move(buffer);
		}

		inline const ProtectedBuffer& GetSecondaryHandshakeData() const noexcept
		{
			// Asymmetric keys should already have been created
			assert(m_SecondaryAsymmetricKeys != nullptr);

			if (m_SecondaryAsymmetricKeys->GetKeyExchangeType() == Crypto::KeyExchangeType::KeyEncapsulation &&
				m_SecondaryAsymmetricKeys->GetOwner() == Crypto::AsymmetricKeyOwner::Bob)
			{
				return m_SecondaryAsymmetricKeys->EncryptedSharedSecret;
			}

			return m_SecondaryAsymmetricKeys->LocalPublicKey;
		}

		[[nodiscard]] bool GeneratePrimarySymmetricKeyPair(const ProtectedBuffer& global_sharedsecret,
														   const Algorithms& algorithms,
														   const PeerConnectionType pctype) noexcept
		{
			// Should not already have a key-pair
			assert(m_PrimarySymmetricKeyPair == nullptr);

			try
			{
				m_PrimarySymmetricKeyPair = std::make_shared<SymmetricKeyPair>();
				m_PrimarySymmetricKeyPair->UseForDecryption = true;
			}
			catch (...) { return false; }

			if (GeneratePrimarySharedSecret())
			{
				if (SymmetricKeys::GenerateSymmetricKeyPair(m_PrimarySymmetricKeyPair,
															m_PrimaryAsymmetricKeys->SharedSecret,
															global_sharedsecret, algorithms, pctype))
				{
					return true;
				}
			}

			return false;
		}

		const std::shared_ptr<SymmetricKeyPair>& GetPrimarySymmetricKeyPair() const noexcept { return m_PrimarySymmetricKeyPair; }

		[[nodiscard]] bool GenerateSecondarySymmetricKeyPair(const ProtectedBuffer& global_sharedsecret,
															 const Algorithms& algorithms,
															 const PeerConnectionType pctype) noexcept
		{
			// Should not already have a key-pair
			assert(m_SecondarySymmetricKeyPair == nullptr);

			try
			{
				m_SecondarySymmetricKeyPair = std::make_shared<SymmetricKeyPair>();
				m_SecondarySymmetricKeyPair->UseForDecryption = true;
			}
			catch (...) { return false; }

			if (GenerateSecondarySharedSecret())
			{
				if (SymmetricKeys::GenerateSymmetricKeyPair(m_SecondarySymmetricKeyPair,
															m_SecondaryAsymmetricKeys->SharedSecret,
															global_sharedsecret, algorithms, pctype))
				{
					return true;
				}
			}

			return false;
		}

		const std::shared_ptr<SymmetricKeyPair>& GetSecondarySymmetricKeyPair() const noexcept { return m_SecondarySymmetricKeyPair; }

		inline void StartUsingPrimarySymmetricKeyPairForEncryption() noexcept
		{
			assert(m_PrimarySymmetricKeyPair->EncryptionKey != nullptr &&
				   m_PrimarySymmetricKeyPair->DecryptionKey != nullptr);
			m_PrimarySymmetricKeyPair->UseForEncryption = true;
		}

		inline void StartUsingSecondarySymmetricKeyPairForEncryption() noexcept
		{
			assert(m_SecondarySymmetricKeyPair->EncryptionKey != nullptr &&
				   m_SecondarySymmetricKeyPair->DecryptionKey != nullptr);
			m_SecondarySymmetricKeyPair->UseForEncryption = true;
		}

		[[nodiscard]] inline bool AddKeyExchangeData(ProtectedBuffer& data) const noexcept
		{
			try
			{
				// The order in which we add the key exchange data matters
				// from the perspective of Alice and Bob
				switch (m_PrimaryAsymmetricKeys->GetOwner())
				{
					case Crypto::AsymmetricKeyOwner::Alice:
					{
						data += m_PrimaryAsymmetricKeys->LocalPublicKey;
						data += m_SecondaryAsymmetricKeys->LocalPublicKey;

						// In the case of key encapsulation Alice does not receive a public key from Bob
						if (m_PrimaryAsymmetricKeys->GetKeyExchangeType() != Crypto::KeyExchangeType::KeyEncapsulation)
						{
							data += m_PrimaryAsymmetricKeys->PeerPublicKey;
						}

						if (m_SecondaryAsymmetricKeys->GetKeyExchangeType() != Crypto::KeyExchangeType::KeyEncapsulation)
						{
							data += m_SecondaryAsymmetricKeys->PeerPublicKey;
						}

						break;
					}
					case Crypto::AsymmetricKeyOwner::Bob:
					{
						data += m_PrimaryAsymmetricKeys->PeerPublicKey;
						data += m_SecondaryAsymmetricKeys->PeerPublicKey;

						// In the case of key encapsulation Bob does not have a public key
						if (m_PrimaryAsymmetricKeys->GetKeyExchangeType() != Crypto::KeyExchangeType::KeyEncapsulation)
						{
							data += m_PrimaryAsymmetricKeys->LocalPublicKey;
						}

						if (m_SecondaryAsymmetricKeys->GetKeyExchangeType() != Crypto::KeyExchangeType::KeyEncapsulation)
						{
							data += m_SecondaryAsymmetricKeys->LocalPublicKey;
						}

						break;
					}
					default:
					{
						// Shouldn't get here
						assert(false);
						return false;
					}
				}

				data += m_PrimaryAsymmetricKeys->SharedSecret;
				data += m_SecondaryAsymmetricKeys->SharedSecret;

				return true;
			}
			catch (...) {}

			return false;
		}

	private:
		[[nodiscard]] inline bool GenerateAsymmetricKeys(std::shared_ptr<Crypto::AsymmetricKeyData>& keydata,
														 const Algorithm::Asymmetric aa,
														 const Crypto::AsymmetricKeyOwner owner) noexcept
		{
			// Should not already have a key
			assert(keydata == nullptr);

			try
			{
				keydata = std::make_shared<Crypto::AsymmetricKeyData>(aa);
			}
			catch (...) { return false; }

			if (keydata->GetKeyExchangeType() == Crypto::KeyExchangeType::KeyEncapsulation &&
				owner == Crypto::AsymmetricKeyOwner::Bob)
			{
				// Bob doesn't need an asymmetric keypair;
				// he'll encrypt a shared secret using Alice's public key
				keydata->SetOwner(owner);
				return true;
			}

			// First check if we have a pre-generated keypair available
			auto keys = m_KeyManager.GetAsymmetricKeys(aa);
			if (keys)
			{
				*keydata = std::move(*keys);
				keydata->SetOwner(owner);
				return true;
			}

			// Generate an asymmetric keypair on the fly below (slower
			// especially for certain algorithms, which introduces delays
			// in the connection handshake which might result in timeouts)
			if (Crypto::GenerateAsymmetricKeys(*keydata))
			{
				keydata->SetOwner(owner);
				return true;
			}

			return false;
		}

		[[nodiscard]] inline bool GeneratePrimarySharedSecret() noexcept
		{
			return GenerateSharedSecret(m_PrimaryAsymmetricKeys);
		}

		[[nodiscard]] inline bool GenerateSecondarySharedSecret() noexcept
		{
			return GenerateSharedSecret(m_SecondaryAsymmetricKeys);
		}

		[[nodiscard]] inline bool GenerateSharedSecret(const std::shared_ptr<Crypto::AsymmetricKeyData>& keydata) noexcept
		{
			// Asymmetric keys should already have been created
			assert(keydata != nullptr);

			return Crypto::GenerateSharedSecret(*keydata);
		}

	private:
		KeyGeneration::Manager& m_KeyManager;

		std::shared_ptr<Crypto::AsymmetricKeyData> m_PrimaryAsymmetricKeys;
		std::shared_ptr<Crypto::AsymmetricKeyData> m_SecondaryAsymmetricKeys;

		std::shared_ptr<SymmetricKeyPair> m_PrimarySymmetricKeyPair;
		std::shared_ptr<SymmetricKeyPair> m_SecondarySymmetricKeyPair;
	};
}