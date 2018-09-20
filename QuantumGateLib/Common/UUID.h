// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <optional>

namespace QuantumGate::Implementation
{
	class Export UUID
	{
		friend struct SerializedUUID;

	public:
		enum class Type
		{
			Unknown, Peer, Extender
		};

		enum class SignAlgorithm
		{
			None, EDDSA_ED25519, EDDSA_ED448
		};

		constexpr UUID() noexcept {}
		UUID(const String& uuid);
		~UUID() = default;

		constexpr UUID(const UUID& other) noexcept
		{
			*this = other;
		}

		constexpr UUID(UUID&& other) noexcept
		{
			*this = std::move(other);
		}

		constexpr UUID& operator=(const UUID& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Data1 = other.m_Data1;
			m_Data2 = other.m_Data2;
			m_Data3 = other.m_Data3;
			m_Data4.UInt64 = other.m_Data4.UInt64;

			return *this;
		}

		constexpr UUID& operator=(UUID&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			*this = other;

			other.Clear();

			return *this;
		}

		void Set(const String& uuid);

		[[nodiscard]] constexpr const bool IsValid() const noexcept
		{
			return (GetType() != Type::Unknown);
		}

		[[nodiscard]] const bool Verify(const ProtectedBuffer& pub_key) const noexcept;

		constexpr Type GetType() const noexcept
		{
			if ((static_cast<UInt8>(m_Data3) & UUID::VersionMask) == UUID::UUIDVersion)
			{
				if ((static_cast<UInt8>(m_Data4.Bytes[0]) & UUID::VariantMask) == UUID::UUIDVariantPeer)
				{
					if (((static_cast<UInt8>(m_Data4.Bytes[0]) & UUID::SignAlgorithmMask) == UUID::UUIDSignAlgorithm_EDDSA_ED448) ||
						((static_cast<UInt8>(m_Data4.Bytes[0]) & UUID::SignAlgorithmMask) == UUID::UUIDSignAlgorithm_EDDSA_ED25519))
					{
						return Type::Peer;
					}
				}
				else if ((static_cast<UInt8>(m_Data4.Bytes[0]) & UUID::VariantMask) == UUID::UUIDVariantExtender) return Type::Extender;
			}

			return Type::Unknown;
		}

		constexpr SignAlgorithm GetSignAlgorithm() const noexcept
		{
			if ((static_cast<UInt8>(m_Data3) & UUID::VersionMask) == UUID::UUIDVersion)
			{
				if ((static_cast<UInt8>(m_Data4.Bytes[0]) & UUID::VariantMask) == UUID::UUIDVariantPeer)
				{
					if ((static_cast<UInt8>(m_Data4.Bytes[0]) & UUID::SignAlgorithmMask) == UUID::UUIDSignAlgorithm_EDDSA_ED25519)
					{
						return SignAlgorithm::EDDSA_ED25519;
					}
					else if ((static_cast<UInt8>(m_Data4.Bytes[0]) & UUID::SignAlgorithmMask) == UUID::UUIDSignAlgorithm_EDDSA_ED448)
					{
						return SignAlgorithm::EDDSA_ED448;
					}
				}
			}

			return SignAlgorithm::None;
		}

		String GetString() const noexcept;

		std::size_t GetHash() const noexcept;

		constexpr explicit operator bool() const noexcept { return IsValid(); }

		constexpr void Clear() noexcept
		{
			m_Data1 = 0;
			m_Data2 = 0;
			m_Data3 = 0;
			m_Data4.UInt64 = 0;
		}

		constexpr const bool operator==(const UUID& other) const noexcept
		{
			return (m_Data1 == other.m_Data1 &&
					m_Data2 == other.m_Data2 &&
					m_Data3 == other.m_Data3 &&
					m_Data4.UInt64 == other.m_Data4.UInt64);
		}

		constexpr const bool operator!=(const UUID& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr bool operator<(const UUID& other) const noexcept
		{
			return ((static_cast<UInt64>(m_Data1) + static_cast<UInt64>(m_Data2) +
					 static_cast<UInt64>(m_Data3) + m_Data4.UInt64) <
				(static_cast<UInt64>(other.m_Data1) + static_cast<UInt64>(other.m_Data2) +
				 static_cast<UInt64>(other.m_Data3) + other.m_Data4.UInt64));
		}

		[[nodiscard]] static const bool TryParse(const String& str, UUID& uuid) noexcept;
		[[nodiscard]] static std::tuple<bool, UUID, std::optional<PeerKeys>> Create(const Type type,
																					const SignAlgorithm salg) noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const UUID& uuid);
		friend Export std::wostream& operator<<(std::wostream& stream, const UUID& uuid);

