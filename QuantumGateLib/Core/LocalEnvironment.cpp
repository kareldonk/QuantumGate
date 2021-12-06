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

		if (!UpdateEnvironmentInformation(false)) return false;

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

		if (UpdateEnvironmentInformation(force_update))
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
			constexpr ULONG family{ AF_UNSPEC };

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

	Result<> LocalEnvironment::OSGetBluetoothDevices(Vector<API::Local::Environment::BluetoothDevice>& devices,
													 const bool refresh) noexcept
	{
		try
		{
			DWORD query_set_len{ sizeof(WSAQUERYSET) };

			auto query_set = reinterpret_cast<WSAQUERYSET*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, query_set_len));
			if (query_set != nullptr)
			{
				// Free resources when we return
				const auto sg = MakeScopeGuard([&]() noexcept { if (query_set != nullptr) HeapFree(GetProcessHeap(), 0, query_set); });

				HANDLE lookup_handle{ nullptr };
				auto lookup_flags = LUP_CONTAINERS | LUP_RETURN_NAME | LUP_RETURN_TYPE | LUP_RETURN_ADDR;

				if (refresh) lookup_flags |= LUP_FLUSHCACHE;

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
								const auto raddr = BTHAddress(query_set->lpcsaBuffer->RemoteAddr.lpSockaddr);

								const auto it = std::find_if(devices.begin(), devices.end(), [&](const auto& device)
								{
									return (device.RemoteAddress == raddr);
								});

								if (it != devices.end())
								{
									it->ServiceClassID = *query_set->lpServiceClassId;
								}
								else
								{
									auto& bthdev = devices.emplace_back();
									bthdev.Name = query_set->lpszServiceInstanceName;
									bthdev.ServiceClassID = *query_set->lpServiceClassId;
									bthdev.RemoteAddress = raddr;
								}
							}
						}
						else
						{
							const auto error = WSAGetLastError();
							if (error == WSA_E_NO_MORE)
							{
								// No more data
								return ResultCode::Succeeded;
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
						return ResultCode::Succeeded;
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

	Result<std::pair<Vector<API::Local::Environment::BluetoothRadio>,
		Vector<API::Local::Environment::BluetoothDevice>>> LocalEnvironment::OSGetBluetoothRadios(const bool refresh) noexcept
	{
		try
		{
			Vector<API::Local::Environment::BluetoothRadio> allbthr;
			Vector<API::Local::Environment::BluetoothDevice> allbthd;

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
						bthradio.Connectable = BluetoothIsConnectable(radio_handle);
						bthradio.Discoverable = BluetoothIsDiscoverable(radio_handle);
						bthradio.Address = BTHAddress(BinaryBTHAddress(BinaryBTHAddress::Family::BTH, radio_info.address.ullLong));

						if (auto result = OSGetBluetoothDevicesForRadio(radio_handle, bthradio.Address, refresh); result.Succeeded())
						{
							for (auto& bthdev : *result)
							{
								allbthd.emplace_back(std::move(bthdev));
							}
						}
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
							return std::make_pair(std::move(allbthr), std::move(allbthd));
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
					return std::make_pair(std::move(allbthr), std::move(allbthd));
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

	Result<Vector<API::Local::Environment::BluetoothDevice>> LocalEnvironment::OSGetBluetoothDevicesForRadio(const HANDLE radio,
																											 const BTHAddress& local_bthaddr,
																											 const bool refresh) noexcept
	{
		try
		{
			Vector<API::Local::Environment::BluetoothDevice> allbthd;

			BLUETOOTH_DEVICE_INFO bthdinfo{ 0 };
			bthdinfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

			BLUETOOTH_DEVICE_SEARCH_PARAMS bthfparams;
			bthfparams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
			bthfparams.hRadio = radio;
			bthfparams.fReturnAuthenticated = true;
			bthfparams.fReturnConnected = true;
			bthfparams.fReturnRemembered = true;
			bthfparams.fReturnUnknown = true;
			bthfparams.fIssueInquiry = refresh;
			bthfparams.cTimeoutMultiplier = 1;

			auto find_handle = BluetoothFindFirstDevice(&bthfparams, &bthdinfo);
			if (find_handle != nullptr)
			{
				// End find when we return
				const auto sg = MakeScopeGuard([&]() noexcept { BluetoothFindDeviceClose(find_handle); });

				while (true)
				{
					auto& bthdev = allbthd.emplace_back();
					bthdev.Name = bthdinfo.szName;
					bthdev.ClassOfDevice = bthdinfo.ulClassofDevice;
					bthdev.LocalAddress = local_bthaddr;
					bthdev.RemoteAddress = BTHAddress(BinaryBTHAddress(BinaryBTHAddress::Family::BTH, bthdinfo.Address.ullLong));
					bthdev.Connected = bthdinfo.fConnected;
					bthdev.Remembered = bthdinfo.fRemembered;
					bthdev.Authenticated = bthdinfo.fAuthenticated;

					if (bthdinfo.stLastSeen.wYear > 1601)
					{
						bthdev.LastSeen = Util::ToTime(bthdinfo.stLastSeen);
					}

					if (bthdinfo.stLastUsed.wYear > 1601)
					{
						bthdev.LastUsed = Util::ToTime(bthdinfo.stLastUsed);
					}

					Vector<GUID> services;

					while (true)
					{
						services.resize(services.size() + 5);

						auto numservices = static_cast<DWORD>(services.size());

						const auto result = BluetoothEnumerateInstalledServices(radio, &bthdinfo, &numservices, services.data());
						if (result == ERROR_SUCCESS)
						{
							if (numservices > 0)
							{
								bthdev.Services.reserve(numservices);

								for (DWORD x = 0; x < numservices; ++x)
								{
									bthdev.Services.emplace_back(services[x]);
								}
							}
							break;
						}
						else if (result == ERROR_MORE_DATA)
						{
							continue;
						}
						else
						{
							const auto error = WSAGetLastError();
							if (error != 0)
							{
								LogErr(L"Could not get installed services for a Bluetooth device '%s'; BluetoothEnumerateInstalledServices() failed (%s)",
									   bthdinfo.szName, GetLastSysErrorString().c_str());
							}
							break;
						}
					}

					MemInit(&bthdinfo, sizeof(bthdinfo));
					bthdinfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

					if (!BluetoothFindNextDevice(find_handle, &bthdinfo))
					{
						const auto error = WSAGetLastError();
						if (error == ERROR_NO_MORE_ITEMS)
						{
							return std::move(allbthd);
						}
						else
						{
							LogErr(L"Could not get Bluetooth devices; BluetoothFindNextDevice() failed (%s)",
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
					return std::move(allbthd);
				}
				else LogErr(L"Could not get Bluetooth devices; BluetoothFindFirstDevice() failed (%s)",
							GetLastSysErrorString().c_str());
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not get Bluetooth devices due to exception: %s",
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
		assert(m_IPInterfaceChangeNotificationHandle == nullptr);

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
		if (m_IPInterfaceChangeNotificationHandle != nullptr)
		{
			if (CancelMibChangeNotify2(m_IPInterfaceChangeNotificationHandle) == NO_ERROR)
			{
				m_IPInterfaceChangeNotificationHandle = nullptr;
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
		assert(CallerContext != nullptr);

		LogDbg(L"Received IP interface change notification (%d) from OS", NotificationType);

		switch (NotificationType)
		{
			case MIB_NOTIFICATION_TYPE::MibAddInstance:
			case MIB_NOTIFICATION_TYPE::MibDeleteInstance:
			{
				auto le = static_cast<LocalEnvironment*>(CallerContext);
				le->m_UpdateRequired = true;
				le->m_ChangedCallback.WithUniqueLock([](auto& callback) noexcept
				{
					if (callback) callback();
				});
				break;
			}
			default:
			{
				break;
			}
		}
	}

	bool LocalEnvironment::UpdateEnvironmentInformation(const bool refresh) noexcept
	{
		if (auto result = OSGetHostname(); result.Failed()) return false;
		else m_Hostname = std::move(result.GetValue());

		if (auto result = OSGetUsername(); result.Failed()) return false;
		else m_Username = std::move(result.GetValue());

		if (auto result = OSGetEthernetInterfaces(); result.Failed()) return false;
		else m_EthernetInterfaces = std::move(result.GetValue());

		if (auto result = OSGetBluetoothRadios(refresh); result.Failed()) return false;
		else
		{
			m_BluetoothRadios = std::move(result.GetValue().first);
			m_BluetoothDevices = std::move(result.GetValue().second);
		}

		if (const auto result = OSGetBluetoothDevices(m_BluetoothDevices, refresh); result.Failed()) return false;

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
				if (radio.Connectable)
				{
					if (std::find(addrs.begin(), addrs.end(), radio.Address) == addrs.end())
					{
						addrs.emplace_back(radio.Address);
					}
				}
			}

			m_PublicEndpoints.SetLocallyBoundPublicIPAddress(has_public_ip);

			// Add any trusted/verified public addresses if we have them
			if (m_PublicEndpoints.AddAddresses(addrs, true).Succeeded())
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
			LogErr(L"Could not update cached addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return false;
	}

	Result<Vector<API::Local::Environment::AddressDetails>> LocalEnvironment::GetAddresses() const noexcept
	{
		try
		{
			Vector<API::Local::Environment::AddressDetails> alladdrs;

			// First add the local IP addresses configured on the host
			for (const auto& ifs : m_EthernetInterfaces)
			{
				if (ifs.Operational)
				{
					for (const auto& ip : ifs.IPAddresses)
					{
						const auto it = std::find_if(alladdrs.begin(), alladdrs.end(), [&](const auto& addrd)
						{
							return (addrd.Address == ip);
						});

						if (it == alladdrs.end())
						{
							auto& adetails = alladdrs.emplace_back();
							adetails.Address = ip;
							adetails.BoundToLocalInterface = true;
						}
					}
				}
			}

			// Add the local Bluetooth addresses configured on the host
			for (const auto& radio : m_BluetoothRadios)
			{
				if (radio.Connectable)
				{
					const auto it = std::find_if(alladdrs.begin(), alladdrs.end(), [&](const auto& addrd)
					{
						return (addrd.Address == radio.Address);
					});

					if (it == alladdrs.end())
					{
						auto& adetails = alladdrs.emplace_back();
						adetails.Address = radio.Address;
						adetails.BoundToLocalInterface = true;
					}
				}
			}

			// Add any public addresses if we have them
			if (m_PublicEndpoints.AddAddresses(alladdrs).Succeeded())
			{
				return std::move(alladdrs);
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Could not get addresses due to exception: %s",
				   Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return ResultCode::Failed;
	}
}