// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Crypto\Crypto.h"
#include "..\KeyGeneration\KeyGenerationManager.h"

namespace QuantumGate::Implementation::Core::UDP
{
	enum class SymmetricKeysType
	{
		Unknown, Default, Derived
	};

	class SymmetricKeys final
	{
	public:
		SymmetricKeys() noexcept {};
		SymmetricKeys(const ProtectedBuffer& global_sharedsecret) :
			m_Type(SymmetricKeysType::Default)
		{
			// This will use default keydata when Global Shared Secret is not in use;
			// this provides basic obfuscation and HMAC checks but won't
			// fool more sophisticated traffic analyzers
			CreateKeys(global_sharedsecret, BufferView(reinterpret_cast<const Byte*>(&SymmetricKeys::DefaultKeyData),
													   SymmetricKeys::KeyDataLength));
		}

		SymmetricKeys(const ProtectedBuffer& global_sharedsecret, const BufferView key_input_data) :
			m_Type(SymmetricKeysType::Derived)
		{
			CreateKeys(global_sharedsecret, key_input_data);
		}

		SymmetricKeys(const SymmetricKeys&) = delete;
		SymmetricKeys(SymmetricKeys&&) noexcept = default;
		~SymmetricKeys() = default;
		SymmetricKeys& operator=(const SymmetricKeys&) = delete;
		SymmetricKeys& operator=(SymmetricKeys&&) noexcept = default;

		inline explicit operator bool() const noexcept
		{
			return (m_Type != SymmetricKeysType::Unknown && m_KeyData.GetSize() == KeyDataLength);
		}

		[[nodiscard]] inline BufferView GetKey() const noexcept
		{
			assert(m_KeyData.GetSize() == KeyDataLength);
			return m_KeyData.operator BufferView().GetFirst(KeyLength);
		}

		[[nodiscard]] inline BufferView GetAuthKey() const noexcept
		{
			assert(m_KeyData.GetSize() == KeyDataLength);
			return m_KeyData.operator BufferView().GetLast(KeyLength);
		}

		inline void Expire() noexcept { m_ExpirationSteadyTime = Util::GetCurrentSteadyTime(); }

		[[nodiscard]] bool IsExpired() const noexcept
		{
			// If it has an expiration time
			if (m_ExpirationSteadyTime.has_value())
			{
				// Check if it has been expired for too long
				if (Util::GetCurrentSteadyTime() - *m_ExpirationSteadyTime > ExpirationGracePeriod)
				{
					return true;
				}
			}

			return false;
		}

		inline void Clear() noexcept
		{
			m_Type = SymmetricKeysType::Unknown;
			m_KeyData.Clear();
			m_ExpirationSteadyTime.reset();
		}

	private:
		void CreateKeys(const ProtectedBuffer& global_sharedsecret, const BufferView key_input_data);

	private:
		static constexpr UInt8 KeyLength{ sizeof(UInt64) };
		static constexpr UInt8 KeyDataLength{ KeyLength * 2 };

		static constexpr UInt8 DefaultKeyData[KeyDataLength]{
			0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
			0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
		};

	private:
		SymmetricKeysType m_Type{ SymmetricKeysType::Unknown };
		ProtectedBuffer m_KeyData;
		std::optional<SteadyTime> m_ExpirationSteadyTime;

		// Maximum amount of seconds a key can still be used after having been expired
		static const constexpr std::chrono::seconds ExpirationGracePeriod{ 120 };
	};

	class KeyExchange final
	{
	public:
		KeyExchange() = delete;
		
		KeyExchange(KeyGeneration::Manager& keymgr, const PeerConnectionType connection_type, ProtectedBuffer&& handshake_data)
		{
			const auto owner = (connection_type == PeerConnectionType::Outbound) ?
				Crypto::AsymmetricKeyOwner::Alice : Crypto::AsymmetricKeyOwner::Bob;
			if (!GenerateAsymmetricKeys(keymgr, Algorithm::Asymmetric::ECDH_X25519, owner))
			{
				throw std::exception("Failed to generate asymmetric keys for UDP connection.");
			}
			else
			{
				if (connection_type == PeerConnectionType::Inbound)
				{
					SetPeerHandshakeData(std::move(handshake_data));
				}
				else
				{
					// Shouldn't have handshakedata for outbound connections
					assert(handshake_data.IsEmpty());
				}
			}
		}