	private:
		constexpr void SetVersion() noexcept
		{
			// Clear first 4 bits and add version
			m_Data3 = (m_Data3 & 0xFFF0) | UUID::UUIDVersion;
		}

		constexpr void SetType(const Type type) noexcept
		{
			// Clear first 3 bits and add type
			if (type == Type::Extender)
			{
				m_Data4.Bytes[0] = (m_Data4.Bytes[0] & Byte{ 0xF8 }) | Byte{ UUID::UUIDVariantExtender };
			}
			else m_Data4.Bytes[0] = (m_Data4.Bytes[0] & Byte{ 0xF8 }) | Byte{ UUID::UUIDVariantPeer };
		}

		constexpr void SetSignAlgorithm(const SignAlgorithm type) noexcept
		{
			// Clear bits 4 - 6 and add signature algorithm
			if (type == SignAlgorithm::EDDSA_ED25519)
			{
				m_Data4.Bytes[0] = (m_Data4.Bytes[0] & Byte{ 0xC7 }) | Byte{ UUID::UUIDSignAlgorithm_EDDSA_ED25519 };
			}
			else m_Data4.Bytes[0] = (m_Data4.Bytes[0] & Byte{ 0xC7 }) | Byte{ UUID::UUIDSignAlgorithm_EDDSA_ED448 };
		}

		[[nodiscard]] const bool FillPeerUUID(const ProtectedBuffer& pub_key) noexcept;
		[[nodiscard]] const bool FillExtenderUUID() noexcept;

	private:
		UInt32 m_Data1{ 0 };
		UInt16 m_Data2{ 0 };
		UInt16 m_Data3{ 0 };
		union
		{
			Byte Bytes[8];
			UInt64 UInt64{ 0 };
		} m_Data4;

		static constexpr UInt8 UUIDVersion{ 0b00001001 }; // 9
		static constexpr UInt8 UUIDVariantPeer{ 0b00000011 }; // 3
		static constexpr UInt8 UUIDVariantExtender{ 0b00000110 }; // 6

		static constexpr UInt8 UUIDSignAlgorithm_EDDSA_ED25519{ 0b00001000 }; // 1
		static constexpr UInt8 UUIDSignAlgorithm_EDDSA_ED448{ 0b00010000 }; // 2

		static constexpr UInt8 VersionMask{ 0b00001111 };
		static constexpr UInt8 VariantMask{ 0b00000111 };
		static constexpr UInt8 SignAlgorithmMask{ 0b00111000 };

		static constexpr UInt8 HashKey1[16]{
			33, 66, 99, 33, 66, 99, 33, 66, 99,
			33, 66, 99, 33, 66, 99, 33
		};

		static constexpr UInt8 HashKey2[16]{
			99, 66, 33, 99, 66, 33, 99, 66, 33,
			99, 66, 33, 99, 66, 33, 99
		};
	};

#pragma pack(push, 1) // Disable padding bytes
	struct SerializedUUID
	{
		UInt32 Data1{ 0 };
		UInt16 Data2{ 0 };
		UInt16 Data3{ 0 };
		union
		{
			Byte Bytes[8];
			UInt64 UInt64{ 0 };
		} Data4;

		SerializedUUID() noexcept {}
		SerializedUUID(const UUID& uuid) noexcept { *this = uuid; }

		SerializedUUID& operator=(const UUID& uuid) noexcept
		{
			Data1 = uuid.m_Data1;
			Data2 = uuid.m_Data2;
			Data3 = uuid.m_Data3;
			Data4.UInt64 = uuid.m_Data4.UInt64;
			return *this;
		}

		operator UUID() const noexcept
		{
			UUID uuid;
			uuid.m_Data1 = Data1;
			uuid.m_Data2 = Data2;
			uuid.m_Data3 = Data3;
			uuid.m_Data4.UInt64 = Data4.UInt64;
			return uuid;
		}

		const bool operator==(const SerializedUUID& other) const noexcept
		{
			return (Data1 == other.Data1 &&
					Data2 == other.Data2 &&
					Data3 == other.Data3 &&
					Data4.UInt64 == other.Data4.UInt64);
		}

		const bool operator!=(const SerializedUUID& other) const noexcept
		{
			return !(*this == other);
		}
	};
#pragma pack(pop)
}

namespace std
{
	// Specialization for standard hash function for UUID
	template<> struct hash<QuantumGate::Implementation::UUID>
	{
		std::size_t operator()(const QuantumGate::Implementation::UUID& uuid) const noexcept
		{
			return uuid.GetHash();
		}
	};
}