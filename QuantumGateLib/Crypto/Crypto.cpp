// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Crypto.h"
#include "OpenSSL.h"
#include "OpenSSLSymmetric.h"
#include "OpenSSLSign.h"
#include "McEliece.h"
#include "NTRUPrime.h"
#include "NewHope.h"
#include "..\Common\Random.h"
#include "..\Common\ScopeGuard.h"
#include "..\..\QuantumGateCryptoLib\QuantumGateCryptoLib.h"

namespace QuantumGate::Implementation::Crypto
{
	const WChar* GetAlgorithmName(const Algorithm::Asymmetric alg) noexcept
	{
		switch (alg)
		{
			case Algorithm::Asymmetric::ECDH_SECP521R1:
				return Algorithm::AsymmetricAlgorithmName::ECDH_SECP521R1;
			case Algorithm::Asymmetric::ECDH_X25519:
				return Algorithm::AsymmetricAlgorithmName::ECDH_X25519;
			case Algorithm::Asymmetric::ECDH_X448:
				return Algorithm::AsymmetricAlgorithmName::ECDH_X448;
			case Algorithm::Asymmetric::KEM_CLASSIC_MCELIECE:
				return Algorithm::AsymmetricAlgorithmName::KEM_CLASSIC_MCELIECE;
			case Algorithm::Asymmetric::KEM_NTRUPRIME:
				return Algorithm::AsymmetricAlgorithmName::KEM_NTRUPRIME;
			case Algorithm::Asymmetric::KEM_NEWHOPE:
				return Algorithm::AsymmetricAlgorithmName::KEM_NEWHOPE;
			case Algorithm::Asymmetric::EDDSA_ED25519:
				return Algorithm::AsymmetricAlgorithmName::EDDSA_ED25519;
			case Algorithm::Asymmetric::EDDSA_ED448:
				return Algorithm::AsymmetricAlgorithmName::EDDSA_ED448;
			default:
				assert(false);
				break;
		}

		return L"Unknown";
	}

	const WChar* GetAlgorithmName(const Algorithm::Symmetric alg) noexcept
	{
		switch (alg)
		{
			case Algorithm::Symmetric::AES256_GCM:
				return Algorithm::SymmetricAlgorithmName::AES256_GCM;
			case Algorithm::Symmetric::CHACHA20_POLY1305:
				return Algorithm::SymmetricAlgorithmName::CHACHA20_POLY1305;
			default:
				assert(false);
				break;
		}

		return L"Unknown";
	}

	const WChar* GetAlgorithmName(const Algorithm::Hash alg) noexcept
	{
		switch (alg)
		{
			case Algorithm::Hash::SHA256:
				return Algorithm::HashAlgorithmName::SHA256;
			case Algorithm::Hash::SHA512:
				return Algorithm::HashAlgorithmName::SHA512;
			case Algorithm::Hash::BLAKE2S256:
				return Algorithm::HashAlgorithmName::BLAKE2S256;
			case Algorithm::Hash::BLAKE2B512:
				return Algorithm::HashAlgorithmName::BLAKE2B512;
			default:
				assert(false);
				break;
		}

		return L"Unknown";
	}

	const WChar* GetAlgorithmName(const Algorithm::Compression alg) noexcept
	{
		switch (alg)
		{
			case Algorithm::Compression::DEFLATE:
				return Algorithm::CompressionAlgorithmName::DEFLATE;
			case Algorithm::Compression::ZSTANDARD:
				return Algorithm::CompressionAlgorithmName::ZSTANDARD;
			default:
				assert(false);
				break;
		}

		return L"Unknown";
	}

	template<typename T>
	void SortAlgorithms(Vector<T>& list)
	{
		std::sort(list.begin(), list.end());
	}

	// Specific instantiations
	template void SortAlgorithms<Algorithm::Hash>(Vector<Algorithm::Hash>& list);

	template void SortAlgorithms<Algorithm::Asymmetric>(Vector<Algorithm::Asymmetric>& list);

	template void SortAlgorithms<Algorithm::Symmetric>(Vector<Algorithm::Symmetric>& list);

	template void SortAlgorithms<Algorithm::Compression>(Vector<Algorithm::Compression>& list);

	template<typename T>
	Export bool HasAlgorithm(const Vector<T>& list, const T value)
	{
		// Assuming list is sorted already
		assert(std::is_sorted(list.begin(), list.end()));

		// Reverse lookup, assuming that since the algorithm with the highest
		// integer value is chosen it would exist at the back of the sorted list
		// and be found sooner this way
		return (std::find(list.rbegin(), list.rend(), value) != list.rend());
	}

