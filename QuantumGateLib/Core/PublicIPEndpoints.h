// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Concurrency\ThreadPool.h"
#include "..\Network\Socket.h"

namespace QuantumGate::Implementation::Core
{
	struct PublicIPEndpointDetails
	{
		std::set<UInt16> Ports;
		bool Trusted{ false };
		bool Verified{ false };
		std::set<std::size_t> ReportingPeerNetworkHashes;
		SteadyTime LastUpdateSteadyTime;
	};

	class PublicIPEndpoints
	{
		struct IPAddressVerification
		{
			enum class Status
			{
				Registered,
				VerificationSent
			};

			Status Status{ Status::Registered };
			BinaryIPAddress IPAddress;
			SteadyTime LastUpdate;
		};

		using IPAddressVerificationMap = std::unordered_map<UInt64, IPAddressVerification>;
		using IPAddressVerificationMap_ThS = Concurrency::ThreadSafe<IPAddressVerificationMap, std::shared_mutex>;

		using IPEndpointsMap = std::unordered_map<BinaryIPAddress, PublicIPEndpointDetails>;
		using IPEndpointsMap_ThS = Concurrency::ThreadSafe<IPEndpointsMap, std::shared_mutex>;

		using ReportingNetworkMap = std::unordered_map<BinaryIPAddress, SteadyTime>;

		struct ThreadPoolData
		{
			using Socket_ThS = Concurrency::ThreadSafe<Network::Socket, std::shared_mutex>;

			Socket_ThS IPv4UDPSocket;
			Socket_ThS IPv6UDPSocket;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData>;

	public:
		PublicIPEndpoints() = default;
		PublicIPEndpoints(const PublicIPEndpoints&) = delete;
		PublicIPEndpoints(PublicIPEndpoints&&) = default;
		virtual ~PublicIPEndpoints() = default;
		PublicIPEndpoints& operator=(const PublicIPEndpoints&) = delete;
		PublicIPEndpoints& operator=(PublicIPEndpoints&&) = default;

		[[nodiscard]] const bool Initialize() noexcept;
		void Deinitialize() noexcept;
		inline const bool IsInitialized() const noexcept { return m_Initialized; }

		Result<std::pair<bool, bool>> AddIPEndpoint(const IPEndpoint& pub_endpoint, const IPEndpoint& rep_peer,
													const PeerConnectionType rep_con_type,
													const bool trusted) noexcept;
		const bool RemoveLeastRecentIPEndpoints(Size num, IPEndpointsMap& ipendpoints) noexcept;

		inline const IPEndpointsMap_ThS& GetIPEndpoints() const noexcept { return m_IPEndpoints; }

		Result<> AddIPAddresses(Vector<BinaryIPAddress>& ips) const noexcept;
		Result<> AddIPAddresses(Vector<IPAddressDetails>& ips) const noexcept;

	private:
		[[nodiscard]] const bool InitializeSockets() noexcept;
		void DeinitializeSockets() noexcept;

		void PreInitialize() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] const bool AddIPAddressVerification(const BinaryIPAddress& ip) noexcept;
		[[nodiscard]] const bool SendIPAddressVerification(const BinaryIPAddress& ip, const UInt64 num) noexcept;

		[[nodiscard]] const bool IsNewReportingNetwork(const BinaryIPAddress& network) const noexcept;
		[[nodiscard]] const bool AddReportingNetwork(const BinaryIPAddress& network, const bool trusted) noexcept;
		void RemoveReportingNetwork(const BinaryIPAddress& network) noexcept;

		std::pair<PublicIPEndpointDetails*, bool>
			GetIPEndpointDetails(const BinaryIPAddress& pub_ip, IPEndpointsMap& ipendpoints) noexcept;

		const std::pair<bool, bool> WorkerThreadProcessor(ThreadPoolData& thpdata,
														  const Concurrency::EventCondition& shutdown_event);

	private:
		static constexpr const UInt8 MaxReportingPeerNetworks{ 32 };
		static constexpr const UInt8 ReportingPeerNetworkIPv4CIDR{ 16 };
		static constexpr const UInt8 ReportingPeerNetworkIPv6CIDR{ 48 };

		static constexpr const UInt8 MaxIPEndpoints{ 32 };
		static constexpr const UInt8 MaxPortsPerIPAddress{ 16 };

	private:
		std::atomic_bool m_Initialized{ false };

		IPAddressVerificationMap_ThS m_IPAddressVerification;
		IPEndpointsMap_ThS m_IPEndpoints;
		ReportingNetworkMap m_ReportingNetworks;

		ThreadPool m_ThreadPool;
	};
}