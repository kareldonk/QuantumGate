// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\QuantumGateCryptoLib\QuantumGateCryptoLib.h"

namespace QuantumGate::Implementation::Crypto
{
	class McEliece final
	{
	private:
		McEliece() noexcept = default;

	public:
		[[nodiscard]] static bool GenerateKey(AsymmetricKeyData& keydata) noexcept
		{
			try
			{
				keydata.LocalPublicKey.Allocate(m_PublicKeySize);
				keydata.LocalPrivateKey.Allocate(m_PrivateKeySize);

				if (crypto_kem_mceliece8192128_keypair(reinterpret_cast<UChar*>(keydata.LocalPublicKey.GetBytes()),
													   reinterpret_cast<UChar*>(keydata.LocalPrivateKey.GetBytes())) == 0)
				{
					return true;
				}
			}
			catch (const std::exception& e)
			{
				LogErr(L"Exception while trying to generate McEliece keypair - %s", Util::ToStringW(e.what()).c_str());
			}

			return false;
		}

		[[nodiscard]] static bool GenerateSharedSecret(AsymmetricKeyData& keydata) noexcept
		{
			try
			{
				switch (keydata.GetOwner())
				{
					case AsymmetricKeyOwner::Bob:
					{
						// Bob needs to encrypt a shared secret
						// with Alice's public key
						keydata.SharedSecret.Allocate(m_SharedSecretSize);
						keydata.EncryptedSharedSecret.Allocate(m_SharedSecretEncryptedSize);

						if (crypto_kem_mceliece8192128_enc(reinterpret_cast<UChar*>(keydata.EncryptedSharedSecret.GetBytes()),
														   reinterpret_cast<UChar*>(keydata.SharedSecret.GetBytes()),
														   reinterpret_cast<UChar*>(keydata.PeerPublicKey.GetBytes())) == 0)
						{
							Dbg(L"\r\nMcEliece (Bob):");
							Dbg(L"PSharedSecret: %u bytes - %s", keydata.SharedSecret.GetSize(),
								Util::ToBase64(keydata.SharedSecret)->c_str());
							Dbg(L"ESharedSecret: %u bytes - %s", keydata.EncryptedSharedSecret.GetSize(),
								Util::ToBase64(keydata.EncryptedSharedSecret)->c_str());
							Dbg(L"\r\n");

							return true;
						}

						break;
					}
					case AsymmetricKeyOwner::Alice:
					{
						// Alice needs to decrypt the shared secret sent
						// by Bob with her private key
						keydata.SharedSecret.Allocate(m_SharedSecretSize);

						if (crypto_kem_mceliece8192128_dec(reinterpret_cast<UChar*>(keydata.SharedSecret.GetBytes()),
														   reinterpret_cast<UChar*>(keydata.EncryptedSharedSecret.GetBytes()),
														   reinterpret_cast<UChar*>(keydata.LocalPrivateKey.GetBytes())) == 0)
						{
							Dbg(L"\r\nMcEliece (Alice):");
							Dbg(L"PSharedSecret: %u bytes - %s", keydata.SharedSecret.GetSize(),
								Util::ToBase64(keydata.SharedSecret)->c_str());
							Dbg(L"ESharedSecret: %u bytes - %s", keydata.EncryptedSharedSecret.GetSize(),
								Util::ToBase64(keydata.EncryptedSharedSecret)->c_str());
							Dbg(L"\r\n");

							return true;
						}

						break;
					}
					default:
					{
						// Shouldn't get here
						assert(false);
						break;
					}
				}
			}
			catch (const std::exception& e)
			{
				LogErr(L"Exception while trying to generate McEliece shared secret - %s", Util::ToStringW(e.what()).c_str());
			}

			return false;
		}

	private:
		inline static Size m_PublicKeySize{ crypto_kem_mceliece8192128_PUBLICKEYBYTES };
		inline static Size m_PrivateKeySize{ crypto_kem_mceliece8192128_SECRETKEYBYTES };
		inline static Size m_SharedSecretSize{ crypto_kem_mceliece8192128_BYTES };
		inline static Size m_SharedSecretEncryptedSize{ crypto_kem_mceliece8192128_CIPHERTEXTBYTES };
	};
}