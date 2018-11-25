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
		Set<UInt16> Ports;
		bool Trusted{ false };
		bool DataVerified{ false };
		bool HopVerified{ false };
		Set<std::size_t> ReportingPeerNetworkHashes;
		SteadyTime LastUpdateSteadyTime;

		[[nodiscard]] inline const bool IsTrusted() const noexcept { return Trusted; }

		[[nodiscard]] inline const bool IsVerified() const noexcept
		{
			// Verified in case data and hop verification succeeded, and peers from at least
			// 3 different IP networks reported the address to us
			return (DataVerified && HopVerified && ReportingPeerNetworkHashes.size() >= 3);
		}
	};

	class PublicIPEndpoints final
	{
		struct HopVerificationDetails final
		{
			BinaryIPAddress IPAddress;

			[[nodiscard]] const bool Verify(const bool has_locally_bound_pubip) noexcept;

			static constexpr std::chrono::seconds TimeoutPeriod{ 2 };
			static constexpr UInt8 MaxHops{ 2 };
			static constexpr std::chrono::milliseconds MaxRTT{ 2 };

			static_assert(HopVerificationDetails::TimeoutPeriod > HopVerificationDetails::MaxRTT, "TimeoutPeriod should be larger than MaxRTT");
		};

		using HopVerificationQueue = Concurrency::Queue<HopVerificationDetails>;

		struct HopVerification final
		{
			std::unordered_set<BinaryIPAddress> Set;
			HopVerificationQueue Queue;

			inline void Clear() noexcept
			{
				Set.clear();
				Queue.Clear();
			}
		};

		using HopVerification_ThS = Concurrency::ThreadSafe<HopVerification, std::shared_mutex>;

		class DataVerificationDetails final
		{
			enum class Status { Initialized, Verifying, Succeeded, Timedout, Failed };

		public:
			DataVerificationDetails(const BinaryIPAddress ip) noexcept;

			[[nodiscard]] const bool Verify(const bool nat_traversal) noexcept;

			[[nodiscard]] inline const BinaryIPAddress& GetIPAddress() const noexcept { return m_IPAddress; }
			[[nodiscard]] inline const bool IsVerifying() const noexcept { return (m_Status == Status::Verifying); }
			[[nodiscard]] inline const bool IsVerified() const noexcept { return (m_Status == Status::Succeeded); }

		private:
			[[nodiscard]] const bool InitializeSocket(const bool nat_traversal) noexcept;
			[[nodiscard]] const bool SendVerification() noexcept;
			Result<bool> ReceiveVerification() noexcept;

		private:
			BinaryIPAddress m_IPAddress;
			SteadyTime m_StartSteadyTime;
			UInt64 m_ExpectedData{ 0 };
			Status m_Status{ Status::Initialized };
			Network::Socket m_Socket;

			static constexpr std::chrono::seconds TimeoutPeriod{ 5 };
		};

		using DataVerificationQueue = Concurrency::Queue<DataVerificationDetails>;

		struct DataVerification final
		{
			std::unordered_set<BinaryIPAddress> Set;
			DataVerificationQueue Queue;

			inline void Clear() noexcept
			{
				Set.clear();
				Queue.Clear();
			}
		};

		using DataVerification_ThS = Concurrency::ThreadSafe<DataVerification, std::shared_mutex>;

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

		void SetLocallyBoundPublicIPAddress(const bool flag) noexcept { m_HasLocallyBoundPublicIPAddress = flag; }
		const bool HasLocallyBoundPublicIPAddress() const noexcept { return m_HasLocallyBoundPublicIPAddress; }

	private:
		void PreInitialize() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] const bool AddIPAddressDataVerification(const BinaryIPAddress& ip) noexcept;
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

		DataVerification_ThS m_DataVerification;
		HopVerification_ThS m_HopVerification;

		IPEndpointsMap_ThS m_IPEndpoints;
		ReportingNetworkMap m_ReportingNetworks;

		std::atomic_bool m_HasLocallyBoundPublicIPAddress{ false };

		ThreadPool m_ThreadPool;
	};
}