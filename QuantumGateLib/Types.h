// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <cassert>
#include <string>
#include <ctime>
#include <chrono>
#include <optional>
#include <filesystem>
#include <set>

// Disable warning for export of class member definitions
#pragma warning(disable:4251)

#ifdef QUANTUMGATE_DLL
#ifdef QUANTUMGATE_EXPORTS
#define Export __declspec(dllexport)
#else
#define Export __declspec(dllimport)
#endif
#else
#define Export
#endif

#define ForceInline __forceinline

namespace QuantumGate
{
	using Byte = std::byte;
	using Char = char;
	using UChar = unsigned char;
	using WChar = wchar_t;

	using Short = short;
	using UShort = unsigned short;

	using Int = int;
	using UInt = unsigned int;

	using Long = long;
	using ULong = unsigned long;

	using Int8 = std::int8_t;
	using UInt8 = std::uint8_t;
	using Int16 = std::int16_t;
	using UInt16 = std::uint16_t;
	using Int32 = std::int32_t;
	using UInt32 = std::uint32_t;
	using Int64 = std::int64_t;
	using UInt64 = std::uint64_t;

	using Size = std::size_t;
	using Time = std::time_t;
	using SystemTime = std::chrono::time_point<std::chrono::system_clock>;
	using SteadyTime = std::chrono::time_point<std::chrono::steady_clock>;

	using Path = std::filesystem::path;

	using RelayPort = UInt64;
	using RelayHop = UInt8;
}

#include "Memory\RingBuffer.h"
#include "Memory\BufferView.h"

namespace QuantumGate
{
	using BufferView = Implementation::Memory::BufferView;
	using BufferSpan = Implementation::Memory::BufferSpan;
	using Buffer = Implementation::Memory::Buffer;
	using ProtectedBuffer = Implementation::Memory::ProtectedBuffer;
	using RingBuffer = Implementation::Memory::RingBuffer;
	using ProtectedRingBuffer = Implementation::Memory::ProtectedRingBuffer;

	using String = std::basic_string<wchar_t, std::char_traits<wchar_t>,
		Implementation::Memory::DefaultAllocator<wchar_t>>;
	using StringView = std::wstring_view;

	using ProtectedString = std::basic_string<wchar_t, std::char_traits<wchar_t>,
		Implementation::Memory::DefaultProtectedAllocator<wchar_t>>;
	using ProtectedStringA = std::basic_string<char, std::char_traits<char>,
		Implementation::Memory::DefaultProtectedAllocator<char>>;

	template<typename T>
	using Vector = std::vector<T, Implementation::Memory::DefaultAllocator<T>>;

	template<typename T, typename C = std::less<T>>
	using Set = std::set<T, C, Implementation::Memory::DefaultAllocator<T>>;
}

#include "Common\UUID.h"

namespace QuantumGate
{
	using Implementation::UUID;

	using PeerLUID = UInt64;
	using PeerUUID = UUID;
	using ExtenderUUID = UUID;
}

#include "Algorithms.h"
#include "API\Result.h"
#include "API\Callback.h"
#include "Network\IPEndpoint.h"

namespace QuantumGate::Implementation
{
	struct ProtocolVersion final
	{
		static constexpr const UInt8 Major{ 0 };
		static constexpr const UInt8 Minor{ 1 };
	};

	enum class PeerConnectionType : UInt16
	{
		Unknown, Inbound, Outbound
	};

	struct PeerConnectionAlgorithms
	{
		Algorithm::Hash Hash{ Algorithm::Hash::Unknown };
		Algorithm::Asymmetric PrimaryAsymmetric{ Algorithm::Asymmetric::Unknown };
		Algorithm::Asymmetric SecondaryAsymmetric{ Algorithm::Asymmetric::Unknown };
		Algorithm::Symmetric Symmetric{ Algorithm::Symmetric::Unknown };
		Algorithm::Compression Compression{ Algorithm::Compression::Unknown };
	};
}

namespace QuantumGate
{
	using IPAddress = Implementation::Network::IPAddress;
	using BinaryIPAddress = Implementation::Network::BinaryIPAddress;

	using IPEndpoint = Implementation::Network::IPEndpoint;

