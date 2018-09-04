// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <openssl/evp.h>

namespace QuantumGate::Implementation::Crypto
{
	// One static thread_local OpenSSLSymmetric object will manage
	// resources for encryption/decryption for efficiency (not
	// having to allocate context memory constantly)
	class OpenSSLSymmetric
	{
	private:
		OpenSSLSymmetric() noexcept
		{
			// One time allocation of context
			m_Context = EVP_CIPHER_CTX_new();
		}

		OpenSSLSymmetric(const OpenSSLSymmetric&) = delete;
		OpenSSLSymmetric(OpenSSLSymmetric&&) = delete;

		~OpenSSLSymmetric()
		{
			// Free resources
			if (m_Context) EVP_CIPHER_CTX_free(m_Context);
		}

		OpenSSLSymmetric& operator=(const OpenSSLSymmetric&) = delete;
		OpenSSLSymmetric& operator=(OpenSSLSymmetric&&) = delete;

		ForceInline static EVP_CIPHER_CTX* GetContext() noexcept
		{
			// Static object for use by the current thread
			// allocated one time for efficiency
			static thread_local OpenSSLSymmetric openssl;
			return openssl.m_Context;
		}

	public:
		[[nodiscard]] static const bool Encrypt(const BufferView& buffer, Buffer& encrbuf,
												const SymmetricKeyData& symkeydata, const BufferView& iv) noexcept
		{
			assert(symkeydata.Key.GetSize() >= 32); // At least 256 bits
			assert(iv.GetSize() >= 12); // At least 96 bits

			const EVP_CIPHER* cipher{ nullptr };

			switch (symkeydata.SymmetricAlgorithm)
			{
				case Algorithm::Symmetric::AES256_GCM:
					cipher = EVP_aes_256_gcm();
					break;
				case Algorithm::Symmetric::CHACHA20_POLY1305:
					cipher = EVP_chacha20_poly1305();
					break;
				default:
					return false;
			}

			// Docs: https://wiki.openssl.org/index.php/EVP_Authenticated_Encryption_and_Decryption
			// https://www.openssl.org/docs/man1.1.0/crypto/EVP_chacha20_poly1305.html

			try
			{
				auto ctx = GetContext();
				if (ctx != nullptr)
				{
					// Initialize the encryption operation
					if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) == 1)
					{
						EVP_CIPHER_CTX_set_padding(ctx, 1);

						// Set IV length (default is 12 bytes (96 bits), but AES supports larger ones)
						if (symkeydata.SymmetricAlgorithm == Algorithm::Symmetric::AES256_GCM)
						{
							if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
													static_cast<int>(iv.GetSize()), nullptr) != 1) return false;
						}

						// Initialize key and IV
						if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
											   reinterpret_cast<const UChar*>(symkeydata.Key.GetBytes()),
											   reinterpret_cast<const UChar*>(iv.GetBytes())) == 1)
						{
							Size encrlen{ 0 };
							Size len{ 0 };
							Size taglen{ 16 };

							encrbuf.Allocate(taglen + buffer.GetSize() + EVP_CIPHER_CTX_block_size(ctx));

							// Provide the message to be encrypted, and obtain the encrypted output
							if (EVP_EncryptUpdate(ctx, reinterpret_cast<UChar*>(encrbuf.GetBytes()) + taglen,
												  reinterpret_cast<int*>(&len), reinterpret_cast<const UChar*>(buffer.GetBytes()),
												  static_cast<int>(buffer.GetSize())) == 1)
							{
								encrlen = len;
								len = 0;

								// Finalize the encryption
								if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<UChar*>(encrbuf.GetBytes()) + taglen + encrlen,
														reinterpret_cast<int*>(&len)) == 1)
								{
									encrlen += len;

									assert(encrlen <= (encrbuf.GetSize() - taglen));

									// Get the tag (16 bytes (128 bits))
									if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG,
															static_cast<int>(taglen), encrbuf.GetBytes()) == 1)
									{
										DbgInvoke([&]()
										{
											const auto tag = BufferView(encrbuf).GetFirst(taglen);

											Dbg(L"Etag: %s", Util::GetBase64(tag)->c_str());
											Dbg(L"Encr: %s", Util::GetBase64(encrbuf)->c_str());
										});

										encrbuf.Resize(taglen + encrlen);
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

		[[nodiscard]] static const bool Decrypt(const BufferView& encrbuf, Buffer& buffer,
												const SymmetricKeyData& symkeydata, const BufferView& iv) noexcept
		{
			assert(symkeydata.Key.GetSize() >= 32); // At least 256 bits
			assert(iv.GetSize() >= 12); // At least 96 bits

			const EVP_CIPHER* cipher{ nullptr };

			switch (symkeydata.SymmetricAlgorithm)
			{
				case Algorithm::Symmetric::AES256_GCM:
					cipher = EVP_aes_256_gcm();
					break;
				case Algorithm::Symmetric::CHACHA20_POLY1305:
					cipher = EVP_chacha20_poly1305();
					break;
				default:
					return false;
			}

			// Docs: https://wiki.openssl.org/index.php/EVP_Authenticated_Encryption_and_Decryption
			// https://www.openssl.org/docs/man1.1.0/crypto/EVP_chacha20_poly1305.html

			try
			{
				auto ctx = GetContext();
				if (ctx != nullptr)
				{
					// Initialize the decryption operation
					if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) == 1)
					{
						EVP_CIPHER_CTX_set_padding(ctx, 1);

						// Set IV length (default is 12 bytes (96 bits))
						if (symkeydata.SymmetricAlgorithm == Algorithm::Symmetric::AES256_GCM)
						{
							if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
													static_cast<int>(iv.GetSize()), nullptr) != 1) return false;
						}

						// Initialize key and IV
						if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
											   reinterpret_cast<const UChar*>(symkeydata.Key.GetBytes()),
											   reinterpret_cast<const UChar*>(iv.GetBytes())) == 1)
						{
							buffer.Allocate(encrbuf.GetSize());
							Size declen{ 0 };
							Size len{ 0 };
							const Size taglen{ 16 };

							// Provide the message to be decrypted, and obtain the plaintext output
							if (EVP_DecryptUpdate(ctx, reinterpret_cast<UChar*>(buffer.GetBytes()), reinterpret_cast<int*>(&len),
												  reinterpret_cast<const UChar*>(encrbuf.GetBytes()) + taglen,
												  static_cast<int>(encrbuf.GetSize() - taglen)) == 1)
							{
								declen = len;

								DbgInvoke([&]()
								{
									const auto tag = BufferView(encrbuf).GetFirst(taglen);

									Dbg(L"Dtag: %s", Util::GetBase64(tag)->c_str());
									Dbg(L"Decr: %s", Util::GetBase64(encrbuf)->c_str());
								});

								// Set expected tag value
								if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
														static_cast<int>(taglen), const_cast<Byte*>(encrbuf.GetBytes())) == 1)
								{
									len = 0;

									// Finalize the decryption; a positive return value indicates success,
									// anything else is a failure - the plaintext is not trustworthy
									const auto ret = EVP_DecryptFinal_ex(ctx, reinterpret_cast<UChar*>(buffer.GetBytes()) + declen,
																		 reinterpret_cast<int*>(&len));
									if (ret > 0)
									{
										declen += len;
										buffer.Resize(declen);
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

	private:
		EVP_CIPHER_CTX * m_Context{ nullptr };
	};
}