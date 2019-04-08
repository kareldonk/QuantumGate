// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "KeyData.h"

namespace QuantumGate::Implementation::Crypto
{
	const WChar* GetAlgorithmName(const Algorithm::Asymmetric alg) noexcept;
	const WChar* GetAlgorithmName(const Algorithm::Symmetric alg) noexcept;
	const WChar* GetAlgorithmName(const Algorithm::Hash alg) noexcept;
	const WChar* GetAlgorithmName(const Algorithm::Compression alg) noexcept;

	template<typename T>
	void SortAlgorithms(Vector<T>& list);

	template<typename T>
	Export bool HasAlgorithm(const Vector<T>& list, const T value);

	template<typename T>
	const T ChooseAlgorithm(const Vector<T>& list1, Vector<T>& list2);

	std::optional<UInt64> GetCryptoRandomNumber() noexcept;
	std::optional<Buffer> GetCryptoRandomBytes(const Size size) noexcept;

	template<typename T>
	[[nodiscard]] Export bool Hash(const BufferView& buffer, T& hashbuf, const Algorithm::Hash type) noexcept;

	template<typename T>
	[[nodiscard]] bool HMAC(const BufferView& buffer, T& hmac, const BufferView& key, const Algorithm::Hash type) noexcept;

	[[nodiscard]] Export bool HKDF(const BufferView& secret, ProtectedBuffer& outkey, const Size outkeylen,
								   const Algorithm::Hash type) noexcept;

	[[nodiscard]] bool GenerateAsymmetricKeys(AsymmetricKeyData& keydata) noexcept;
	[[nodiscard]] bool GenerateSharedSecret(AsymmetricKeyData& keydata) noexcept;
	[[nodiscard]] bool GenerateSymmetricKeys(const BufferView& sharedsecret,
											 SymmetricKeyData& key1, SymmetricKeyData& key2) noexcept;

	[[nodiscard]] std::optional<ProtectedBuffer> GetPEMPrivateKey(AsymmetricKeyData& keydata) noexcept;
	[[nodiscard]] std::optional<ProtectedBuffer> GetPEMPublicKey(AsymmetricKeyData& keydata) noexcept;

	[[nodiscard]] bool Encrypt(const BufferView& buffer, Buffer& encrbuf,
							   SymmetricKeyData& symkeydata, const BufferView& iv) noexcept;

	[[nodiscard]] bool Decrypt(const BufferView& encrbuf, Buffer& buffer,
							   SymmetricKeyData& symkeydata, const BufferView& iv) noexcept;

	[[nodiscard]] bool HashAndSign(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& priv_key,
								   Buffer& sig, const Algorithm::Hash type) noexcept;

	[[nodiscard]] bool HashAndVerify(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& pub_key,
									 const Buffer& sig, const Algorithm::Hash type) noexcept;

	[[nodiscard]] bool Sign(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& priv_key,
							Buffer& sig) noexcept;
	[[nodiscard]] bool Verify(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& pub_key,
							  const BufferView& sig) noexcept;

	[[nodiscard]] bool CompareBuffers(const BufferView& buffer1, const BufferView& buffer2) noexcept;

	[[nodiscard]] bool ValidateBuffer(const BufferView& buffer) noexcept;
}
