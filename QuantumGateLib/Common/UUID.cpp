// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "UUID.h"
#include "Util.h"
#include "Random.h"
#include "Endian.h"
#include "..\Common\Hash.h"
#include "..\Crypto\Crypto.h"

#include <regex>

namespace QuantumGate::Implementation
{
	UUID::UUID(const String& uuid)
	{
		Set(uuid);
	}

	const bool UUID::Verify(const ProtectedBuffer& pub_key) const noexcept
	{
		if (GetType() == Type::Peer)
		{
			if (!pub_key.IsEmpty())
			{
				UUID uuid;

				// Fill with hash of public key
				if (uuid.FillPeerUUID(pub_key))
				{
					// Compare all bits except for the version, type and signing algorithm
					if ((m_Data1 == uuid.m_Data1) &&
						(m_Data2 == uuid.m_Data2) &&
						((m_Data3 & 0xfff0) == (uuid.m_Data3 & 0xfff0)) &&
						((m_Data4 >> 6) == (uuid.m_Data4 >> 6)))
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	String UUID::GetString() const noexcept
	{
		const auto data4_bytes = reinterpret_cast<const Byte*>(&m_Data4);
		return Util::FormatString(L"%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x",
								  Endian::ToNetworkByteOrder(m_Data1), Endian::ToNetworkByteOrder(m_Data2),
								  Endian::ToNetworkByteOrder(m_Data3), data4_bytes[0], data4_bytes[1],
								  data4_bytes[2], data4_bytes[3], data4_bytes[4],
								  data4_bytes[5], data4_bytes[6], data4_bytes[7]);
	}

	std::size_t UUID::GetHash() const noexcept
	{
		const QuantumGate::Implementation::SerializedUUID suuid{ *this }; // Gets rid of padding bytes

		return static_cast<std::size_t>(
			QuantumGate::Implementation::Hash::GetNonPersistentHash(
				QuantumGate::Implementation::Memory::BufferView(reinterpret_cast<const QuantumGate::Byte*>(&suuid),
																sizeof(suuid))));
	}

	const bool UUID::TryParse(const String& str, UUID& uuid) noexcept
	{
		try
		{
			uuid.Set(str);
			return true;
		}
		catch (...) {}

		return false;
	}

	std::tuple<bool, UUID, std::optional<PeerKeys>> UUID::Create(const Type type,
																 const SignAlgorithm salg) noexcept
	{
		auto success = false;
		UUID uuid;
		std::optional<PeerKeys> keys;

		try
		{
			switch (type)
			{
				case Type::Unknown:
				{
					break;
				}
				case Type::Peer:
				{
					auto alg = Algorithm::Asymmetric::Unknown;
					switch (salg)
					{
						case SignAlgorithm::EDDSA_ED25519:
							alg = Algorithm::Asymmetric::EDDSA_ED25519;
							break;
						case SignAlgorithm::EDDSA_ED448:
							alg = Algorithm::Asymmetric::EDDSA_ED448;
							break;
						default:
							assert(false);
							break;
					}

					if (alg != Algorithm::Asymmetric::Unknown)
					{
						Crypto::AsymmetricKeyData keydata(alg);
						if (Crypto::GenerateAsymmetricKeys(keydata))
						{
							if (uuid.FillPeerUUID(keydata.LocalPublicKey))
							{
								uuid.SetVersion();
								uuid.SetType(type);
								uuid.SetSignAlgorithm(salg);

								PeerKeys tkeys;
								tkeys.PrivateKey = std::move(keydata.LocalPrivateKey);
								tkeys.PublicKey = std::move(keydata.LocalPublicKey);

								keys = std::move(tkeys);

								success = true;
							}
						}
					}
					break;
				}
				case Type::Extender:
				{
					if (uuid.FillExtenderUUID())
					{
						uuid.SetVersion();
						uuid.SetType(type);
						success = true;
					}
					break;
				}
			}
		}
		catch (...) {}

		return std::make_tuple(success, std::move(uuid), std::move(keys));
	}

	const bool UUID::FillPeerUUID(const ProtectedBuffer& pub_key) noexcept
	{
		UInt64 hash[2]{ 0, 0 };
		hash[0] = Hash::GetHash(pub_key,
								BufferView(reinterpret_cast<const Byte*>(&HashKey1),
										   sizeof(HashKey1)));
		hash[1] = Hash::GetHash(pub_key,
								BufferView(reinterpret_cast<const Byte*>(&HashKey2),
										   sizeof(HashKey2)));

		static_assert(sizeof(UUID) == sizeof(hash), "UUID size mismatch; expecting 16 bytes");

		// Fill member data with hash of the public key
		memcpy(this, &hash, sizeof(hash));

		return true;
	}

	const bool UUID::FillExtenderUUID() noexcept
	{
		static_assert(sizeof(UUID) == 16, "UUID size mismatch; expecting 16 bytes");

		auto buffer = Random::GetPseudoRandomBytes(sizeof(UUID));

		// Fill member data with random bytes
		memcpy(this, buffer.GetBytes(), sizeof(UUID));

		return true;
	}

	void UUID::Set(const String& uuid)
	{
		try
		{
			if (uuid.size() == 36)
			{
				// Looks for UUID in the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
				// such as "3df5b8e4-50d2-48c5-8c23-c544f0f0653e"
				std::wregex r(LR"uuid(^\s*([0-9a-f]{8})-([0-9a-f]{4})-([0-9a-f]{4})-([0-9a-f]{4})-([0-9a-f]{12})\s*$)uuid",
							  std::regex_constants::icase);
				std::wsmatch m;
				if (std::regex_search(uuid, m, r))
				{
					wchar_t* end{ nullptr };
					errno = 0;

					m_Data1 = Endian::FromNetworkByteOrder(
						static_cast<UInt32>(std::wcstoull(m[1].str().c_str(), &end, 16)));
					if (errno == 0)
					{
						m_Data2 = Endian::FromNetworkByteOrder(
							static_cast<UInt16>(std::wcstoull(m[2].str().c_str(), &end, 16)));
						if (errno == 0)
						{
							m_Data3 = Endian::FromNetworkByteOrder(
								static_cast<UInt16>(std::wcstoull(m[3].str().c_str(), &end, 16)));
							if (errno == 0)
							{
								m_Data4 = Endian::FromNetworkByteOrder(
									std::wcstoull(String(m[4].str() + m[5].str()).c_str(), &end, 16));
								if (errno == 0 && IsValid())
								{
									return;
								}
							}
						}
					}
				}
			}
		}
		catch (...) {}

		Clear();

		throw std::invalid_argument("Invalid UUID");
	}

	std::ostream& operator<<(std::ostream& stream, const UUID& uuid)
	{
		stream << Util::ToStringA(uuid.GetString());
		return stream;
	}
	
	std::wostream& operator<<(std::wostream& stream, const UUID& uuid)
	{
		stream << uuid.GetString();
		return stream;
	}
}
