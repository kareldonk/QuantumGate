// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "LocalEnvironment.h"
#include "..\Common\ScopeGuard.h"

namespace QuantumGate::Implementation::Core
{
	bool LocalEnvironment::Initialize(ChangedCallback&& callback) noexcept
	{
		assert(!IsInitialized());

		if (!m_PublicEndpoints.Initialize()) return false;

		// Upon failure deinit public IP endpoints when we return
		auto sg0 = MakeScopeGuard([&]() noexcept { m_PublicEndpoints.Deinitialize(); });

		if (!UpdateEnvironmentInformation()) return false;

		// Upon failure clear environment info when we return
		auto sg1 = MakeScopeGuard([&]() noexcept { ClearEnvironmentInformation(); });

		if (callback) m_ChangedCallback.WithUniqueLock() = std::move(callback);

		// Upon failure clear callback when we return
		auto sg2 = MakeScopeGuard([&]() noexcept { m_ChangedCallback.WithUniqueLock()->Clear(); });

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
		m_UpdateRequired = false;

		DeregisterIPInterfaceChangeNotification();

		ClearEnvironmentInformation();

		m_PublicEndpoints.Deinitialize();
	}

	bool LocalEnvironment::Update(const bool force_update) noexcept
	{
		assert(IsInitialized());

		if (!force_update && !m_UpdateRequired) return true;

		if (UpdateEnvironmentInformation())
		{
			m_UpdateRequired = false;
			return true;
		}

		return false;
	}

