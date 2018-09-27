// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PublicIPEndpoints.h"
#include "..\Common\ScopeGuard.h"

namespace QuantumGate::Implementation::Core
{
	Result<std::pair<bool, bool>> PublicIPEndpoints::AddIPEndpoint(const IPEndpoint& pub_endpoint,
																   const IPEndpoint& rep_peer,
																   const PeerConnectionType rep_con_type,
																   const bool trusted) noexcept
	{
		if (rep_con_type != PeerConnectionType::Unknown &&
			pub_endpoint.GetIPAddress().GetFamily() == rep_peer.GetIPAddress().GetFamily())
		{
			// Should be in public network address range
			if (!pub_endpoint.GetIPAddress().IsLocal() &&
				!pub_endpoint.GetIPAddress().IsMulticast() &&
				!pub_endpoint.GetIPAddress().IsReserved())
			{
				BinaryIPAddress network;
				const auto cidr = (rep_peer.GetIPAddress().GetFamily() == IPAddressFamily::IPv4) ?
					ReportingPeerNetworkIPv4CIDR : ReportingPeerNetworkIPv6CIDR;

				if (BinaryIPAddress::GetNetwork(rep_peer.GetIPAddress().GetBinary(), cidr, network))
				{
					if (AddReportingNetwork(network, trusted))
					{
						// Upon failure to add the public IP address details remove the network
						auto sg = MakeScopeGuard([&] { RemoveReportingNetwork(network); });

						const auto[pub_ipd, new_insert] =
							GetIPEndpointDetails(pub_endpoint.GetIPAddress().GetBinary());
						if (pub_ipd != nullptr)
						{
							sg.Deactivate();

							pub_ipd->LastUpdateSteadyTime = Util::GetCurrentSteadyTime();

							if (trusted) pub_ipd->Trusted = true;

							try
							{
								// Only interested in the port for inbound peers
								// so we know what public port they actually used
								// to connect to us
								if (rep_con_type == PeerConnectionType::Inbound &&
									pub_ipd->Ports.size() < MaxPortsPerIPAddress)
								{
									pub_ipd->Ports.emplace(pub_endpoint.GetPort());
								}

								if (pub_ipd->ReportingPeerNetworkHashes.size() < MaxReportingPeerNetworks)
								{
									pub_ipd->ReportingPeerNetworkHashes.emplace(network.GetHash());
								}

								return std::make_pair(true, new_insert);
							}
							catch (...) {}
						}
					}
					else return std::make_pair(false, false);
				}
			}
		}

		return ResultCode::Failed;
	}

	std::pair<PublicIPEndpointDetails*, bool>
		PublicIPEndpoints::GetIPEndpointDetails(const BinaryIPAddress& pub_ip) noexcept
	{
		auto new_insert = false;
		PublicIPEndpointDetails* pub_ipd{ nullptr };

		if (const auto it = m_IPEndpoints.find(pub_ip); it != m_IPEndpoints.end())
		{
			pub_ipd = &it->second;
		}
		else
		{
			if (m_IPEndpoints.size() >= MaxIPEndpoints)
			{
				RemoveLeastRecentIPEndpoints(m_IPEndpoints.size() - MaxIPEndpoints);
			}

			if (m_IPEndpoints.size() < MaxIPEndpoints)
			{
				try
				{
					const auto[iti, inserted] = m_IPEndpoints.emplace(
						std::make_pair(pub_ip, PublicIPEndpointDetails{}));

					pub_ipd = &iti->second;
					new_insert = inserted;
				}
				catch (...) {}
			}
		}

		return std::make_pair(pub_ipd, new_insert);
	}

