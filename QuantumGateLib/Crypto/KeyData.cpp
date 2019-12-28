// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "KeyData.h"

#include <regex>
#include <openssl/evp.h>

namespace QuantumGate::Implementation::Crypto
{
	AsymmetricKeyData::AsymmetricKeyData(const Algorithm::Asymmetric aa) noexcept : Algorithm(aa)
	{
		switch (aa)
		{
			case Algorithm::Asymmetric::KEM_CLASSIC_MCELIECE:
			case Algorithm::Asymmetric::KEM_NTRUPRIME:
			case Algorithm::Asymmetric::KEM_NEWHOPE:
				KeyExchange = KeyExchangeType::KeyEncapsulation;
				break;
			case Algorithm::Asymmetric::ECDH_SECP521R1:
			case Algorithm::Asymmetric::ECDH_X25519:
			case Algorithm::Asymmetric::ECDH_X448:
				KeyExchange = KeyExchangeType::DiffieHellman;
				break;
			case Algorithm::Asymmetric::EDDSA_ED25519:
			case Algorithm::Asymmetric::EDDSA_ED448:
				KeyExchange = KeyExchangeType::DigitalSigning;
				break;
			default:
				assert(false);
				break;
		}
	}

	AsymmetricKeyData::AsymmetricKeyData(AsymmetricKeyData&& other) noexcept
	{
		*this = std::move(other);
	}

	AsymmetricKeyData::~AsymmetricKeyData()
	{
		ReleaseKeys();
	}

	AsymmetricKeyData& AsymmetricKeyData::operator=(AsymmetricKeyData&& other) noexcept
	{
		Algorithm = std::exchange(other.Algorithm, Algorithm::Asymmetric::Unknown);
		KeyExchange = std::exchange(other.KeyExchange, KeyExchangeType::Unknown);
		Owner = std::exchange(other.Owner, AsymmetricKeyOwner::Unknown);
		Key = std::exchange(other.Key, nullptr);
		LocalPrivateKey = std::move(other.LocalPrivateKey);
		LocalPublicKey = std::move(other.LocalPublicKey);
		PeerPublicKey = std::move(other.PeerPublicKey);
		SharedSecret = std::move(other.SharedSecret);
		EncryptedSharedSecret = std::move(other.EncryptedSharedSecret);

		return *this;
	}

	void AsymmetricKeyData::ReleaseKeys() noexcept
	{
		if (Key != nullptr)
		{
			EVP_PKEY_free(static_cast<EVP_PKEY*>(Key));
			Key = nullptr;
		}

		LocalPrivateKey.Clear();
		LocalPublicKey.Clear();
		PeerPublicKey.Clear();
		EncryptedSharedSecret.Clear();
	}
}