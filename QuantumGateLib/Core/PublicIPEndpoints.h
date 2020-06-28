// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Concurrency\ThreadPool.h"
#include "..\Concurrency\Queue.h"
#include "..\Network\Socket.h"
#include "..\API\Local.h"

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

		[[nodiscard]] inline bool IsTrusted() const noexcept { return Trusted; }

		[[nodiscard]] inline bool IsVerified() const noexcept
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

			[[nodiscard]] bool Verify(const bool has_locally_bound_pubip) noexcept;

			static constexpr std::chrono::seconds TimeoutPeriod{ 2 };
			static constexpr UInt8 MaxHops{ 2 };
			static constexpr std::chrono::milliseconds MaxRTT{ 2 };

			static_assert(HopVerificationDetails::TimeoutPeriod > HopVerificationDetails::MaxRTT, "TimeoutPeriod should be larger than MaxRTT");
		};

		using HopVerificationQueue = Concurrency::Queue<HopVerificationDetails>;

		struct HopVerification final
		{
			Containers::UnorderedSet<BinaryIPAddress> Set;
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

			[[nodiscard]] bool Verify(const bool nat_traversal) noexcept;

			[[nodiscard]] inline const BinaryIPAddress& GetIPAddress() const noexcept { return m_IPAddress; }
			[[nodiscard]] inline bool IsVerifying() const noexcept { return (m_Status == Status::Verifying); }
			[[nodiscard]] inline bool IsVerified() const noexcept { return (m_Status == Status::Succeeded); }

		private:
			[[nodiscard]] bool InitializeSocket(const bool nat_traversal) noexcept;
			[[nodiscard]] bool SendVerification() noexcept;
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
			Containers::UnorderedSet<BinaryIPAddress> Set;
			DataVerificationQueue Queue;

			inline void Clear() noexcept
			{
				Set.clear();
				Queue.Clear();
			}
		};

		using DataVerification_ThS = Concurrency::ThreadSafe<DataVerification, std::shared_mutex>;

		using IPEndpointsMap = Containers::UnorderedMap<BinaryIPAddress, PublicIPEndpointDetails>;
		using IPEndpointsMap_ThS = Concurrency::ThreadSafe<IPEndpointsMap, std::shared_mutex>;

		using ReportingNetworkMap = Containers::UnorderedMap<BinaryIPAddress, SteadyTime>;

		using ThreadPool = Concurrency::ThreadPool<>;

	public:
		PublicIPEndpoints(const Settings_CThS& settings) noexcept :
			m_Settings(settings)
		{}

		PublicIPEndpoints(const PublicIPEndpoints&) = delete;
		PublicIPEndpoints(PublicIPEndpoints&&) noexcept = default;
		~PublicIPEndpoints() { if (IsInitialized()) Deinitialize(); }
		PublicIPEndpoints& operator=(const PublicIPEndpoints&) = delete;
		PublicIPEndpoints& operator=(PublicIPEndpoints&&) noexcept = default;

		[[nodiscard]] bool Initialize() noexcept;
		void Deinitialize() noexcept;
		[[nodiscard]] inline bool IsInitialized() const noexcept { return m_Initialized; }

		Result<std::pair<bool, bool>> AddIPEndpoint(const IPEndpoint& pub_endpoint, const IPEndpoint& rep_peer,
													const PeerConnectionType rep_con_type,
													const bool trusted, const bool verified = false) noexcept;
		bool RemoveLeastRelevantIPEndpoints(Size num, IPEndpointsMap& ipendpoints) noexcept;

		inline IPEndpointsMap_ThS& GetIPEndpoints() noexcept { return m_IPEndpoints; }

		Result<> AddIPAddresses(Vector<BinaryIPAddress>& ips, const bool only_trusted_verified) const noexcept;
		Result<> AddIPAddresses(Vector<API::Local::Environment::IPAddressDetails>& ips) const noexcept;

		void SetLocallyBoundPublicIPAddress(const bool flag) noexcept { m_HasLocallyBoundPublicIPAddress = flag; }
		bool HasLocallyBoundPublicIPAddress() const noexcept { return m_HasLocallyBoundPublicIPAddress; }

	private:
		void PreInitialize() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] bool AddIPAddressDataVerification(const BinaryIPAddress& ip) noexcept;
		[[nodiscard]] bool AddIPAddressHopVerification(const BinaryIPAddress& ip) noexcept;

		[[nodiscard]] bool IsNewReportingNetwork(const BinaryIPAddress& network) const noexcept;
		[[nodiscard]] bool AddReportingNetwork(const BinaryIPAddress& network, const bool trusted) noexcept;
		void RemoveReportingNetwork(const BinaryIPAddress& network) noexcept;

		std::pair<PublicIPEndpointDetails*, bool>
			GetIPEndpointDetails(const BinaryIPAddress& pub_ip, IPEndpointsMap& ipendpoints) noexcept;

		ThreadPool::ThreadCallbackResult DataVerificationWorkerThread(const Concurrency::Event& shutdown_event);
		ThreadPool::ThreadCallbackResult HopVerificationWorkerThread(const Concurrency::Event& shutdown_event);

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