// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Common\ScopeGuard.h"

#include <openssl/evp.h>

namespace QuantumGate::Implementation::Crypto
{
	class OpenSSLSign final
	{
	private:
		OpenSSLSign() noexcept = default;

	public:
		[[nodiscard]] static bool Sign(const BufferView& msg, const Algorithm::Asymmetric alg,
									   const BufferView& priv_key, Buffer& sig) noexcept
		{
			switch (alg)
			{
				case Algorithm::Asymmetric::EDDSA_ED25519:
				case Algorithm::Asymmetric::EDDSA_ED448:
					return SignWithRawKey(msg, alg, priv_key, sig);
				default:
					assert(false);
					break;
			}

			return false;
		}

		[[nodiscard]] static bool Verify(const BufferView& msg, const Algorithm::Asymmetric alg,
										 const BufferView& pub_key, const BufferView& sig) noexcept
		{
			switch (alg)
			{
				case Algorithm::Asymmetric::EDDSA_ED25519:
				case Algorithm::Asymmetric::EDDSA_ED448:
					return VerifyWithRawKey(msg, alg, pub_key, sig);
				default:
					assert(false);
					break;
			}

			return false;
		}

	private:
		[[nodiscard]] static bool SignWithRawKey(const BufferView& msg, const Algorithm::Asymmetric alg,
												 const BufferView& priv_key, Buffer& sig) noexcept
		{
			int id{ 0 };

			switch (alg)
			{
				case Algorithm::Asymmetric::EDDSA_ED25519:
					id = EVP_PKEY_ED25519;
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
				// Docs: https://wiki.openssl.org/index.php/EVP_Signing_and_Verifying

				EVP_PKEY* key = EVP_PKEY_new_raw_private_key(id, nullptr,
															 reinterpret_cast<const UChar*>(priv_key.GetBytes()),
															 priv_key.GetSize());
				if (key != nullptr)
				{
					// Release key when we exit
					const auto sg1 = MakeScopeGuard([&]() noexcept { EVP_PKEY_free(key); });

					auto ctx = EVP_MD_CTX_new();
					if (ctx != nullptr)
					{
						// Release ctx when we exit
						const auto sg2 = MakeScopeGuard([&]() noexcept { EVP_MD_CTX_free(ctx); });

						if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1)
						{
							Size slen{ 0 };

							// Get signature size
							if (EVP_DigestSign(ctx, nullptr, &slen,
											   reinterpret_cast<const UChar*>(msg.GetBytes()), msg.GetSize()) == 1)
							{
								sig.Allocate(slen);

								// Now sign
								if (EVP_DigestSign(ctx, reinterpret_cast<UChar*>(sig.GetBytes()), &slen,
												   reinterpret_cast<const UChar*>(msg.GetBytes()), msg.GetSize()) == 1)
								{
									sig.Resize(slen);

									Dbg(L"Sig: %u bytes - %s",
										sig.GetSize(), Util::ToBase64(sig)->c_str());

									return true;
								}
							}
						}
					}
				}
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] static bool VerifyWithRawKey(const BufferView& msg, const Algorithm::Asymmetric alg,
												   const BufferView& pub_key, const BufferView& sig) noexcept
		{
			int id{ 0 };

			switch (alg)
			{
				case Algorithm::Asymmetric::EDDSA_ED25519:
					id = EVP_PKEY_ED25519;
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
				// Docs: https://wiki.openssl.org/index.php/EVP_Signing_and_Verifying

				EVP_PKEY* key = EVP_PKEY_new_raw_public_key(id, nullptr,
															reinterpret_cast<const UChar*>(pub_key.GetBytes()),
															pub_key.GetSize());
				if (key != nullptr)
				{
					// Release key when we exit
					const auto sg1 = MakeScopeGuard([&]() noexcept { EVP_PKEY_free(key); });

					auto ctx = EVP_MD_CTX_new();
					if (ctx != nullptr)
					{
						// Release ctx when we exit
						const auto sg2 = MakeScopeGuard([&]() noexcept { EVP_MD_CTX_free(ctx); });

						if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1)
						{
							if (EVP_DigestVerify(ctx, reinterpret_cast<const UChar*>(sig.GetBytes()), sig.GetSize(),
												 reinterpret_cast<const UChar*>(msg.GetBytes()), msg.GetSize()) == 1)
							{
								return true;
							}
						}
					}
				}
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] static bool SignWithPEMKey(const BufferView& msg, const BufferView& priv_key,
												 Buffer& sig) noexcept
		{
			try
			{
				// Docs: https://wiki.openssl.org/index.php/EVP_Signing_and_Verifying

				auto buff = BIO_new_mem_buf(priv_key.GetBytes(),
											static_cast<int>(priv_key.GetSize()));
				BIO_set_close(buff, BIO_CLOSE);

				// Release buff when we exit
				const auto sg1 = MakeScopeGuard([&]() noexcept { BIO_free_all(buff); });

				EVP_PKEY* key{ nullptr };

				if (PEM_read_bio_PrivateKey(buff, &key, nullptr, nullptr) != nullptr)
				{
					// Release key when we exit
					const auto sg2 = MakeScopeGuard([&]() noexcept { EVP_PKEY_free(key); });

					auto ctx = EVP_MD_CTX_new();
					if (ctx != nullptr)
					{
						// Release ctx when we exit
						const auto sg3 = MakeScopeGuard([&]() noexcept { EVP_MD_CTX_free(ctx); });

						if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1)
						{
							Size slen{ 0 };

							// Get signature size
							if (EVP_DigestSign(ctx, nullptr, &slen,
											   reinterpret_cast<const UChar*>(msg.GetBytes()), msg.GetSize()) == 1)
							{
								sig.Allocate(slen);

								// Now sign
								if (EVP_DigestSign(ctx, reinterpret_cast<UChar*>(sig.GetBytes()), &slen,
												   reinterpret_cast<const UChar*>(msg.GetBytes()), msg.GetSize()) == 1)
								{
									sig.Resize(slen);

									Dbg(L"Sig: %u bytes - %s",
										sig.GetSize(), Util::ToBase64(sig)->c_str());

									return true;
								}
							}
						}
					}
				}
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] static bool VerifyWithPEMKey(const BufferView& msg, const BufferView& pub_key,
												   const BufferView& sig) noexcept
		{
			try
			{
				// Docs: https://wiki.openssl.org/index.php/EVP_Signing_and_Verifying

				auto buff = BIO_new_mem_buf(pub_key.GetBytes(),
											static_cast<int>(pub_key.GetSize()));
				BIO_set_close(buff, BIO_CLOSE);

				// Release buff when we exit
				const auto sg1 = MakeScopeGuard([&]() noexcept { BIO_free_all(buff); });

				EVP_PKEY* key{ nullptr };

				if (PEM_read_bio_PUBKEY(buff, &key, nullptr, nullptr) != nullptr)
				{
					// Release key when we exit
					const auto sg2 = MakeScopeGuard([&]() noexcept { EVP_PKEY_free(key); });

					auto ctx = EVP_MD_CTX_new();
					if (ctx != nullptr)
					{
						// Release ctx when we exit
						const auto sg3 = MakeScopeGuard([&]() noexcept { EVP_MD_CTX_free(ctx); });

						if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1)
						{
							if (EVP_DigestVerify(ctx, reinterpret_cast<const UChar*>(sig.GetBytes()), sig.GetSize(),
												 reinterpret_cast<const UChar*>(msg.GetBytes()), msg.GetSize()) == 1)
							{
								return true;
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