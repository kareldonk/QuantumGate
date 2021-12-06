// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Common\Containers.h"
#include "..\Concurrency\ThreadPool.h"
#include "..\Concurrency\Queue.h"
#include "..\Network\Socket.h"
#include "..\API\Local.h"

namespace QuantumGate::Implementation::Core
{
	struct PublicEndpointDetails final
	{
		Containers::Map<Network::Protocol, Set<UInt16>> PortsMap;
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

	class PublicEndpoints final
	{
		using IPAddressSet = Containers::UnorderedSet<IPAddress>;
		using IPAddressSet_ThS = Concurrency::ThreadSafe<IPAddressSet, std::shared_mutex>;

		struct HopVerificationDetails final
		{
			IPAddress IPAddress;

			[[nodiscard]] bool Verify(const bool has_locally_bound_pubip) noexcept;

			static constexpr std::chrono::seconds TimeoutPeriod{ 2 };
			static constexpr UInt8 MaxHops{ 2 };
			static constexpr std::chrono::milliseconds MaxRTT{ 2 };

			static_assert(HopVerificationDetails::TimeoutPeriod > HopVerificationDetails::MaxRTT, "TimeoutPeriod should be larger than MaxRTT");
		};

		using HopVerificationQueue = Concurrency::Queue<HopVerificationDetails>;

		struct HopVerification final
		{
			IPAddressSet_ThS Set;
			HopVerificationQueue Queue;

			inline void Clear() noexcept
			{
				Set.WithUniqueLock()->clear();
				Queue.Clear();
			}
		};

		class DataVerificationDetails final
		{
			enum class Status { Initialized, Verifying, Succeeded, Timedout, Failed };

		public:
			DataVerificationDetails(const IPAddress& ip) noexcept;

			[[nodiscard]] bool Verify(const bool nat_traversal) noexcept;

			[[nodiscard]] inline const IPAddress& GetIPAddress() const noexcept { return m_IPAddress; }
			[[nodiscard]] inline bool IsVerifying() const noexcept { return (m_Status == Status::Verifying); }
			[[nodiscard]] inline bool IsVerified() const noexcept { return (m_Status == Status::Succeeded); }

		private:
			[[nodiscard]] bool InitializeSocket(const bool nat_traversal) noexcept;
			[[nodiscard]] bool SendVerification() noexcept;
			Result<bool> ReceiveVerification() noexcept;

		private:
			IPAddress m_IPAddress;
			SteadyTime m_StartSteadyTime;
			UInt64 m_ExpectedData{ 0 };
			Status m_Status{ Status::Initialized };
			Network::Socket m_Socket;

			static constexpr std::chrono::seconds TimeoutPeriod{ 5 };
		};

		using DataVerificationQueue = Concurrency::Queue<DataVerificationDetails>;

		struct DataVerification final
		{
			IPAddressSet_ThS Set;
			DataVerificationQueue Queue;

			inline void Clear() noexcept
			{
				Set.WithUniqueLock()->clear();
				Queue.Clear();
			}
		};

		using EndpointsMap = Containers::UnorderedMap<Network::Address, PublicEndpointDetails>;
		using EndpointsMap_ThS = Concurrency::ThreadSafe<EndpointsMap, std::shared_mutex>;

		using ReportingNetworkMap = Containers::UnorderedMap<Network::Address, SteadyTime>;

		using ThreadPool = Concurrency::ThreadPool<>;

	public:
		PublicEndpoints(const Settings_CThS& settings) noexcept :
			m_Settings(settings)
		{}

		PublicEndpoints(const PublicEndpoints&) = delete;
		PublicEndpoints(PublicEndpoints&&) noexcept = default;
		~PublicEndpoints() { if (IsInitialized()) Deinitialize(); }
		PublicEndpoints& operator=(const PublicEndpoints&) = delete;
		PublicEndpoints& operator=(PublicEndpoints&&) noexcept = default;

		[[nodiscard]] bool Initialize() noexcept;
		void Deinitialize() noexcept;
		[[nodiscard]] inline bool IsInitialized() const noexcept { return m_Initialized; }

		Result<std::pair<bool, bool>> AddEndpoint(const Endpoint& pub_endpoint, const Endpoint& rep_peer,
												  const PeerConnectionType rep_con_type,
												  const bool trusted, const bool verified = false) noexcept;
		bool RemoveLeastRelevantEndpoints(Size num, EndpointsMap& ipendpoints) noexcept;

		inline EndpointsMap_ThS& GetEndpoints() noexcept { return m_Endpoints; }

		Result<> AddAddresses(Vector<Network::Address>& addrs, const bool only_trusted_verified) const noexcept;
		Result<> AddAddresses(Vector<API::Local::Environment::AddressDetails>& addrs) const noexcept;

		void SetLocallyBoundPublicIPAddress(const bool flag) noexcept { m_HasLocallyBoundPublicIPAddress = flag; }
		bool HasLocallyBoundPublicIPAddress() const noexcept { return m_HasLocallyBoundPublicIPAddress; }

	private:
		void PreInitialize() noexcept;
		void ResetState() noexcept;

		[[nodiscard]] bool AddIPAddressDataVerification(const IPAddress& ip) noexcept;
		[[nodiscard]] bool AddIPAddressHopVerification(const IPAddress& ip) noexcept;

		[[nodiscard]] bool IsNewReportingNetwork(const Network::Address& network) const noexcept;
		[[nodiscard]] bool AddReportingNetwork(const Network::Address& network, const bool trusted) noexcept;
		void RemoveReportingNetwork(const Network::Address& network) noexcept;

		std::pair<PublicEndpointDetails*, bool>
			GetEndpointDetails(const Network::Address& pub_addr, EndpointsMap& endpoints) noexcept;

		void DataVerificationWorkerThreadWait(const Concurrency::Event& shutdown_event);
		void DataVerificationWorkerThreadWaitInterrupt();
		void DataVerificationWorkerThread(const Concurrency::Event& shutdown_event);

		void HopVerificationWorkerThreadWait(const Concurrency::Event& shutdown_event);
		void HopVerificationWorkerThreadWaitInterrupt();
		void HopVerificationWorkerThread(const Concurrency::Event& shutdown_event);

	public:
		static constexpr const UInt8 MaxReportingPeerNetworks{ 32 };
		static constexpr const UInt8 ReportingPeerNetworkIPv4CIDR{ 16 };
		static constexpr const UInt8 ReportingPeerNetworkIPv6CIDR{ 48 };

		static constexpr const UInt8 MaxEndpoints{ 32 };
		static constexpr const UInt8 MaxProtocolsPerAddress{ 2 };
		static constexpr const UInt8 MaxPortsPerProtocol{ 16 };

	private:
		std::atomic_bool m_Initialized{ false };

		const Settings_CThS& m_Settings;

		DataVerification m_DataVerification;
		HopVerification m_HopVerification;

		EndpointsMap_ThS m_Endpoints;
		ReportingNetworkMap m_ReportingNetworks;

		std::atomic_bool m_HasLocallyBoundPublicIPAddress{ false };

		ThreadPool m_ThreadPool;
	};
}