// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core::Peer
{
	using Algorithms = PeerConnectionAlgorithms;

	struct SymmetricKeyPair final
	{
		[[nodiscard]] bool IsExpired() const noexcept
		{
			// If it has an expiration time
			if (ExpirationSteadyTime.has_value())
			{
				// Check if it has been expired for too long
				if (Util::GetCurrentSteadyTime() - *ExpirationSteadyTime > ExpirationGracePeriod)
				{
					return true;
				}
			}

			return false;
		}

		std::shared_ptr<Crypto::SymmetricKeyData> EncryptionKey;
		std::shared_ptr<Crypto::SymmetricKeyData> DecryptionKey;

		bool UseForEncryption{ false };
		bool UseForDecryption{ false };

		std::optional<SteadyTime> ExpirationSteadyTime;

		// Maximum amount of seconds a key can still be used after having been expired
		static const constexpr std::chrono::seconds ExpirationGracePeriod{ 120 };
	};

	using SymmetricKeyPairCollection = Vector<std::shared_ptr<SymmetricKeyPair>>;

	class SymmetricKeys final
	{
	public:
		inline SymmetricKeyPairCollection& GetSymmetricKeyPairs() noexcept { return m_SymmetricKeyPairs; }

		[[nodiscard]] bool GenerateAndAddSymmetricKeyPair(const ProtectedBuffer& sharedsecret,
														  const ProtectedBuffer& global_sharedsecret,
														  const Algorithms& pa,
														  const PeerConnectionType pctype) noexcept
		{
			try
			{
				auto keys = std::make_shared<SymmetricKeyPair>();
				if (GenerateSymmetricKeyPair(keys, sharedsecret, global_sharedsecret,
											 pa, pctype))
				{
					keys->UseForEncryption = true;
					keys->UseForDecryption = true;

					return AddSymmetricKeyPair(keys);
				}
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] bool AddSymmetricKeyPair(const std::shared_ptr<SymmetricKeyPair>& keypair) noexcept
		{
			assert(!keypair->EncryptionKey->Key.IsEmpty() && !keypair->EncryptionKey->AuthKey.IsEmpty() &&
				   !keypair->DecryptionKey->Key.IsEmpty() && !keypair->DecryptionKey->AuthKey.IsEmpty());

			try
			{
				m_SymmetricKeyPairs.insert(m_SymmetricKeyPairs.begin(), keypair);
			}
			catch (...)
			{
				return false;
			}

			// Remove older keys if the collection grows too big
			if (m_SymmetricKeyPairs.size() > SymmetricKeys::MaxNumSymmetricKeyPairs) m_SymmetricKeyPairs.pop_back();

			return true;
		}

		std::pair<std::shared_ptr<Crypto::SymmetricKeyData>, Buffer> GetEncryptionKeyAndNonce(const UInt32 nonce_seed,
																							  const PeerConnectionType pctype,
																							  const bool autogenkey_allowed) const noexcept
		{
			auto error = false;

			// Get the most recent key that's enabled;
			// the most recent keys are in front
			for (std::size_t x = 0; x < m_SymmetricKeyPairs.size(); ++x)
			{
				if (m_SymmetricKeyPairs[x]->UseForEncryption &&
					!m_SymmetricKeyPairs[x]->IsExpired())
				{
					Buffer nonce;
					if (GetNonce(nonce_seed, nonce, m_SymmetricKeyPairs[x]->EncryptionKey->HashAlgorithm))
					{
						return std::make_pair(m_SymmetricKeyPairs[x]->EncryptionKey, std::move(nonce));
					}
					else error = true;

					break;
				}
			}

			// If we found no enabled keys we use
			// the autogen key if allowed
			if (!error && autogenkey_allowed)
			{
				return GetAutoGenKeyAndNonce(nonce_seed, pctype, true);
			}

			// Should return nullptr for symmetric key to indicate failure
			return std::make_pair(nullptr, Buffer());
		}

		std::pair<std::shared_ptr<Crypto::SymmetricKeyData>, Buffer> GetDecryptionKeyAndNonce(const UInt32 keynum,
																							  const UInt32 nonce_seed,
																							  const PeerConnectionType pctype,
																							  const bool autogenkey_allowed) const noexcept
		{
			// If we have a symmetric key use it otherwise we'll generate one
			// if allowed (an autogen key)
			const auto numkeys = static_cast<UInt32>(m_SymmetricKeyPairs.size());

			if (numkeys > 0 && keynum < numkeys)
			{
				if (m_SymmetricKeyPairs[keynum]->UseForDecryption &&
					!m_SymmetricKeyPairs[keynum]->IsExpired())
				{
					Buffer nonce;
					if (GetNonce(nonce_seed, nonce, m_SymmetricKeyPairs[keynum]->DecryptionKey->HashAlgorithm))
					{
						return std::make_pair(m_SymmetricKeyPairs[keynum]->DecryptionKey, std::move(nonce));
					}
				}
			}
			else if (keynum == numkeys && autogenkey_allowed)
			{
				// Autogen key is the last key we try
				return GetAutoGenKeyAndNonce(nonce_seed, pctype, false);
			}

			// Should return nullptr for symmetric key to indicate failure
			return std::make_pair(nullptr, Buffer());
		}

		[[nodiscard]] bool HasNumBytesProcessedExceededForLatestKeyPair(const Size max_num) const noexcept
		{
			return (GetNumBytesProcessedForLatestKeyPair(m_SymmetricKeyPairs) > max_num);
		}

		[[nodiscard]] static bool GenerateSymmetricKeyPair(const std::shared_ptr<SymmetricKeyPair>& keypair,
														   const ProtectedBuffer& sharedsecret,
														   const ProtectedBuffer& global_sharedsecret,
														   const Algorithms& pa,
														   const PeerConnectionType pctype) noexcept
		{
			// Should have shared secret
			assert(!sharedsecret.IsEmpty());

			// Keys should not already have been created
			assert(keypair->EncryptionKey == nullptr);
			assert(keypair->DecryptionKey == nullptr);

			try
			{
				auto key1 = std::make_shared<Crypto::SymmetricKeyData>(Crypto::SymmetricKeyType::Derived,
																	   pa.Hash,
																	   pa.Symmetric,
																	   pa.Compression);

				auto key2 = std::make_shared<Crypto::SymmetricKeyData>(Crypto::SymmetricKeyType::Derived,
																	   pa.Hash,
																	   pa.Symmetric,
																	   pa.Compression);
				// Copy
				auto secret = sharedsecret;

				// If we have a global shared secret, combine it
				// with the shared secret
				if (!global_sharedsecret.IsEmpty())
				{
					secret += global_sharedsecret;

					assert(sharedsecret.GetSize() + global_sharedsecret.GetSize() == secret.GetSize());
				}

				if (Crypto::GenerateSymmetricKeys(secret, *key1, *key2))
				{
					// Should have keys
					assert(!key1->Key.IsEmpty() && !key1->AuthKey.IsEmpty());
					assert(!key2->Key.IsEmpty() && !key2->AuthKey.IsEmpty());

					if (pctype == PeerConnectionType::Outbound)
					{
						keypair->EncryptionKey = std::move(key1);
						keypair->DecryptionKey = std::move(key2);
					}
					else
					{
						keypair->EncryptionKey = std::move(key2);
						keypair->DecryptionKey = std::move(key1);
					}

					return true;
				}
			}
			catch (...) {}

			LogErr(L"Could not generate symmetric key-pair");

			return false;
		}

		static Size GetNumBytesProcessedForLatestKeyPair(const SymmetricKeyPairCollection& keypairs) noexcept
		{
			for (std::size_t x = 0; x < keypairs.size(); ++x)
			{
				if (keypairs[x]->UseForEncryption && keypairs[x]->UseForDecryption)
				{
					return (keypairs[x]->EncryptionKey->NumBytesProcessed +
							keypairs[x]->DecryptionKey->NumBytesProcessed);
				}
			}

			return 0;
		}

		void ExpireAllExceptLatestKeyPair() noexcept
		{
			ExpireAllExceptLatestKeyPair(m_SymmetricKeyPairs);
		}

	private:
		static std::pair<std::shared_ptr<Crypto::SymmetricKeyData>, Buffer> GetAutoGenKeyAndNonce(const UInt32 nonce_seed,
																								  const PeerConnectionType pctype,
																								  bool enc) noexcept
		{
			try
			{
				Algorithms alg{
					// All peers should *at least* support these
					.Hash = Algorithm::Hash::BLAKE2B512,
					.PrimaryAsymmetric = Algorithm::Asymmetric::ECDH_X25519,
					.SecondaryAsymmetric = Algorithm::Asymmetric::ECDH_X448,
					.Symmetric = Algorithm::Symmetric::CHACHA20_POLY1305,
					.Compression = Algorithm::Compression::ZSTANDARD
				};

				auto tempkey1 = std::make_shared<Crypto::SymmetricKeyData>(Crypto::SymmetricKeyType::AutoGen,
																		   alg.Hash, alg.Symmetric,
																		   alg.Compression);
				auto tempkey2 = std::make_shared<Crypto::SymmetricKeyData>(Crypto::SymmetricKeyType::AutoGen,
																		   alg.Hash, alg.Symmetric,
																		   alg.Compression);
				Buffer nonce;
				if (GetNonce(nonce_seed, nonce, alg.Hash))
				{
					// Generate symmetric keys by using the nonce as a "secret"; this is not secure
					// but it only serves the purpose of obfuscating the message data to make it
					// look random for traffic analyzers until we get a better key to work with
					ProtectedBuffer pbuf(nonce.GetBytes(), nonce.GetSize());
					if (Crypto::GenerateSymmetricKeys(pbuf, *tempkey1, *tempkey2))
					{
						std::shared_ptr<Crypto::SymmetricKeyData> symkey;

						if (pctype == PeerConnectionType::Outbound)
						{
							if (enc) symkey = std::move(tempkey1);
							else symkey = std::move(tempkey2);
						}
						else
						{
							if (enc) symkey = std::move(tempkey2);
							else symkey = std::move(tempkey1);
						}

						return make_pair(std::move(symkey), std::move(nonce));
					}
				}
			}
			catch (...) {}

			LogErr(L"Could not generate autogen symmetric key");

			// Should return nullptr for symmetric key to indicate failure
			return std::make_pair(nullptr, Buffer());
		}

		[[nodiscard]] static bool GetNonce(const UInt32 nonce_seed, Buffer& nonce, const Algorithm::Hash ha) noexcept
		{
			const BufferView seedb(reinterpret_cast<const Byte*>(&nonce_seed), sizeof(UInt32));
			if (Crypto::Hash(seedb, nonce, ha))
			{
				Dbg(L"Nonce: %u bytes - %s", nonce.GetSize(), Util::ToBase64(nonce)->c_str());
				return true;
			}

			LogErr(L"Could not generate nonce");

			return false;
		}

		static void ExpireAllExceptLatestKeyPair(SymmetricKeyPairCollection& keypairs) noexcept
		{
			if (keypairs.size() <= 1) return;

			for (std::size_t x = 1; x < keypairs.size(); ++x)
			{
				if (!keypairs[x]->ExpirationSteadyTime.has_value())
				{
					keypairs[x]->ExpirationSteadyTime = Util::GetCurrentSteadyTime();
				}
			}
		}

	private:
		// Maximum number of key-pairs to keep in collection
		static const constexpr Size MaxNumSymmetricKeyPairs{ 4 };

		SymmetricKeyPairCollection m_SymmetricKeyPairs;
	};
}
