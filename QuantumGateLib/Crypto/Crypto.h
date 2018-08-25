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
	std::vector<T> MakeAlgorithmVector(const std::set<T>& list);

	template<typename T>
	void SortAlgorithms(std::vector<T>& list);

	template<typename T>
	Export const bool HasAlgorithm(const std::set<T>& list, const T value);

	template<typename T>
	const T ChooseAlgorithm(const std::set<T>& list1, std::vector<T>& list2);

	std::optional<UInt64> GetCryptoRandomNumber() noexcept;
	std::optional<Buffer> GetCryptoRandomBytes(const Size size) noexcept;

	template<typename T>
	[[nodiscard]] Export const bool Hash(const BufferView& buffer, T& hashbuf, const Algorithm::Hash type) noexcept;

	template<typename T>
	[[nodiscard]] const bool HMAC(const BufferView& buffer, T& hmac, const BufferView& key,
								  const Algorithm::Hash type) noexcept;

	[[nodiscard]] Export const bool HKDF(const BufferView& secret, ProtectedBuffer& outkey, const Size outkeylen,
										 const Algorithm::Hash type) noexcept;

	[[nodiscard]] const bool GenerateAsymmetricKeys(AsymmetricKeyData& keydata) noexcept;
	[[nodiscard]] const bool GenerateSharedSecret(AsymmetricKeyData& keydata) noexcept;
	[[nodiscard]] const bool GenerateSymmetricKeys(const BufferView& sharedsecret,
												   SymmetricKeyData& key1, SymmetricKeyData& key2) noexcept;

	[[nodiscard]] std::optional<ProtectedBuffer> GetPEMPrivateKey(AsymmetricKeyData& keydata) noexcept;
	[[nodiscard]] std::optional<ProtectedBuffer> GetPEMPublicKey(AsymmetricKeyData& keydata) noexcept;

	[[nodiscard]] const bool Encrypt(const BufferView& buffer, Buffer& encrbuf,
									 SymmetricKeyData& symkeydata, const BufferView& iv) noexcept;

	[[nodiscard]] const bool Decrypt(const BufferView& encrbuf, Buffer& buffer,
									 SymmetricKeyData& symkeydata, const BufferView& iv) noexcept;

	[[nodiscard]] const bool HashAndSign(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& priv_key,
										 Buffer& sig, const Algorithm::Hash type) noexcept;

	[[nodiscard]] const bool HashAndVerify(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& pub_key,
										   const Buffer& sig, const Algorithm::Hash type) noexcept;

	[[nodiscard]] const bool Sign(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& priv_key,
								  Buffer& sig) noexcept;
	[[nodiscard]] const bool Verify(const BufferView& msg, const Algorithm::Asymmetric alg, const BufferView& pub_key,
									const BufferView& sig) noexcept;

	[[nodiscard]] const bool CompareBuffers(const BufferView& buffer1, const BufferView& buffer2) noexcept;

	[[nodiscard]] const bool ValidateBuffer(const BufferView& buffer) noexcept;
}
