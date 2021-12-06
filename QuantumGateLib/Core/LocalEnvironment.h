// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PublicEndpoints.h"

namespace QuantumGate::Implementation::Core
{
	class LocalEnvironment final
	{
		using CachedAddresses_ThS =
			Concurrency::ThreadLocalCache<Vector<Network::Address>, Concurrency::SpinMutex, 369>;

	public:
		using ChangedCallback = Callback<void() noexcept>;
		using ChangedCallback_ThS = Concurrency::ThreadSafe<ChangedCallback, std::mutex>;

		LocalEnvironment(const Settings_CThS& settings) noexcept :
			m_PublicEndpoints(settings)
		{}

		LocalEnvironment(const LocalEnvironment&) = delete;
		LocalEnvironment(LocalEnvironment&&) noexcept = default;
		~LocalEnvironment() { if (IsInitialized()) Deinitialize(); }
		LocalEnvironment& operator=(const LocalEnvironment&) = delete;
		LocalEnvironment& operator=(LocalEnvironment&&) noexcept = default;

		[[nodiscard]] bool Initialize(ChangedCallback&& callback) noexcept;
		[[nodiscard]] inline bool IsInitialized() const noexcept { return m_Initialized; }
		void Deinitialize() noexcept;

		[[nodiscard]] bool Update(const bool force_update = false) noexcept;

		inline const String& GetHostname() const noexcept { return m_Hostname; }
		inline const String& GetUsername() const noexcept { return m_Username; }
		Result<Vector<API::Local::Environment::AddressDetails>> GetAddresses() const noexcept;
		inline const Vector<API::Local::Environment::EthernetInterface>& GetEthernetInterfaces() const noexcept { return m_EthernetInterfaces; }
		inline const Vector<API::Local::Environment::BluetoothDevice>& GetBluetoothDevices() const noexcept { return m_BluetoothDevices; }
		inline const Vector<API::Local::Environment::BluetoothRadio>& GetBluetoothRadios() const noexcept { return m_BluetoothRadios; }

		const Vector<Network::Address>* GetTrustedAndVerifiedAddresses() const noexcept;

		String GetIPAddressesString() const noexcept;
		String GetMACAddressesString() const noexcept;

		[[nodiscard]] bool AddPublicEndpoint(const Endpoint& pub_endpoint, const Endpoint& rep_peer,
											 const PeerConnectionType rep_con_type, const bool trusted) noexcept;

	private:
		[[nodiscard]] bool RegisterIPInterfaceChangeNotification() noexcept;
		void DeregisterIPInterfaceChangeNotification() noexcept;

		static VOID NETIOAPI_API_ IPInterfaceChangeNotificationCallback(PVOID CallerContext, PMIB_IPINTERFACE_ROW Row,
																		MIB_NOTIFICATION_TYPE NotificationType);

		[[nodiscard]] bool UpdateEnvironmentInformation(const bool refresh) noexcept;
		void ClearEnvironmentInformation() noexcept;

		[[nodiscard]] bool UpdateCachedAddresses() noexcept;

		static Result<String> OSGetHostname() noexcept;
		static Result<String> OSGetUsername() noexcept;
		static Result<Vector<API::Local::Environment::EthernetInterface>> OSGetEthernetInterfaces() noexcept;
		static Result<> OSGetBluetoothDevices(Vector<API::Local::Environment::BluetoothDevice>& devices, const bool refresh) noexcept;
		static Result<std::pair<Vector<API::Local::Environment::BluetoothRadio>,
			Vector<API::Local::Environment::BluetoothDevice>>> OSGetBluetoothRadios(const bool refresh) noexcept;
		static Result<Vector<API::Local::Environment::BluetoothDevice>> OSGetBluetoothDevicesForRadio(const HANDLE radio,
																									  const BTHAddress& local_bthaddr,
																									  const bool refresh) noexcept;
		static Result<Vector<BinaryIPAddress>> OSGetIPAddresses(const String& hostname) noexcept;

	private:
		bool m_Initialized{ false };
		bool m_UpdateRequired{ false };

		ChangedCallback_ThS m_ChangedCallback;

		HANDLE m_IPInterfaceChangeNotificationHandle{ nullptr };

		String m_Hostname;
		String m_Username;
		Vector<API::Local::Environment::EthernetInterface> m_EthernetInterfaces;
		Vector<API::Local::Environment::BluetoothRadio> m_BluetoothRadios;
		Vector<API::Local::Environment::BluetoothDevice> m_BluetoothDevices;

		PublicEndpoints m_PublicEndpoints;

		CachedAddresses_ThS m_CachedAddresses;
	};

	using LocalEnvironment_ThS = Concurrency::ThreadSafe<LocalEnvironment, std::shared_mutex>;
}