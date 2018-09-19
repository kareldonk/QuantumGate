// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

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
		using IPEndpointsMap = std::unordered_map<BinaryIPAddress, PublicIPEndpointDetails>;
		using ReportingNetworkMap = std::unordered_map<BinaryIPAddress, SteadyTime>;

	public:
		PublicIPEndpoints() = default;
		PublicIPEndpoints(const PublicIPEndpoints&) = delete;
		PublicIPEndpoints(PublicIPEndpoints&&) = default;
		virtual ~PublicIPEndpoints() = default;
		PublicIPEndpoints& operator=(const PublicIPEndpoints&) = delete;
		PublicIPEndpoints& operator=(PublicIPEndpoints&&) = default;

		Result<std::pair<bool, bool>> AddIPEndpoint(const IPEndpoint& pub_endpoint, const IPEndpoint& rep_peer,
														  const PeerConnectionType rep_con_type,
														  const bool trusted) noexcept;
		const bool RemoveLeastRecentIPEndpoints(Size num) noexcept;

		inline const IPEndpointsMap& GetIPEndpoints() const noexcept { return m_IPEndpoints; }

		Result<> AddIPAddresses(std::vector<BinaryIPAddress>& ips) const noexcept;
		Result<> AddIPAddresses(std::vector<IPAddressDetails>& ips) const noexcept;

		void Clear() noexcept;

	private:
		[[nodiscard]] const bool IsNewReportingNetwork(const BinaryIPAddress& network) const noexcept;

		[[nodiscard]] const bool AddReportingNetwork(const BinaryIPAddress& network, const bool trusted) noexcept;
		void RemoveReportingNetwork(const BinaryIPAddress& network) noexcept;

		std::pair<PublicIPEndpointDetails*, bool>
			GetIPEndpointDetails(const BinaryIPAddress& pub_ip) noexcept;

	private:
		static constexpr const UInt8 MaxReportingPeerNetworks{ 32 };
		static constexpr const UInt8 ReportingPeerNetworkIPv4CIDR{ 16 };
		static constexpr const UInt8 ReportingPeerNetworkIPv6CIDR{ 48 };

		static constexpr const UInt8 MaxIPEndpoints{ 32 };
		static constexpr const UInt8 MaxPortsPerIPAddress{ 16 };

	private:
		IPEndpointsMap m_IPEndpoints;
		ReportingNetworkMap m_ReportingNetworks;
	};
}