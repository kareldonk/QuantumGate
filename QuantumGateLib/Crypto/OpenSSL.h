// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Common\ScopeGuard.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/ec.h>
#include <openssl/pem.h>

namespace QuantumGate::Implementation::Crypto
{
	class OpenSSL final
	{
	private:
		OpenSSL() noexcept = default;

	public:
		[[nodiscard]] static bool GetRandomBytes(const UInt seed, Byte* buffer, const Size len) noexcept
		{
			assert(buffer != nullptr);

			// Seed OpenSSL
			RAND_seed(&seed, sizeof(seed));

			return (RAND_bytes(reinterpret_cast<UChar*>(buffer), static_cast<int>(len)) == 1);
		}

		template<typename T>
		[[nodiscard]] static bool Hash(const BufferView& buffer, T& hashbuf, const Algorithm::Hash type) noexcept
		{
			try
			{
				auto context = EVP_MD_CTX_create();
				if (context != nullptr)
				{
					// Release context when we exit
					const auto sg = MakeScopeGuard([&]() noexcept { EVP_MD_CTX_destroy(context); });

					const EVP_MD* md = nullptr;

					switch (type)
					{
						case Algorithm::Hash::SHA256:
							md = EVP_sha256();
							break;
						case Algorithm::Hash::SHA512:
							md = EVP_sha512();
							break;
						case Algorithm::Hash::BLAKE2S256:
							md = EVP_blake2s256();
							break;
						case Algorithm::Hash::BLAKE2B512:
							md = EVP_blake2b512();
							break;
						default:
							return false;
					}

					if (EVP_DigestInit_ex(context, md, nullptr))
					{
						// Calculate hash
						if (EVP_DigestUpdate(context, buffer.GetBytes(), buffer.GetSize()))
						{
							std::array<Byte, EVP_MAX_MD_SIZE> hash{ Byte{ 0 } };
							UInt hlen{ EVP_MAX_MD_SIZE };

							// Finalize and get hash and final length back
							if (EVP_DigestFinal_ex(context, reinterpret_cast<UChar*>(hash.data()), &hlen))
							{
								hashbuf.Resize(hlen);
								std::memcpy(hashbuf.GetBytes(), hash.data(), hlen);
								return true;
							}
						}
					}
				}
			}
			catch (...) {}

			return false;
		}

