// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "LocalEnvironment.h"
#include "..\Common\ScopeGuard.h"

#include <ws2tcpip.h>
#include <Iphlpapi.h>

namespace QuantumGate::Implementation::Core
{
	const bool LocalEnvironment::Initialize(ChangedCallback&& callback) noexcept
	{
		assert(!IsInitialized());

		if (!m_PublicIPEndpoints.Initialize()) return false;

		// Upon failure deinit public IP endpoints when we return
		auto sg0 = MakeScopeGuard([&] { m_PublicIPEndpoints.Deinitialize(); });

		if (!UpdateEnvironmentInformation()) return false;

		// Upon failure clear environment info when we return
		auto sg1 = MakeScopeGuard([&] { ClearEnvironmentInformation(); });

		if (callback) m_ChangedCallback.WithUniqueLock() = std::move(callback);

		// Upon failure clear callback when we return
		auto sg2 = MakeScopeGuard([&] { m_ChangedCallback.WithUniqueLock()->Clear(); });

		if (!RegisterIPInterfaceChangeNotification()) return false;

		sg0.Deactivate();
		sg1.Deactivate();
		sg2.Deactivate();

		m_Initialized = true;

		return true;
	}

	void LocalEnvironment::Deinitialize() noexcept
	{
		assert(IsInitialized());

		m_Initialized = false;

		DeregisterIPInterfaceChangeNotification();

		ClearEnvironmentInformation();

		m_PublicIPEndpoints.Deinitialize();
	}

	const bool LocalEnvironment::Update() noexcept
	{
		assert(IsInitialized());

		return UpdateEnvironmentInformation();
	}

	const Vector<BinaryIPAddress>* LocalEnvironment::GetTrustedAndVerifiedIPAddresses() const noexcept
	{
		try
		{
			return &m_CachedIPAddresses.GetCache();
		}
		catch (...) {}

		return nullptr;
	}

	String LocalEnvironment::GetIPAddressesString() const noexcept
	{
		try
		{
			String allips;
			for (const auto& ifs : m_EthernetInterfaces)
			{
				if (ifs.Operational)
				{
					for (const auto& ip : ifs.IPAddresses)
					{
						if (allips.length() > 0) allips += L", ";

						allips += ip.GetString();
					}
				}
			}

			return allips;
		}
		catch (...) {}

		return {};
	}

	String LocalEnvironment::GetMACAddressesString() const noexcept
	{
		try
		{
			String alladdr;
			for (const auto& ifs : m_EthernetInterfaces)
			{
				if (ifs.Operational)
				{
					if (alladdr.length() > 0) alladdr += L", ";

					alladdr += ifs.MACAddress;
				}
			}

			return alladdr;
		}
		catch (...) {}

		return {};
	}

	Result<String> LocalEnvironment::OSGetHostname() noexcept
	{
		try
		{
			char hostname[NI_MAXHOST]{ 0 };

			const auto ret = gethostname(hostname, NI_MAXHOST);
			if (ret == 0)
			{
				return Util::ToStringW(hostname);
			}
		}
		catch (...) {}

		LogErr(L"Could not get the name of the local host (" + Util::GetSystemErrorString(GetLastError()) + L")");

		return ResultCode::Failed;
	}