	// Specific instantiations
	template Export bool HasAlgorithm<Algorithm::Hash>(
		const Vector<Algorithm::Hash>& list, const Algorithm::Hash value);

	template Export bool HasAlgorithm<Algorithm::Asymmetric>(
		const Vector<Algorithm::Asymmetric>& list, const Algorithm::Asymmetric value);

	template Export bool HasAlgorithm<Algorithm::Symmetric>(
		const Vector<Algorithm::Symmetric>& list, const Algorithm::Symmetric value);

	template Export bool HasAlgorithm<Algorithm::Compression>(
		const Vector<Algorithm::Compression>& list, const Algorithm::Compression value);

	template<typename T>
	const T ChooseAlgorithm(const Vector<T>& list1, Vector<T>& list2)
	{
		// Assuming list1 is sorted already
		assert(std::is_sorted(list1.begin(), list1.end()));

		// Sort list2 and make an intersection
		// of algorithms that exist in both lists
		SortAlgorithms(list2);

		Vector<T> intersect;
		std::set_intersection(list1.begin(), list1.end(),
							  list2.begin(), list2.end(),
							  std::back_inserter(intersect));

		// Always choose the last algorithm which is the one
		// with the highest integer value in the sorted list
		if (intersect.size() > 0) return intersect.back();

		return static_cast<T>(0);
	}

	// Specific instantiations
	template const Algorithm::Hash ChooseAlgorithm<Algorithm::Hash>(
		const Vector<Algorithm::Hash>& list1, Vector<Algorithm::Hash>& list2);

	template const Algorithm::Asymmetric ChooseAlgorithm<Algorithm::Asymmetric>(
		const Vector<Algorithm::Asymmetric>& list1, Vector<Algorithm::Asymmetric>& list2);

	template const Algorithm::Symmetric ChooseAlgorithm<Algorithm::Symmetric>(
		const Vector<Algorithm::Symmetric>& list1, Vector<Algorithm::Symmetric>& list2);

	template const Algorithm::Compression ChooseAlgorithm<Algorithm::Compression>(
		const Vector<Algorithm::Compression>& list1, Vector<Algorithm::Compression>& list2);

	std::optional<UInt64> GetCryptoRandomNumber() noexcept
	{
		UInt64 num{ 0 };
		if (QGCryptoGetRandomBytes(reinterpret_cast<UChar*>(&num), sizeof(num)) == 0)
		{
			return { num };
		}
		
		return std::nullopt;
	}

	std::optional<Buffer> GetCryptoRandomBytes(const Size size) noexcept
	{
		try
		{
			Buffer bytes(size);

			if (QGCryptoGetRandomBytes(reinterpret_cast<UChar*>(bytes.GetBytes()),
									   gsl::narrow<ULong>(bytes.GetSize())) == 0)
			{
				return { std::move(bytes) };
			}
		}
		catch (...) {}

		return std::nullopt;
	}

	template<typename T>
	Export bool Hash(const BufferView& buffer, T& hashbuf, const Algorithm::Hash type) noexcept
	{
		return OpenSSL::Hash(buffer, hashbuf, type);
	}

	// Specific instantiations
	template Export bool Hash<Buffer>(const BufferView& buffer, Buffer& hashbuf,
									  const Algorithm::Hash type) noexcept;
	template Export bool Hash<ProtectedBuffer>(const BufferView& buffer, ProtectedBuffer& hashbuf,
											   const Algorithm::Hash type) noexcept;

	template<typename T>
	bool HMAC(const BufferView& buffer, T& hmac, const BufferView& key, const Algorithm::Hash type) noexcept
	{
		return OpenSSL::HMAC(buffer, hmac, key, type);
	}

	// Specific instantiations
	template bool HMAC<Buffer>(const BufferView& buffer, Buffer& hmac,
							   const BufferView& key, const Algorithm::Hash type) noexcept;
	template bool HMAC<ProtectedBuffer>(const BufferView& buffer, ProtectedBuffer& hmac,
										const BufferView& key, const Algorithm::Hash type) noexcept;

	Export bool HKDF(const BufferView& secret, ProtectedBuffer& outkey, const Size outkeylen,
					 const Algorithm::Hash type) noexcept
	{
		if (ValidateBuffer(secret))
		{
			if (OpenSSL::HKDF(secret, outkey, outkeylen, type))
			{
				return ValidateBuffer(outkey);
			}
		}

		return false;
	}

