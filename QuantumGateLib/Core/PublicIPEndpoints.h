// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Concurrency\ThreadPool.h"
#include "..\Concurrency\Queue.h"
#include "..\Network\Socket.h"

#include <unordered_set>

namespace QuantumGate::Implementation::Core
{
	struct PublicIPEndpointDetails final
	{
		std::set<UInt16> Ports;
		bool Trusted{ false };
		bool DataVerified{ false };
		bool HopVerified{ false };
		std::set<std::size_t> ReportingPeerNetworkHashes;
		SteadyTime LastUpdateSteadyTime;

		[[nodiscard]] inline const bool IsTrusted() const noexcept { return Trusted; }
		[[nodiscard]] inline const bool IsVerified() const noexcept { return (DataVerified && HopVerified); }
	};

	class PublicIPEndpoints final
	{
		struct HopVerification final
		{
			BinaryIPAddress IPAddress;

			static constexpr std::chrono::seconds TimeoutPeriod{ 5 };
			static constexpr UInt8 MaxHops{ 2 };
		};

		using HopVerificationQueue = Concurrency::Queue<HopVerification>;

		struct HopVerificationData final
		{
			std::unordered_set<BinaryIPAddress> Set;
			HopVerificationQueue Queue;
		};

		using HopVerificationData_ThS = Concurrency::ThreadSafe<HopVerificationData, std::shared_mutex>;

		struct DataVerification final
		{
			enum class Status
			{
				Registered,
				VerificationSent
			};

			Status Status{ Status::Registered };
			BinaryIPAddress IPAddress;
			SteadyTime LastUpdateSteadyTime;
			UInt8 NumVerificationTries{ 0 };

			static constexpr std::chrono::seconds TimeoutPeriod{ 30 };
			static constexpr UInt8 MaxVerificationTries{ 3 };
		};

		using DataVerificationMap = std::unordered_map<UInt64, DataVerification>;
		using DataVerificationMap_ThS = Concurrency::ThreadSafe<DataVerificationMap, std::shared_mutex>;

		struct DataVerificationSockets final
		{
			using Socket_ThS = Concurrency::ThreadSafe<Network::Socket, std::shared_mutex>;

			Socket_ThS IPv4UDPSocket;
			Socket_ThS IPv6UDPSocket;
			std::atomic<UInt16> Port{ 0 };
		};

		using IPEndpointsMap = std::unordered_map<BinaryIPAddress, PublicIPEndpointDetails>;
		using IPEndpointsMap_ThS = Concurrency::ThreadSafe<IPEndpointsMap, std::shared_mutex>;

		using ReportingNetworkMap = std::unordered_map<BinaryIPAddress, SteadyTime>;

		using ThreadPool = Concurrency::ThreadPool<>;

	public:
		PublicIPEndpoints(const Settings_CThS& settings) noexcept :
			m_Settings(settings)
		{}

		PublicIPEndpoints(const PublicIPEndpoints&) = delete;
		PublicIPEndpoints(PublicIPEndpoints&&) = default;
		~PublicIPEndpoints() { if (IsInitialized()) Deinitialize(); }
		PublicIPEndpoints& operator=(const PublicIPEndpoints&) = delete;
		PublicIPEndpoints& operator=(PublicIPEndpoints&&) = default;

		[[nodiscard]] const bool Initialize() noexcept;
		void Deinitialize() noexcept;
		[[nodiscard]] inline const bool IsInitialized() const noexcept { return m_Initialized; }

		Result<std::pair<bool, bool>> AddIPEndpoint(const IPEndpoint& pub_endpoint, const IPEndpoint& rep_peer,
													const PeerConnectionType rep_con_type,
													const bool trusted, const bool verified = false) noexcept;
		const bool RemoveLeastRelevantIPEndpoints(Size num, IPEndpointsMap& ipendpoints) noexcept;

		inline IPEndpointsMap_ThS& GetIPEndpoints() noexcept { return m_IPEndpoints; }

		Result<> AddIPAddresses(Vector<BinaryIPAddress>& ips, const bool only_trusted_verified) const noexcept;
		Result<> AddIPAddresses(Vector<IPAddressDetails>& ips) const noexcept;

	private:
		[[nodiscard]] const bool InitializeDataVerificationSockets() noexcept;
		void DeinitializeDataVerificationSockets() noexcept;

		void PreInitialize() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] const bool AddIPAddressDataVerification(const BinaryIPAddress& ip) noexcept;
		[[nodiscard]] const bool SendIPAddressVerification(const UInt64 num, DataVerification& ip_verification) noexcept;

		[[nodiscard]] const bool AddIPAddressHopVerification(const BinaryIPAddress& ip) noexcept;

		[[nodiscard]] const bool IsNewReportingNetwork(const BinaryIPAddress& network) const noexcept;
		[[nodiscard]] const bool AddReportingNetwork(const BinaryIPAddress& network, const bool trusted) noexcept;
		void RemoveReportingNetwork(const BinaryIPAddress& network) noexcept;

		std::pair<PublicIPEndpointDetails*, bool>
			GetIPEndpointDetails(const BinaryIPAddress& pub_ip, IPEndpointsMap& ipendpoints) noexcept;

		const std::pair<bool, bool> DataVerificationWorkerThread(const Concurrency::EventCondition& shutdown_event);
		const std::pair<bool, bool> HopVerificationWorkerThread(const Concurrency::EventCondition& shutdown_event);

	public:
		static constexpr const UInt8 MaxReportingPeerNetworks{ 32 };
		static constexpr const UInt8 ReportingPeerNetworkIPv4CIDR{ 16 };
		static constexpr const UInt8 ReportingPeerNetworkIPv6CIDR{ 48 };

		static constexpr const UInt8 MaxIPEndpoints{ 32 };
		static constexpr const UInt8 MaxPortsPerIPAddress{ 16 };

	private:
		std::atomic_bool m_Initialized{ false };

		const Settings_CThS& m_Settings;

		DataVerificationMap_ThS m_DataVerification;
		DataVerificationSockets m_DataVerificationSockets;

		HopVerificationData_ThS m_HopVerification;

		IPEndpointsMap_ThS m_IPEndpoints;
		ReportingNetworkMap m_ReportingNetworks;

		ThreadPool m_ThreadPool;
	};
}