	Result<Vector<BinaryIPAddress>> LocalEnvironment::OSGetIPAddresses(const String& hostname) noexcept
	{
		try
		{
			ADDRINFOW* result{ nullptr };

			const auto ret = GetAddrInfoW(hostname.c_str(), L"0", nullptr, &result);
			if (ret == 0)
			{
				// Free resources when we return
				auto sg = MakeScopeGuard([&] { FreeAddrInfoW(result); });

				Vector<BinaryIPAddress> alladdr;

				for (auto ptr = result; ptr != nullptr; ptr = ptr->ai_next)
				{
					if (ptr->ai_family == AF_INET || ptr->ai_family == AF_INET6)
					{
						alladdr.emplace_back(IPAddress(ptr->ai_addr).GetBinary());
					}
				}

				return std::move(alladdr);
			}
			else LogErr(L"Could not get addresses for host %s (" + Util::GetSystemErrorString(GetLastError()) + L")",
						hostname.c_str());
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not get addresses for host %s due to exception: %s",
				   hostname.c_str(), Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<Vector<EthernetInterface>> LocalEnvironment::OSGetEthernetInterfaces() noexcept
	{
		try
		{
			const ULONG family{ AF_UNSPEC };

			ULONG buflen{ sizeof(IP_ADAPTER_ADDRESSES) };
			auto addresses = std::make_unique<Byte[]>(buflen);

			// Make an initial call to GetAdaptersAddresses to get the necessary size into the buflen variable
			if (GetAdaptersAddresses(family, 0, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(addresses.get()),
									 &buflen) == ERROR_BUFFER_OVERFLOW)
			{
				addresses = std::make_unique<Byte[]>(buflen);
			}

			const auto ret = GetAdaptersAddresses(family, 0, nullptr,
												  reinterpret_cast<IP_ADAPTER_ADDRESSES*>(addresses.get()),
												  &buflen);
			if (ret == NO_ERROR)
			{
				Vector<EthernetInterface> allifs;

				auto address = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(addresses.get());

				while (address != nullptr)
				{
					if (address->IfType == IF_TYPE_ETHERNET_CSMACD ||
						address->IfType == IF_TYPE_IEEE80211 ||
						address->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
					{
						auto& ifs = allifs.emplace_back();
						ifs.Name = Util::ToStringW(address->AdapterName);
						ifs.Description = address->Description;

						if (address->OperStatus == IfOperStatusUp) ifs.Operational = true;

						// Get MAC address
						for (auto i = 0ul; i < address->PhysicalAddressLength; ++i)
						{
							ifs.MACAddress += Util::FormatString(L"%.2X", static_cast<int>(address->PhysicalAddress[i]));
						}

						// Get IP Addresses
						for (auto pUnicast = address->FirstUnicastAddress;
							 pUnicast != nullptr; pUnicast = pUnicast->Next)
						{
							ifs.IPAddresses.emplace_back(IPAddress(pUnicast->Address.lpSockaddr));
						}
					}

					address = address->Next;
				}

				return std::move(allifs);
			}
			else LogErr(L"Could not get addresses for local networking adapters (" +
						Util::GetSystemErrorString(GetLastError()) + L")");
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not get addresses for local networking adapters due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<String> LocalEnvironment::OSGetUsername() noexcept
	{
		try
		{
			ULONG nlen{ 256 };
			WChar name[256]{ 0 };

			if (GetUserNameEx(EXTENDED_NAME_FORMAT::NameSamCompatible, reinterpret_cast<LPWSTR>(name), &nlen))
			{
				return { name };
			}
		}
		catch (...) {}

		LogErr(L"Could not get the username for the current user (" + Util::GetSystemErrorString(GetLastError()) + L")");

		return ResultCode::Failed;
	}

	const bool LocalEnvironment::AddPublicIPEndpoint(const IPEndpoint& pub_endpoint,
													 const IPEndpoint& rep_peer,
													 const PeerConnectionType rep_con_type,
													 const bool trusted) noexcept
	{
		if (const auto result = m_PublicIPEndpoints.AddIPEndpoint(pub_endpoint, rep_peer,
																  rep_con_type, trusted);
			result.Succeeded())
		{
			if (result->first && result->second)
			{
				// New address was added; update cache
				DiscardReturnValue(UpdateCachedIPAddresses());
			}

			return true;
		}

		return false;
	}

	const bool LocalEnvironment::RegisterIPInterfaceChangeNotification() noexcept
	{
		assert(m_IPInterfaceChangeNotificationHandle == NULL);

		if (NotifyIpInterfaceChange(AF_UNSPEC, &IPInterfaceChangeNotificationCallback,
									reinterpret_cast<PVOID>(this), FALSE,
									&m_IPInterfaceChangeNotificationHandle) == NO_ERROR)
		{
			return true;
		}
		else
		{
			LogErr(L"Failed to register ethernet interfaces change notification");
		}

		return false;
	}

	void LocalEnvironment::DeregisterIPInterfaceChangeNotification() noexcept
	{
		if (m_IPInterfaceChangeNotificationHandle != NULL)
		{
			if (CancelMibChangeNotify2(m_IPInterfaceChangeNotificationHandle) == NO_ERROR)
			{
				m_IPInterfaceChangeNotificationHandle = NULL;
			}
			else
			{
				LogErr(L"Failed to cancel ethernet interfaces change notification");
			}
		}
	}

	VOID LocalEnvironment::IPInterfaceChangeNotificationCallback(PVOID CallerContext,
																 PMIB_IPINTERFACE_ROW Row,
																 MIB_NOTIFICATION_TYPE NotificationType)
	{
		assert(CallerContext != NULL);

		LogDbg(L"Received IP interface change notification (%d) from OS", NotificationType);

		const auto le = reinterpret_cast<LocalEnvironment*>(CallerContext);
		le->m_ChangedCallback.WithSharedLock([](const auto& callback)
		{
			if (callback) callback();
		});
	}

	const bool LocalEnvironment::UpdateEnvironmentInformation() noexcept
	{
		if (auto result = OSGetHostname(); result.Failed()) return false;
		else m_Hostname = std::move(result.GetValue());

		if (auto result = OSGetUsername(); result.Failed()) return false;
		else m_Username = std::move(result.GetValue());

		if (auto result = OSGetEthernetInterfaces(); result.Failed()) return false;
		else m_EthernetInterfaces = std::move(result.GetValue());

		if (!UpdateCachedIPAddresses()) return false;

		return true;
	}

	void LocalEnvironment::ClearEnvironmentInformation() noexcept
	{
		m_Hostname.clear();
		m_Username.clear();
		m_EthernetInterfaces.clear();

		m_CachedIPAddresses.UpdateValue([](auto& addresses) noexcept
		{
			addresses.clear();
		});

		return;
	}

	const bool LocalEnvironment::UpdateCachedIPAddresses() noexcept
	{
		try
		{
			auto has_public_ip = false;
			Vector<BinaryIPAddress> allips;

			// First add the local IP addresses configured on the host
			for (const auto& ifs : m_EthernetInterfaces)
			{
				if (ifs.Operational)
				{
					for (const auto& ip : ifs.IPAddresses)
					{
						if (ip.IsPublic())
						{
							// Probably connected via public IP address
							// directly to the Internet
							has_public_ip = true;
						}

						if (std::find(allips.begin(), allips.end(), ip.GetBinary()) == allips.end())
						{
							allips.emplace_back(ip.GetBinary());
						}
					}
				}
			}

			m_PublicIPEndpoints.SetLocallyBoundPublicIPAddress(has_public_ip);

			// Add any trusted/verified public IP addresses if we have them
			if (m_PublicIPEndpoints.AddIPAddresses(allips, true).Succeeded())
			{
				m_CachedIPAddresses.UpdateValue([&](auto& addresses) noexcept
				{
					addresses = std::move(allips);
				});

				return true;
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not update cached IP addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return false;
	}

	Result<Vector<IPAddressDetails>> LocalEnvironment::GetIPAddresses() const noexcept
	{
		try
		{
			Vector<IPAddressDetails> allips;

			// First add the local IP addresses configured on the host
			for (const auto& ifs : m_EthernetInterfaces)
			{
				if (ifs.Operational)
				{
					for (const auto& ip : ifs.IPAddresses)
					{
						const auto it = std::find_if(allips.begin(), allips.end(), [&](const auto& ipd)
						{
							return (ipd.IPAddress == ip);
						});

						if (it == allips.end())
						{
							auto& ipdetails = allips.emplace_back();
							ipdetails.IPAddress = ip;
							ipdetails.BoundToLocalEthernetInterface = true;
						}
					}
				}
			}

			// Add any public IP addresses if we have them
			if (m_PublicIPEndpoints.AddIPAddresses(allips).Succeeded())
			{
				return std::move(allips);
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not get IP addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}
}