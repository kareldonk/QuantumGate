// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Algorithm
{
	enum class Hash : UInt16
	{
		Unknown = 0,
		SHA256 = 100,
		SHA512 = 200,
		BLAKE2S256 = 300,
		BLAKE2B512 = 400
	};

	struct HashAlgorithmName
	{
		static constexpr const WChar* SHA256{ L"SHA256" };
		static constexpr const WChar* SHA512{ L"SHA512" };
		static constexpr const WChar* BLAKE2S256{ L"BLAKE2S256" };
		static constexpr const WChar* BLAKE2B512{ L"BLAKE2B512" };
	};

	enum class Asymmetric : UInt16
	{
		Unknown = 0,

		ECDH_SECP521R1 = 100,
		ECDH_X25519 = 200,
		ECDH_X448 = 300,

		KEM_NTRUPRIME = 1000,
		KEM_NEWHOPE = 1100,
		KEM_CLASSIC_MCELIECE = 1200,

		EDDSA_ED25519 = 2000,
		EDDSA_ED448 = 2100
	};

	struct AsymmetricAlgorithmName
	{
		static constexpr const WChar* ECDH_SECP521R1{ L"ECDH_SECP521R1" };
		static constexpr const WChar* ECDH_X25519{ L"ECDH_X25519" };
		static constexpr const WChar* ECDH_X448{ L"ECDH_X448" };

		static constexpr const WChar* KEM_CLASSIC_MCELIECE{ L"KEM_CLASSIC_MCELIECE" };
		static constexpr const WChar* KEM_NTRUPRIME{ L"KEM_NTRUPRIME" };
		static constexpr const WChar* KEM_NEWHOPE{ L"KEM_NEWHOPE" };

		static constexpr const WChar* EDDSA_ED25519{ L"EDDSA_ED25519" };
		static constexpr const WChar* EDDSA_ED448{ L"EDDSA_ED448" };
	};

	enum class Symmetric : UInt16
	{
		Unknown = 0,
		AES256_GCM = 100,
		CHACHA20_POLY1305 = 200
	};

	struct SymmetricAlgorithmName
	{
		static constexpr const WChar* AES256_GCM{ L"AES256_GCM" };
		static constexpr const WChar* CHACHA20_POLY1305{ L"CHACHA20_POLY1305" };
	};

	enum class Compression : UInt16
	{
		Unknown = 0,
		DEFLATE = 100,
		ZSTANDARD = 200
	};

	struct CompressionAlgorithmName
	{
		static constexpr const WChar* DEFLATE{ L"DEFLATE" };
		static constexpr const WChar* ZSTANDARD{ L"ZSTANDARD" };
	};
}