		KeyExchange(const KeyExchange&) = delete;
		KeyExchange(KeyExchange&&) noexcept = default;
		~KeyExchange() = default;
		KeyExchange& operator=(const KeyExchange&) = delete;
		KeyExchange& operator=(KeyExchange&&) noexcept = default;

		inline void SetPeerHandshakeData(ProtectedBuffer&& buffer) noexcept
		{
			// Asymmetric keys should already have been created
			assert(m_AsymmetricKeys.has_value());

			if (m_AsymmetricKeys->GetKeyExchangeType() == Crypto::KeyExchangeType::KeyEncapsulation &&
				m_AsymmetricKeys->GetOwner() == Crypto::AsymmetricKeyOwner::Alice)
			{
				m_AsymmetricKeys->EncryptedSharedSecret = std::move(buffer);
				return;
			}

			m_AsymmetricKeys->PeerPublicKey = std::move(buffer);
		}

		inline const ProtectedBuffer& GetHandshakeData() const noexcept
		{
			// Asymmetric keys should already have been created
			assert(m_AsymmetricKeys.has_value());

			if (m_AsymmetricKeys->GetKeyExchangeType() == Crypto::KeyExchangeType::KeyEncapsulation &&
				m_AsymmetricKeys->GetOwner() == Crypto::AsymmetricKeyOwner::Bob)
			{
				return m_AsymmetricKeys->EncryptedSharedSecret;
			}

			return m_AsymmetricKeys->LocalPublicKey;
		}

		[[nodiscard]] SymmetricKeys GenerateSymmetricKeys(const ProtectedBuffer& global_sharedsecret) noexcept
		{
			if (GenerateSharedSecret())
			{
				try
				{
					return { global_sharedsecret, m_AsymmetricKeys->SharedSecret };
				}
				catch (...) {}
			}

			return {};
		}

	private:
		[[nodiscard]] inline bool GenerateAsymmetricKeys(KeyGeneration::Manager& keymgr, const Algorithm::Asymmetric aa,
														 const Crypto::AsymmetricKeyOwner owner) noexcept
		{
			// Should not already have a key
			assert(!m_AsymmetricKeys.has_value());

			m_AsymmetricKeys.emplace(aa);

			if (m_AsymmetricKeys->GetKeyExchangeType() == Crypto::KeyExchangeType::KeyEncapsulation &&
				owner == Crypto::AsymmetricKeyOwner::Bob)
			{
				// Bob doesn't need an asymmetric keypair;
				// he'll encrypt a shared secret using Alice's public key
				m_AsymmetricKeys->SetOwner(owner);
				return true;
			}

			// First check if we have a pre-generated keypair available
			auto keys = keymgr.GetAsymmetricKeys(aa);
			if (keys)
			{
				*m_AsymmetricKeys = std::move(*keys);
				m_AsymmetricKeys->SetOwner(owner);
				return true;
			}

			// Generate an asymmetric keypair on the fly below (slower
			// especially for certain algorithms, which introduces delays
			// in the connection handshake which might result in timeouts)
			if (Crypto::GenerateAsymmetricKeys(*m_AsymmetricKeys))
			{
				m_AsymmetricKeys->SetOwner(owner);
				return true;
			}

			return false;
		}

		[[nodiscard]] inline bool GenerateSharedSecret() noexcept
		{
			// Asymmetric keys should already have been created
			assert(m_AsymmetricKeys.has_value());

			return Crypto::GenerateSharedSecret(*m_AsymmetricKeys);
		}

	private:
		std::optional<Crypto::AsymmetricKeyData> m_AsymmetricKeys;
	};
}