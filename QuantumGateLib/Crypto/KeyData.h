// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Algorithms.h"

namespace QuantumGate::Implementation::Crypto
{
	enum class AsymmetricKeyOwner
	{
		Unknown, Alice, Bob
	};

	enum class KeyExchangeType
	{
		Unknown, DiffieHellman, KeyEncapsulation, DigitalSigning
	};

	struct AsymmetricKeyData final
	{
		AsymmetricKeyData() = delete;
		AsymmetricKeyData(const Algorithm::Asymmetric aa) noexcept;
		AsymmetricKeyData(const AsymmetricKeyData&) = delete;
		AsymmetricKeyData(AsymmetricKeyData&& other) noexcept;
		~AsymmetricKeyData();
		AsymmetricKeyData& operator=(const AsymmetricKeyData&) = delete;
		AsymmetricKeyData& operator=(AsymmetricKeyData&& other) noexcept;

		inline void SetKey(void* key) noexcept { Key = key; }
		inline void* GetKey() const noexcept { return Key; }

		inline void SetOwner(const AsymmetricKeyOwner owner) noexcept { Owner = owner; }
		inline AsymmetricKeyOwner GetOwner() const noexcept { return Owner; }

		inline Algorithm::Asymmetric GetAlgorithm() const noexcept { return Algorithm; }
		inline KeyExchangeType GetKeyExchangeType() const noexcept { return KeyExchange; }

		void ReleaseKeys() noexcept;

		ProtectedBuffer LocalPrivateKey;
		ProtectedBuffer LocalPublicKey;
		ProtectedBuffer PeerPublicKey;
		ProtectedBuffer SharedSecret;
		ProtectedBuffer EncryptedSharedSecret;

	private:
		Algorithm::Asymmetric Algorithm{ Algorithm::Asymmetric::Unknown };
		KeyExchangeType KeyExchange{ KeyExchangeType::Unknown };
		AsymmetricKeyOwner Owner{ AsymmetricKeyOwner::Unknown };
		void* Key{ nullptr };
	};

	enum class SymmetricKeyType
	{
		Unknown, AutoGen, Derived
	};

	struct SymmetricKeyData final
	{
		SymmetricKeyData() = delete;
		SymmetricKeyData(const SymmetricKeyType type, const Algorithm::Hash ha,
						 const Algorithm::Symmetric sa, const Algorithm::Compression ca) noexcept :
			Type(type), HashAlgorithm(ha), SymmetricAlgorithm(sa), CompressionAlgorithm(ca)
		{}

		SymmetricKeyType Type{ SymmetricKeyType::Unknown };
		ProtectedBuffer Key;
		ProtectedBuffer AuthKey;
		Algorithm::Hash HashAlgorithm{ Algorithm::Hash::Unknown };
		Algorithm::Symmetric SymmetricAlgorithm{ Algorithm::Symmetric::Unknown };
		Algorithm::Compression CompressionAlgorithm{ Algorithm::Compression::Unknown };
		Size NumBytesProcessed{ 0 };
	};
}