	const Vector<Network::Address>* LocalEnvironment::GetTrustedAndVerifiedAddresses() const noexcept
	{
		try
		{
			return &m_CachedAddresses.GetCache();
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

		LogErr(L"Could not get the name of the local host (%s)", GetLastSysErrorString().c_str());

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
				const auto sg = MakeScopeGuard([&]() noexcept { FreeAddrInfoW(result); });

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
			else LogErr(L"Could not get addresses for host %s (%s)",
						hostname.c_str(), GetLastSysErrorString().c_str());
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not get addresses for host %s due to exception: %s",
				   hostname.c_str(), Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<Vector<API::Local::Environment::EthernetInterface>> LocalEnvironment::OSGetEthernetInterfaces() noexcept
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
				Vector<API::Local::Environment::EthernetInterface> allifs;

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
			else LogErr(L"Could not get addresses for local networking adapters (%s)",
						GetLastSysErrorString().c_str());
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not get addresses for local networking adapters due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<Vector<API::Local::Environment::BluetoothDevice>> LocalEnvironment::OSGetBluetoothDevices() noexcept
	{
		try
		{
			DWORD query_set_len{ sizeof(WSAQUERYSET) };

			auto query_set = reinterpret_cast<WSAQUERYSET*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, query_set_len));
			if (query_set != nullptr)
			{
				// Free resources when we return
				const auto sg = MakeScopeGuard([&]() noexcept { if (query_set != nullptr) HeapFree(GetProcessHeap(), 0, query_set); });

				Vector<API::Local::Environment::BluetoothDevice> allbths;

				HANDLE lookup_handle{ nullptr };
				auto lookup_flags = LUP_CONTAINERS | LUP_RETURN_NAME | LUP_RETURN_TYPE | LUP_RETURN_ADDR;

				MemInit(query_set, 0);
				query_set->dwNameSpace = NS_BTH;
				query_set->dwSize = sizeof(WSAQUERYSET);

				if (WSALookupServiceBegin(query_set, lookup_flags, &lookup_handle) == 0 && lookup_handle != nullptr)
				{
					// End lookup when we return
					const auto sg2 = MakeScopeGuard([&]() noexcept { WSALookupServiceEnd(lookup_handle); });

					while (true)
					{
						if (WSALookupServiceNext(lookup_handle, lookup_flags, &query_set_len, query_set) == 0)
						{
							if (query_set->lpszServiceInstanceName != nullptr &&
								query_set->lpcsaBuffer->RemoteAddr.lpSockaddr->sa_family == AF_BTH)
							{
								auto& bthdev = allbths.emplace_back();
								bthdev.Name = query_set->lpszServiceInstanceName;
								bthdev.ServiceClassID = *query_set->lpServiceClassId;
								bthdev.RemoteAddress = BTHAddress(query_set->lpcsaBuffer->RemoteAddr.lpSockaddr);
							}
						}
						else
						{
							const auto error = WSAGetLastError();
							if (error == WSA_E_NO_MORE)
							{
								// No more data
								return std::move(allbths);
							}
							else if (error == WSAEFAULT)
							{
								// The buffer for QUERYSET was insufficient;
								// needed size is set in query_set_len now
								HeapFree(GetProcessHeap(), 0, query_set);
								query_set = reinterpret_cast<WSAQUERYSET*>(HeapAlloc(GetProcessHeap(),
																					 HEAP_ZERO_MEMORY, query_set_len));
								if (query_set == nullptr)
								{
									break;
								}
							}
							else LogErr(L"Could not get addresses for local Bluetooth devices; WSALookupServiceNext() failed (%s)",
										GetLastSysErrorString().c_str());
						}
					}
				}
				else
				{
					const auto error = WSAGetLastError();
					if (error == WSASERVICE_NOT_FOUND)
					{
						// Bluetooth is off or there are no devices
						return std::move(allbths);
					}
					else LogErr(L"Could not get addresses for local Bluetooth devices; WSALookupServiceBegin() failed (%s)",
								GetLastSysErrorString().c_str());
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not get addresses for local Bluetooth devices due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<Vector<API::Local::Environment::BluetoothRadio>> LocalEnvironment::OSGetBluetoothRadios() noexcept
	{
		try
		{
			Vector<API::Local::Environment::BluetoothRadio> allbthr;

			BLUETOOTH_FIND_RADIO_PARAMS bthfparams;
			bthfparams.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS);

			HANDLE radio_handle{ nullptr };
			auto find_handle = BluetoothFindFirstRadio(&bthfparams, &radio_handle);
			if (find_handle != nullptr)
			{
				// End find when we return
				const auto sg = MakeScopeGuard([&]() noexcept { BluetoothFindRadioClose(find_handle); });

				while (true)
				{
					BLUETOOTH_RADIO_INFO radio_info{ 0 };
					radio_info.dwSize = sizeof(BLUETOOTH_RADIO_INFO);

					if (BluetoothGetRadioInfo(radio_handle, &radio_info) == ERROR_SUCCESS)
					{
						auto& bthradio = allbthr.emplace_back();
						bthradio.Name = radio_info.szName;
						bthradio.ManufacturerID = radio_info.manufacturer;
						bthradio.IsConnectable = BluetoothIsConnectable(radio_handle);
						bthradio.IsDiscoverable = BluetoothIsDiscoverable(radio_handle);
						bthradio.Address = BTHAddress(BinaryBTHAddress(BinaryBTHAddress::Family::BTH, radio_info.address.ullLong));
					}
					else
					{
						LogErr(L"Could not get information for Bluetooth radio; BluetoothGetRadioInfo() failed (%s)",
							   GetLastSysErrorString().c_str());
						break;
					}

					if (!BluetoothFindNextRadio(find_handle, &radio_handle))
					{
						const auto error = WSAGetLastError();
						if (error == ERROR_NO_MORE_ITEMS)
						{
							return std::move(allbthr);
						}
						else
						{
							LogErr(L"Could not get local Bluetooth radios; BluetoothFindNextRadio() failed (%s)",
								   GetLastSysErrorString().c_str());
							break;
						}
					}
				}
			}
			else
			{
				const auto error = WSAGetLastError();
				if (error == ERROR_NO_MORE_ITEMS)
				{
					return std::move(allbthr);
				}
				else LogErr(L"Could not get local Bluetooth radios; BluetoothFindFirstRadio() failed (%s)",
							GetLastSysErrorString().c_str());
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not get local Bluetooth radios due to exception: %s",
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

		LogErr(L"Could not get the username for the current user (%s)", GetLastSysErrorString().c_str());

		return ResultCode::Failed;
	}

	bool LocalEnvironment::AddPublicEndpoint(const Endpoint& pub_endpoint, const Endpoint& rep_peer,
											 const PeerConnectionType rep_con_type, const bool trusted) noexcept
	{
		if (const auto result = m_PublicEndpoints.AddEndpoint(pub_endpoint, rep_peer, rep_con_type, trusted);
			result.Succeeded())
		{
			if (result->first && result->second)
			{
				// New address was added; update cache
				DiscardReturnValue(UpdateCachedAddresses());
			}

			return true;
		}

		return false;
	}

	bool LocalEnvironment::RegisterIPInterfaceChangeNotification() noexcept
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

		auto le = reinterpret_cast<LocalEnvironment*>(CallerContext);
		le->m_UpdateRequired = true;
		le->m_ChangedCallback.WithUniqueLock([](auto& callback)
		{
			if (callback) callback();
		});
	}

	bool LocalEnvironment::UpdateEnvironmentInformation() noexcept
	{
		if (auto result = OSGetHostname(); result.Failed()) return false;
		else m_Hostname = std::move(result.GetValue());

		if (auto result = OSGetUsername(); result.Failed()) return false;
		else m_Username = std::move(result.GetValue());

		if (auto result = OSGetEthernetInterfaces(); result.Failed()) return false;
		else m_EthernetInterfaces = std::move(result.GetValue());

		if (auto result = OSGetBluetoothRadios(); result.Failed()) return false;
		else m_BluetoothRadios = std::move(result.GetValue());

		if (auto result = OSGetBluetoothDevices(); result.Failed()) return false;
		else m_BluetoothDevices = std::move(result.GetValue());

		if (!UpdateCachedAddresses()) return false;

		return true;
	}

	void LocalEnvironment::ClearEnvironmentInformation() noexcept
	{
		m_Hostname.clear();
		m_Username.clear();
		m_EthernetInterfaces.clear();
		m_BluetoothRadios.clear();
		m_BluetoothDevices.clear();

		m_CachedAddresses.UpdateValue([](auto& addresses) noexcept
		{
			addresses.clear();
		});

		return;
	}

	bool LocalEnvironment::UpdateCachedAddresses() noexcept
	{
		try
		{
			auto has_public_ip = false;
			Vector<Network::Address> addrs;

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

						if (std::find(addrs.begin(), addrs.end(), ip) == addrs.end())
						{
							addrs.emplace_back(ip);
						}
					}
				}
			}

			// Add the local Bluetooth addresses configured on the host
			for (const auto& radio : m_BluetoothRadios)
			{
				if (radio.IsConnectable)
				{
					if (std::find(addrs.begin(), addrs.end(), radio.Address) == addrs.end())
					{
						addrs.emplace_back(radio.Address);
					}
				}
			}

			m_PublicEndpoints.SetLocallyBoundPublicIPAddress(has_public_ip);

			// Add any trusted/verified public IP addresses if we have them
			if (m_PublicEndpoints.AddIPAddresses(addrs, true).Succeeded())
			{
				m_CachedAddresses.UpdateValue([&](auto& addresses) noexcept
				{
					addresses = std::move(addrs);
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

	Result<Vector<API::Local::Environment::IPAddressDetails>> LocalEnvironment::GetIPAddresses() const noexcept
	{
		try
		{
			Vector<API::Local::Environment::IPAddressDetails> allips;

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
			if (m_PublicEndpoints.AddIPAddresses(allips).Succeeded())
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