	const bool PublicIPEndpoints::RemoveLeastRecentIPEndpoints(Size num) noexcept
	{
		if (!m_IPEndpoints.empty())
		{
			try
			{
				struct MinimalIPEndpointDetails
				{
					BinaryIPAddress IPAddress;
					bool Trusted{ false };
					SteadyTime LastUpdateSteadyTime;
				};

				Vector<MinimalIPEndpointDetails> temp_endp;

				std::for_each(m_IPEndpoints.begin(), m_IPEndpoints.end(), [&](const auto& it)
				{
					temp_endp.emplace_back(MinimalIPEndpointDetails{ it.first, it.second.Trusted,
										   it.second.LastUpdateSteadyTime });
				});

				// Sort by least trusted and least recent
				std::sort(temp_endp.begin(), temp_endp.end(), [](const auto& a, const auto& b) noexcept
				{
					if (!a.Trusted && b.Trusted) return true;
					if (a.Trusted && !b.Trusted) return false;

					return (a.LastUpdateSteadyTime < b.LastUpdateSteadyTime);
				});

				if (num > temp_endp.size()) num = temp_endp.size();

				// Remove first few items which should be
				// least trusted and least recent;
				for (Size x = 0; x < num; x++)
				{
					m_IPEndpoints.erase((temp_endp.begin() + x)->IPAddress);
				}
			}
			catch (...) { return false; }
		}

		return true;
	}

	Result<> PublicIPEndpoints::AddIPAddresses(Vector<BinaryIPAddress>& ips) const noexcept
	{
		try
		{
			for (const auto it : m_IPEndpoints)
			{
				if (std::find(ips.begin(), ips.end(), it.first) == ips.end())
				{
					ips.emplace_back(it.first);
				}
			}

			return ResultCode::Succeeded;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not add public IP addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<> PublicIPEndpoints::AddIPAddresses(Vector<IPAddressDetails>& ips) const noexcept
	{
		try
		{
			for (const auto it : m_IPEndpoints)
			{
				const auto it2 = std::find_if(ips.begin(), ips.end(), [&](const auto& ipd)
				{
					return (ipd.IPAddress.GetBinary() == it.first);
				});

				if (it2 == ips.end())
				{
					auto& ipdetails = ips.emplace_back();
					ipdetails.IPAddress = it.first;
					ipdetails.BoundToLocalEthernetInterface = false;

					ipdetails.PublicDetails.emplace();
					ipdetails.PublicDetails->ReportedByPeers = true;
					ipdetails.PublicDetails->ReportedByTrustedPeers = it.second.Trusted;
					ipdetails.PublicDetails->NumReportingNetworks = it.second.ReportingPeerNetworkHashes.size();
					ipdetails.PublicDetails->Verified = it.second.Verified;
				}
				else
				{
					// May be a locally configured IP that's also
					// publicly visible; add the public details 
					if (!it2->PublicDetails.has_value())
					{
						it2->PublicDetails.emplace();
						it2->PublicDetails->ReportedByPeers = true;
						it2->PublicDetails->ReportedByTrustedPeers = it.second.Trusted;
						it2->PublicDetails->NumReportingNetworks = it.second.ReportingPeerNetworkHashes.size();
						it2->PublicDetails->Verified = it.second.Verified;
					}
				}
			}

			return ResultCode::Succeeded;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not add public IP addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	void PublicIPEndpoints::Clear() noexcept
	{
		m_IPEndpoints.clear();
	}

	const bool PublicIPEndpoints::IsNewReportingNetwork(const BinaryIPAddress& network) const noexcept
	{
		return (m_ReportingNetworks.find(network) == m_ReportingNetworks.end());
	}

	const bool PublicIPEndpoints::AddReportingNetwork(const BinaryIPAddress& network, const bool trusted) noexcept
	{
		if (!IsNewReportingNetwork(network))
		{
			if (!trusted) return false;
			else
			{
				// If the peer is trusted we are very much interested
				// in the public IP and port that it reports back to us
				// even if we already heard from the network it's on
				return true;
			}
		}

		while (m_ReportingNetworks.size() >= MaxReportingPeerNetworks)
		{
			// Remove the network that's least recent
			const auto it = std::min_element(m_ReportingNetworks.begin(), m_ReportingNetworks.end(),
											 [](const auto& a, const auto& b)
			{
				return (a.second < b.second);
			});

			m_ReportingNetworks.erase(it);
		}

		try
		{
			m_ReportingNetworks.emplace(std::make_pair(network, Util::GetCurrentSteadyTime()));
			return true;
		}
		catch (...) {}

		return false;
	}

	void PublicIPEndpoints::RemoveReportingNetwork(const BinaryIPAddress& network) noexcept
	{
		m_ReportingNetworks.erase(network);
	}
}