	struct Algorithms
	{
		Set<Algorithm::Hash> Hash;
		Set<Algorithm::Asymmetric> PrimaryAsymmetric;
		Set<Algorithm::Asymmetric> SecondaryAsymmetric;
		Set<Algorithm::Symmetric> Symmetric;
		Set<Algorithm::Compression> Compression;
	};

	struct StartupParameters
	{
		PeerUUID UUID;											// The UUID for the local peer
		std::optional<PeerUUID::PeerKeys> Keys;					// The private and public keys for the local peer
		std::optional<ProtectedBuffer> GlobalSharedSecret;		// Global shared secret to use for all connections with peers (in addition to each individual secret key for every peer)
		bool RequireAuthentication{ true };						// Whether authentication is required for connecting peers

		Algorithms SupportedAlgorithms;							// The supported algorithms
		Size NumPreGeneratedKeysPerAlgorithm{ 5 };				// The number of pregenerated keys per supported algorithm

		bool EnableExtenders{ false };							// Enable extenders on startup?

		struct
		{
			struct
			{
				bool Enable{ false };							// Enable listening for incoming connections on startup?
				Set<UInt16> Ports{ 999 };						// Which TCP ports to listen on
			} TCP;

			struct
			{
				bool Enable{ false };							// Enable listening for incoming connections on startup?
				Set<UInt16> Ports{ 999 };						// Which UDP ports to listen on
			} UDP;

			bool EnableNATTraversal{ false };					// Whether NAT traversal is enabled
		} Listeners;

		struct
		{
			bool Enable{ false };								// Enable relays on startup?
			UInt8 IPv4ExcludedNetworksCIDRLeadingBits{ 16 };	// The CIDR leading bits of the IPv4 network address spaces of the source and destination endpoints to exclude from the relay link
			UInt8 IPv6ExcludedNetworksCIDRLeadingBits{ 48 };	// The CIDR leading bits of the IPv6 network address spaces of the source and destination endpoints to exclude from the relay link
		} Relays;
	};

	enum class SecurityLevel : UInt16
	{
		One = 1, Two, Three, Four, Five, Custom
	};

	struct SecurityParameters
	{
		struct
		{
			bool UseConditionalAcceptFunction{ true };							// Whether to use the conditional accept function before accepting connections

			std::chrono::seconds ConnectTimeout{ 0 };							// Maximum number of seconds to wait for a connection to be established

			std::chrono::seconds SuspendTimeout{ 60 };							// Maximum number of seconds of inactivity after which a connection gets suspended (only for endpoints that support suspending connections)
			std::chrono::seconds MaxSuspendDuration{ 60 };						// Maximum number of seconds that a connection may be suspended before the peer is disconnected (only for endpoints that support suspending connections)

			std::chrono::milliseconds MaxHandshakeDelay{ 0 };					// Maximum number of milliseconds to delay a handshake
			std::chrono::seconds MaxHandshakeDuration{ 0 };						// Maximum number of seconds a handshake may last after connecting before peer is disconnected

			std::chrono::seconds IPReputationImprovementInterval{ 0 };			// Period of time after which the reputation of an IP address gets slightly improved

			struct
			{
				Size MaxPerInterval{ 0 };										// Maximum number of allowed connection attempts per interval before IP gets blocked
				std::chrono::seconds Interval{ 0 };								// Period of time after which the connection attempts are reset to 0 for an IP
			} IPConnectionAttempts;
		} General;

		struct
		{
			std::chrono::seconds MinInterval{ 0 };						// Minimum number of seconds to wait before initiating an encryption key update
			std::chrono::seconds MaxInterval{ 0 };						// Maximum number of seconds to wait before initiating an encryption key update
			std::chrono::seconds MaxDuration{ 0 };						// Maximum number of seconds that an encryption key update may last after initiation
			Size RequireAfterNumProcessedBytes{ 0 };					// Number of bytes that may be encrypted and transfered using a single symmetric key after which to require a key update
		} KeyUpdate;

		struct
		{
			std::chrono::seconds ConnectTimeout{ 0 };					// Maximum number of seconds to wait for a relay link to be established
			std::chrono::seconds GracePeriod{ 0 };						// Number of seconds after a relay is closed to still silently accept messages for that relay link
			std::chrono::seconds MaxSuspendDuration{ 60 };				// Maximum number of seconds that a relay link may be suspended before it is closed/removed