	bool GenerateAsymmetricKeys(AsymmetricKeyData& keydata) noexcept
	{
		// Should have algorithm
		assert(keydata.GetAlgorithm() != Algorithm::Asymmetric::Unknown);

		switch (keydata.GetAlgorithm())
		{
			case Algorithm::Asymmetric::KEM_CLASSIC_MCELIECE:
			{
				if (McEliece::GenerateKey(keydata))
				{
					return ValidateBuffer(keydata.LocalPublicKey);
				}
				break;
			}
			case Algorithm::Asymmetric::KEM_NTRUPRIME:
			{
				if (NTRUPrime::GenerateKey(keydata))
				{
					return ValidateBuffer(keydata.LocalPublicKey);
				}
				break;
			}
			case Algorithm::Asymmetric::KEM_NEWHOPE:
			{
				if (NewHope::GenerateKey(keydata))
				{
					return ValidateBuffer(keydata.LocalPublicKey);
				}
				break;
			}
			default:
			{
				if (OpenSSL::GenerateKey(keydata))
				{
					return ValidateBuffer(keydata.LocalPublicKey);
				}
				break;
			}
		}

		return false;
	}

	bool GenerateSharedSecret(AsymmetricKeyData& keydata) noexcept
	{
		// Should have algorithm and owner
		assert(keydata.GetAlgorithm() != Algorithm::Asymmetric::Unknown &&
			   keydata.GetOwner() != AsymmetricKeyOwner::Unknown);

		if (keydata.GetKeyExchangeType() == KeyExchangeType::KeyEncapsulation)
		{
			switch (keydata.GetOwner())
			{
				case AsymmetricKeyOwner::Bob:
				{
					assert(!keydata.PeerPublicKey.IsEmpty());

					if (!ValidateBuffer(keydata.PeerPublicKey)) return false;
					break;
				}
				case AsymmetricKeyOwner::Alice:
				{
					assert(!keydata.LocalPrivateKey.IsEmpty() && !keydata.EncryptedSharedSecret.IsEmpty());

					if (!ValidateBuffer(keydata.LocalPrivateKey) ||
						!ValidateBuffer(keydata.EncryptedSharedSecret)) return false;
					break;
				}
				default:
				{
					// Shouldn't get here
					assert(false);
					return false;
				}
			}

			switch (keydata.GetAlgorithm())
			{
				case Algorithm::Asymmetric::KEM_CLASSIC_MCELIECE:
				{
					if (McEliece::GenerateSharedSecret(keydata))
					{
						return ValidateBuffer(keydata.SharedSecret);
					}
					break;
				}
				case Algorithm::Asymmetric::KEM_NTRUPRIME:
				{
					if (NTRUPrime::GenerateSharedSecret(keydata))
					{
						return ValidateBuffer(keydata.SharedSecret);
					}
					break;
				}
				case Algorithm::Asymmetric::KEM_NEWHOPE:
				{
					if (NewHope::GenerateSharedSecret(keydata))
					{
						return ValidateBuffer(keydata.SharedSecret);
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
		else if (keydata.GetKeyExchangeType() == KeyExchangeType::DiffieHellman)
		{
			assert(!keydata.LocalPublicKey.IsEmpty() && !keydata.PeerPublicKey.IsEmpty());

			if (ValidateBuffer(keydata.LocalPublicKey) &&
				ValidateBuffer(keydata.PeerPublicKey))
			{
				if (OpenSSL::GenerateSharedSecret(keydata))
				{
					return ValidateBuffer(keydata.SharedSecret);
				}
			}
		}

		return false;
	}

	bool GenerateSymmetricKeys(const BufferView& sharedsecret,
							   SymmetricKeyData& key1, SymmetricKeyData& key2) noexcept
	{
		// Should have a shared secret
		assert(!sharedsecret.IsEmpty());

		// Keys should use same crypto algorithms
		assert(key1.HashAlgorithm == key2.HashAlgorithm &&
			   key1.SymmetricAlgorithm == key2.SymmetricAlgorithm);

		auto key_size = 0u;
		switch (key1.SymmetricAlgorithm)
		{
			case Algorithm::Symmetric::AES256_GCM:
			case Algorithm::Symmetric::CHACHA20_POLY1305:
				key_size = 32;
				break;
			default:
				assert(false);
				return false;
		}

		try
		{
			ProtectedBuffer hkdfbuf;
			const auto outlen = (2 * key_size) + (2 * 64); // Two encryption keys and two authentication keys

			// Generate random bytes which will be divided into the four keys
			if (HKDF(sharedsecret, hkdfbuf, outlen, key1.HashAlgorithm))
			{
				assert(hkdfbuf.GetSize() == outlen);

				auto kbuf = BufferView(hkdfbuf);

				// First (2 * key_size) bytes are encryption keys
				key1.Key = kbuf.GetFirst(key_size);
				kbuf.RemoveFirst(key_size);

				key2.Key = kbuf.GetFirst(key_size);
				kbuf.RemoveFirst(key_size);

				// Last 128 bytes are authentication keys
				key1.AuthKey = kbuf.GetFirst(64);
				kbuf.RemoveFirst(64);

				key2.AuthKey = kbuf.GetFirst(64);
				kbuf.RemoveFirst(64);

				assert(kbuf.IsEmpty());

				Dbg(L"Secret: %d bytes - %s", sharedsecret.GetSize(), Util::GetBase64(sharedsecret)->c_str());
				Dbg(L"Enckey1: %d bytes - %s", key1.Key.GetSize(), Util::GetBase64(key1.Key)->c_str());
				Dbg(L"Authkey1: %d bytes - %s", key1.AuthKey.GetSize(), Util::GetBase64(key1.AuthKey)->c_str());
				Dbg(L"Enckey2: %d bytes - %s", key2.Key.GetSize(), Util::GetBase64(key2.Key)->c_str());
				Dbg(L"Authkey2: %d bytes - %s", key2.AuthKey.GetSize(), Util::GetBase64(key2.AuthKey)->c_str());

				return true;
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Exception while copying symmetric keys - %s", Util::ToStringW(e.what()).c_str());
		}

		return false;
	}

	std::optional<ProtectedBuffer> GetPEMPrivateKey(const AsymmetricKeyData& keydata) noexcept
	{
		// Must have a key already
		assert(keydata.GetKey() != nullptr);

		return OpenSSL::GetPEMPrivateKey(static_cast<EVP_PKEY*>(keydata.GetKey()));
	}

	std::optional<ProtectedBuffer> GetPEMPublicKey(const AsymmetricKeyData& keydata) noexcept
	{
		// Must have a key already
		assert(keydata.GetKey() != nullptr);

		return OpenSSL::GetPEMPublicKey(static_cast<EVP_PKEY*>(keydata.GetKey()));
	}

	bool Encrypt(const BufferView& buffer, Buffer& encrbuf,
				 SymmetricKeyData& symkeydata, const BufferView& iv) noexcept
	{
		if (OpenSSLSymmetric::Encrypt(buffer, encrbuf, symkeydata, iv))
		{
			symkeydata.NumBytesProcessed += buffer.GetSize();
			return true;
		}

		return false;
	}

	bool Decrypt(const BufferView& encrbuf, Buffer& buffer,
				 SymmetricKeyData& symkeydata, const BufferView& iv) noexcept
	{
		if (OpenSSLSymmetric::Decrypt(encrbuf, buffer, symkeydata, iv))
		{
			symkeydata.NumBytesProcessed += buffer.GetSize();
			return true;
		}

		return false;
	}

	bool HashAndSign(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& priv_key,
					 Buffer& sig, const Algorithm::Hash type) noexcept
	{
		try
		{
			Buffer hash;
			if (Hash(msg, hash, type))
			{
				return Sign(hash, alg, priv_key, sig);
			}
		}
		catch (...) {}

		return false;
	}

	bool HashAndVerify(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& pub_key,
					   const Buffer& sig, const Algorithm::Hash type) noexcept
	{
		try
		{
			Buffer hash;
			if (Hash(msg, hash, type))
			{
				return Verify(hash, alg, pub_key, sig);
			}
		}
		catch (...) {}

		return false;
	}

	bool Sign(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& priv_key, Buffer& sig) noexcept
	{
		return OpenSSLSign::Sign(msg, alg, priv_key, sig);
	}

	bool Verify(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& pub_key,
				const BufferView& sig) noexcept
	{
		return OpenSSLSign::Verify(msg, alg, pub_key, sig);
	}

	bool CompareBuffers(const BufferView& buffer1, const BufferView& buffer2) noexcept
	{
		if (buffer1.GetSize() != buffer2.GetSize()) return false;

		UChar chksum{ 0 };
		for (BufferView::SizeType x = 0; x < buffer1.GetSize(); ++x)
		{
			chksum |= (static_cast<UChar>(buffer1[x])) ^ (static_cast<UChar>(buffer2[x]));
		}

		return (chksum == 0);
	}

	bool ValidateBuffer(const BufferView& buffer) noexcept
	{
		const auto bsize = buffer.GetSize();

		// Buffer should not be empty
		if (bsize == 0) return false;

		const auto cbuf = reinterpret_cast<const UChar*>(buffer.GetBytes());

		// Buffer bits should not be all on or off
		UChar chksum1{ 0 };
		UChar chksum2{ 0 };
		for (BufferView::SizeType x = 0; x < bsize; ++x)
		{
			chksum1 |= *(cbuf + x);
			chksum2 |= ~*(cbuf + x);

			if (chksum1 > 0 && chksum2 > 0) break;
		}

		return (chksum1 != 0 && chksum2 != 0);
	}
}