		template<typename T>
		[[nodiscard]] static bool HMAC(const BufferView& buffer, T& hmac, const BufferView& key,
									   const Algorithm::Hash type) noexcept
		{
			try
			{
				const EVP_MD* md{ nullptr };

				switch (type)
				{
					case Algorithm::Hash::SHA256:
						md = EVP_sha256();
						break;
					case Algorithm::Hash::SHA512:
						md = EVP_sha512();
						break;
					case Algorithm::Hash::BLAKE2S256:
						md = EVP_blake2s256();
						break;
					case Algorithm::Hash::BLAKE2B512:
						md = EVP_blake2b512();
						break;
					default:
						return false;
				}

				std::array<Byte, EVP_MAX_MD_SIZE> digest{ Byte{ 0 } };
				UInt dlen{ EVP_MAX_MD_SIZE };

				if (::HMAC(md, key.GetBytes(), static_cast<int>(key.GetSize()), reinterpret_cast<const UChar*>(buffer.GetBytes()),
						   static_cast<int>(buffer.GetSize()), reinterpret_cast<UChar*>(digest.data()), &dlen) != nullptr)
				{
					hmac.Resize(dlen);
					std::memcpy(hmac.GetBytes(), digest.data(), dlen);

					return true;
				}
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] static bool HKDF(const BufferView& secret, ProtectedBuffer& outkey, const Size outkeylen,
									   const Algorithm::Hash type) noexcept
		{
			try
			{
				auto pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
				if (pctx != nullptr)
				{
					// Release pctx when we exit
					const auto sg = MakeScopeGuard([&]() noexcept { EVP_PKEY_CTX_free(pctx); });

					const EVP_MD* md{ nullptr };

					switch (type)
					{
						case Algorithm::Hash::SHA256:
							md = EVP_sha256();
							break;
						case Algorithm::Hash::SHA512:
							md = EVP_sha512();
							break;
						case Algorithm::Hash::BLAKE2S256:
							md = EVP_blake2s256();
							break;
						case Algorithm::Hash::BLAKE2B512:
							md = EVP_blake2b512();
							break;
						default:
							return false;
					}

					constexpr std::array salt{ 'q', 'g', 'k', 'e', 'y', 's', 'a', 'l', 't' };
					constexpr std::array label{ 'q', 'g', 'k', 'e', 'y', 'l', 'a', 'b', 'e', 'l'};

					if (EVP_PKEY_derive_init(pctx) == 1 &&
						EVP_PKEY_CTX_set_hkdf_md(pctx, md) == 1 &&
						EVP_PKEY_CTX_set1_hkdf_salt(pctx, reinterpret_cast<const UChar*>(salt.data()), static_cast<int>(salt.size())) == 1 &&
						EVP_PKEY_CTX_set1_hkdf_key(pctx, reinterpret_cast<const UChar*>(secret.GetBytes()), static_cast<int>(secret.GetSize())) == 1 &&
						EVP_PKEY_CTX_add1_hkdf_info(pctx, reinterpret_cast<const UChar*>(label.data()), static_cast<int>(label.size())) == 1)
					{
						outkey.Allocate(outkeylen);
						Size len = outkeylen;

						if (EVP_PKEY_derive(pctx, reinterpret_cast<UChar*>(outkey.GetBytes()), &len) == 1)
						{
							outkey.Resize(len);

							Dbg(L"HKDF: %u bytes - %s", outkey.GetSize(), Util::ToBase64(outkey)->c_str());

							return true;
						}
					}
				}
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] static bool GenerateKey(AsymmetricKeyData& keydata) noexcept
		{
			switch (keydata.GetAlgorithm())
			{
				case Algorithm::Asymmetric::ECDH_SECP521R1:
					return GenerateKeyWithParam(keydata);
				case Algorithm::Asymmetric::ECDH_X25519:
				case Algorithm::Asymmetric::EDDSA_ED25519:
				case Algorithm::Asymmetric::ECDH_X448:
				case Algorithm::Asymmetric::EDDSA_ED448:
					return GenerateKeyNoParam(keydata);
				default:
					// Shouldn't get here
					assert(false);
					break;
			}

			return false;
		}

		[[nodiscard]] static bool GenerateSharedSecret(AsymmetricKeyData& keydata) noexcept
		{
			switch (keydata.GetAlgorithm())
			{
				case Algorithm::Asymmetric::ECDH_SECP521R1:
					return GenerateSharedSecretWithPEMKeys(keydata);
				case Algorithm::Asymmetric::ECDH_X25519:
				case Algorithm::Asymmetric::EDDSA_ED25519:
				case Algorithm::Asymmetric::ECDH_X448:
				case Algorithm::Asymmetric::EDDSA_ED448:
					return GenerateSharedSecretWithRawKeys(keydata);
				default:
					assert(false);
					break;
			}

			return false;
		}

		static std::optional<ProtectedBuffer> GetRawPublicKey(EVP_PKEY* key) noexcept
		{
			assert(key != nullptr);

			try
			{
				Size pkey_len{ 0 };

				// First get required buffer length
				if (EVP_PKEY_get_raw_public_key(key, nullptr, &pkey_len) == 1)
				{
					ProtectedBuffer pkey;
					pkey.Allocate(pkey_len);

					// Now get the key data
					if (EVP_PKEY_get_raw_public_key(key, reinterpret_cast<UChar*>(pkey.GetBytes()), &pkey_len) == 1)
					{
						pkey.Resize(pkey_len);

						Dbg(L"Pubkey: %u bytes - %s", pkey.GetSize(), Util::ToBase64(pkey)->c_str());

						return { std::move(pkey) };
					}
				}
			}
			catch (...) {}

			return std::nullopt;
		}

		static std::optional<ProtectedBuffer> GetRawPrivateKey(EVP_PKEY* key) noexcept
		{
			assert(key != nullptr);

			try
			{
				Size pkey_len{ 0 };

				// First get required buffer length
				if (EVP_PKEY_get_raw_private_key(key, nullptr, &pkey_len) == 1)
				{
					ProtectedBuffer pkey;
					pkey.Allocate(pkey_len);

					// Now get the key data
					if (EVP_PKEY_get_raw_private_key(key, reinterpret_cast<UChar*>(pkey.GetBytes()), &pkey_len) == 1)
					{
						pkey.Resize(pkey_len);

						Dbg(L"Privkey: %u bytes - %s", pkey.GetSize(), Util::ToBase64(pkey)->c_str());

						return { std::move(pkey) };
					}
				}
			}
			catch (...) {}

			return std::nullopt;
		}

		static std::optional<ProtectedBuffer> GetPEMPublicKey(EVP_PKEY* key) noexcept
		{
			assert(key != nullptr);

			try
			{
				auto buff = BIO_new(BIO_s_mem());
				BIO_set_close(buff, BIO_CLOSE);

				// Release buff when we exit
				const auto sg = MakeScopeGuard([&]() noexcept { BIO_free_all(buff); });

				if (PEM_write_bio_PUBKEY(buff, key) == 1)
				{
					BIO_flush(buff);

					BUF_MEM* ptr{ nullptr };
					BIO_get_mem_ptr(buff, &ptr);
					if (ptr != nullptr)
					{
						ProtectedBuffer pkey;
						pkey.Allocate(ptr->length);
						memcpy(pkey.GetBytes(), ptr->data, ptr->length);

						Dbg(L"Pubkey: %u bytes - %s", pkey.GetSize(), Util::ToBase64(pkey)->c_str());

						return { std::move(pkey) };
					}
				}
			}
			catch (...) {}

			return std::nullopt;
		}

		static std::optional<ProtectedBuffer> GetPEMPrivateKey(EVP_PKEY* key) noexcept
		{
			assert(key != nullptr);

			try
			{
				auto buff = BIO_new(BIO_s_mem());
				BIO_set_close(buff, BIO_CLOSE);

				// Release buff when we exit
				const auto sg = MakeScopeGuard([&]() noexcept { BIO_free_all(buff); });

				if (PEM_write_bio_PrivateKey(buff, key, nullptr,
											 nullptr, 0, nullptr, nullptr) == 1)
				{
					BIO_flush(buff);

					BUF_MEM* ptr{ nullptr };
					BIO_get_mem_ptr(buff, &ptr);
					if (ptr != nullptr)
					{
						ProtectedBuffer pkey;
						pkey.Allocate(ptr->length);
						memcpy(pkey.GetBytes(), ptr->data, ptr->length);

						Dbg(L"Privkey: %u bytes - %s", pkey.GetSize(), Util::ToBase64(pkey)->c_str());

						return { std::move(pkey) };
					}
				}
			}
			catch (...) {}

			return std::nullopt;
		}

	private:
		[[nodiscard]] static bool GenerateKeyWithParam(AsymmetricKeyData& keydata) noexcept
		{
			int nid{ 0 };

			switch (keydata.GetAlgorithm())
			{
				case Algorithm::Asymmetric::ECDH_SECP521R1:
					nid = NID_secp521r1;
					break;
				default:
					assert(false);
					return false;
			}

			// Docs: https://wiki.openssl.org/index.php/Elliptic_Curve_Diffie_Hellman

			// Create the context for parameter generation
			auto pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
			if (pctx != nullptr)
			{
				// Release pctx when we exit
				const auto sg1 = MakeScopeGuard([&]() noexcept { EVP_PKEY_CTX_free(pctx); });

				// Initialise the parameter generation
				if (EVP_PKEY_paramgen_init(pctx) == 1 && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, nid) == 1)
				{
					EVP_PKEY* params{ nullptr };

					// Create the parameter object params
					if (EVP_PKEY_paramgen(pctx, &params))
					{
						// Release params when we exit
						const auto sg2 = MakeScopeGuard([&]() noexcept { EVP_PKEY_free(params); });

						// Create the context for the key generation
						auto kctx = EVP_PKEY_CTX_new(params, nullptr);
						if (kctx != nullptr)
						{
							// Release kctx when we exit
							const auto sg3 = MakeScopeGuard([&]() noexcept { EVP_PKEY_CTX_free(kctx); });

							EVP_PKEY* key{ nullptr };

							// Generate the key
							if (EVP_PKEY_keygen_init(kctx) == 1 && EVP_PKEY_keygen(kctx, &key) == 1)
							{
								keydata.SetKey(key);

								auto priv_key = GetPEMPrivateKey(key);
								if (priv_key.has_value())
								{
									keydata.LocalPrivateKey = std::move(*priv_key);

									auto pub_key = GetPEMPublicKey(key);
									if (pub_key.has_value())
									{
										keydata.LocalPublicKey = std::move(*pub_key);
										return true;
									}
								}
							}
						}
					}
				}
			}

			return false;
		}

		[[nodiscard]] static bool GenerateKeyNoParam(AsymmetricKeyData& keydata) noexcept
		{
			int id{ 0 };

			switch (keydata.GetAlgorithm())
			{
				case Algorithm::Asymmetric::ECDH_X25519:
					id = EVP_PKEY_X25519;
					break;
				case Algorithm::Asymmetric::EDDSA_ED25519:
					id = EVP_PKEY_ED25519;
					break;
				case Algorithm::Asymmetric::ECDH_X448:
					id = EVP_PKEY_X448;
					break;
				case Algorithm::Asymmetric::EDDSA_ED448:
					id = EVP_PKEY_ED448;
					break;
				default:
					assert(false);
					return false;
			}

			// Create the context for parameter generation
			auto pctx = EVP_PKEY_CTX_new_id(id, nullptr);
			if (pctx != nullptr)
			{
				// Release pctx when we exit
				const auto sg = MakeScopeGuard([&]() noexcept { EVP_PKEY_CTX_free(pctx); });

				EVP_PKEY* key{ nullptr };

				// Generate the key
				if (EVP_PKEY_keygen_init(pctx) == 1 && EVP_PKEY_keygen(pctx, &key) == 1)
				{
					keydata.SetKey(key);

					auto priv_key = GetRawPrivateKey(key);
					if (priv_key.has_value())
					{
						keydata.LocalPrivateKey = std::move(*priv_key);

						auto pub_key = GetRawPublicKey(key);
						if (pub_key.has_value())
						{
							keydata.LocalPublicKey = std::move(*pub_key);
							return true;
						}
					}
				}
			}

			return false;
		}

		[[nodiscard]] static bool GenerateSharedSecretWithPEMKeys(AsymmetricKeyData& keydata) noexcept
		{
			// Docs: https://wiki.openssl.org/index.php/Elliptic_Curve_Diffie_Hellman

			try
			{
				// Create the context for the shared secret derivation
				auto ctx = EVP_PKEY_CTX_new(static_cast<EVP_PKEY*>(keydata.GetKey()), nullptr);
				if (ctx != nullptr)
				{
					// Release ctx when we exit
					const auto sg1 = MakeScopeGuard([&]() noexcept { EVP_PKEY_CTX_free(ctx); });

					// Initialize
					if (EVP_PKEY_derive_init(ctx) == 1)
					{
						auto buff = BIO_new_mem_buf(keydata.PeerPublicKey.GetBytes(),
													static_cast<int>(keydata.PeerPublicKey.GetSize()));
						BIO_set_close(buff, BIO_CLOSE);

						// Release buff when we exit
						const auto sg2 = MakeScopeGuard([&]() noexcept { BIO_free_all(buff); });

						EVP_PKEY* peerkey{ nullptr };

						if (PEM_read_bio_PUBKEY(buff, &peerkey, nullptr, nullptr) != nullptr)
						{
							// Release peerkey when we exit
							const auto sg3 = MakeScopeGuard([&]() noexcept { EVP_PKEY_free(peerkey); });

							// Provide the peer public key
							if (EVP_PKEY_derive_set_peer(ctx, peerkey))
							{
								size_t klen{ 0 };

								// Determine buffer length for shared secret
								if (EVP_PKEY_derive(ctx, nullptr, &klen) == 1)
								{
									keydata.SharedSecret.Allocate(klen);

									// Derive the shared secret
									if (EVP_PKEY_derive(ctx, reinterpret_cast<UChar*>(keydata.SharedSecret.GetBytes()), &klen) == 1)
									{
										keydata.SharedSecret.Resize(klen);
										return true;
									}
								}
							}
						}
					}
				}
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] static bool GenerateSharedSecretWithRawKeys(AsymmetricKeyData& keydata) noexcept
		{
			// Docs: https://wiki.openssl.org/index.php/Elliptic_Curve_Diffie_Hellman

			int id{ 0 };

			switch (keydata.GetAlgorithm())
			{
				case Algorithm::Asymmetric::ECDH_X25519:
					id = EVP_PKEY_X25519;
					break;
				case Algorithm::Asymmetric::EDDSA_ED25519:
					id = EVP_PKEY_ED25519;
					break;
				case Algorithm::Asymmetric::ECDH_X448:
					id = EVP_PKEY_X448;
					break;
				case Algorithm::Asymmetric::EDDSA_ED448:
					id = EVP_PKEY_ED448;
					break;
				default:
					assert(false);
					return false;
			}

			try
			{
				// Create the context for the shared secret derivation
				auto ctx = EVP_PKEY_CTX_new(static_cast<EVP_PKEY*>(keydata.GetKey()), nullptr);
				if (ctx != nullptr)
				{
					// Release ctx when we exit
					const auto sg1 = MakeScopeGuard([&]() noexcept { EVP_PKEY_CTX_free(ctx); });

					// Initialize
					if (EVP_PKEY_derive_init(ctx) == 1)
					{
						EVP_PKEY* peerkey = EVP_PKEY_new_raw_public_key(id, nullptr,
																		reinterpret_cast<const UChar*>(keydata.PeerPublicKey.GetBytes()),
																		keydata.PeerPublicKey.GetSize());
						if (peerkey != nullptr)
						{
							// Release peerkey when we exit
							const auto sg2 = MakeScopeGuard([&]() noexcept { EVP_PKEY_free(peerkey); });

							// Provide the peer public key
							if (EVP_PKEY_derive_set_peer(ctx, peerkey))
							{
								size_t klen{ 0 };

								// Determine buffer length for shared secret
								if (EVP_PKEY_derive(ctx, nullptr, &klen) == 1)
								{
									keydata.SharedSecret.Allocate(klen);

									// Derive the shared secret
									if (EVP_PKEY_derive(ctx, reinterpret_cast<UChar*>(keydata.SharedSecret.GetBytes()), &klen) == 1)
									{
										keydata.SharedSecret.Resize(klen);
										return true;
									}
								}
							}
						}
					}
				}
			}
			catch (...) {}

			return false;
		}
	};
}