			struct
			{
				Size MaxPerInterval{ 0 };								// Maximum number of allowed relay connection attempts per interval before IP gets blocked
				std::chrono::seconds Interval{ 0 };						// Period of time after which the relay connection attempts are reset to 0 for an IP
			} IPConnectionAttempts;
		} Relay;

		struct
		{
			Size ConnectCookieRequirementThreshold{ 10 };				// The number of incoming connections that may be in the process of being established after which a cookie is required
			std::chrono::seconds CookieExpirationInterval{ 120 };		// The number of seconds after which a cookie expires
			std::chrono::milliseconds MaxMTUDiscoveryDelay{ 0 };		// Maximum number of milliseconds to wait before starting MTU discovery
			Size MaxNumDecoyMessages{ 0 };								// Maximum number of decoy messages to send during handshake
			std::chrono::milliseconds MaxDecoyMessageInterval{ 1000 };	// Maximum time interval for decoy messages during handshake
		} UDP;

		struct
		{
			std::chrono::seconds AgeTolerance{ 0 };				// Maximum age of a message in seconds before it's not accepted
			std::chrono::seconds ExtenderGracePeriod{ 0 };		// Number of seconds after an extender is removed to still silently accept messages for that extender
			Size MinRandomDataPrefixSize{ 0 };					// Minimum size in bytes of random data prefix sent with messages
			Size MaxRandomDataPrefixSize{ 0 };					// Maximum size in bytes of random data prefix sent with messages
			Size MinInternalRandomDataSize{ 0 };				// Minimum size in bytes of random data sent with each message
			Size MaxInternalRandomDataSize{ 0 };				// Maximum size in bytes of random data sent with each message
		} Message;

		struct
		{
			bool Enabled{ false };							// Whether sending of noise messages is enabled
			std::chrono::seconds TimeInterval{ 0 };			// Noise time interval in seconds
			Size MinMessagesPerInterval{ 0 };				// Minimum number of noise messages to send in given time interval
			Size MaxMessagesPerInterval{ 0 };				// Maximum number of noise messages to send in given time interval
			Size MinMessageSize{ 0 };						// Minimum size of noise message
			Size MaxMessageSize{ 0 };						// Maximum size of noise message
		} Noise;
	};

}

namespace QuantumGate::API
{
	class Peer;
}

namespace QuantumGate
{
	using ConnectCallback = Callback<void(PeerLUID, Result<API::Peer>)>;
	using DisconnectCallback = Callback<void(PeerLUID, PeerUUID)>;
	using SendCallback = Callback<void()>;

	struct ConnectParameters
	{
		IPEndpoint PeerIPEndpoint;							// The address of the peer
		std::optional<ProtectedBuffer> GlobalSharedSecret;	// Global shared secret to use for this connection
		bool ReuseExistingConnection{ true };				// Whether or not an already existing connection to the peer is allowed to be reused

		struct
		{
			UInt8 Hops{ 0 };								// Number of hops to relay the connection through
			std::optional<PeerLUID> GatewayPeer;			// An already connected peer to attempt to relay through
		} Relay;
	};

	struct SendParameters
	{
		enum class PriorityOption : UInt8
		{
			Normal, Expedited, Delayed
		};

		bool Compress{ true };
		PriorityOption Priority{ PriorityOption::Normal };
		std::chrono::milliseconds Delay{ 0 };
	};

	struct PeerQueryParameters
	{
		enum class RelayOption
		{
			Both, NotRelayed, Relayed
		};

		RelayOption Relays{ RelayOption::Both };

		enum class AuthenticationOption
		{
			Both, NotAuthenticated, Authenticated
		};

		AuthenticationOption Authentication{ AuthenticationOption::Both };

		enum class ConnectionOption
		{
			Both, Inbound, Outbound
		};

		ConnectionOption Connections{ ConnectionOption::Both };

		struct Extenders final
		{
			enum class IncludeOption
			{
				NoneOf, AllOf, OneOf
			};

			Set<ExtenderUUID> UUIDs;
			IncludeOption Include{ IncludeOption::NoneOf };
		} Extenders;
	};
}

