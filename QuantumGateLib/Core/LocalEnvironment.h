// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PublicIPEndpoints.h"

namespace QuantumGate::Implementation::Core
{
	class LocalEnvironment
	{
		using CachedIPAddresses_ThS =
			Concurrency::ThreadLocalCache<std::vector<BinaryIPAddress>, Concurrency::SpinMutex, 369>;

	public:
		LocalEnvironment() = default;
		LocalEnvironment(const LocalEnvironment&) = delete;
		LocalEnvironment(LocalEnvironment&&) = default;
		virtual ~LocalEnvironment() = default;
		LocalEnvironment& operator=(const LocalEnvironment&) = delete;
		LocalEnvironment& operator=(LocalEnvironment&&) = default;

		[[nodiscard]] const bool Initialize() noexcept;
		inline const bool IsInitialized() const noexcept { return m_Initialized; }
		void Clear() noexcept;

		inline const String& GetHostname() const noexcept { return m_Hostname; }
		inline const String& GetUsername() const noexcept { return m_Username; }
		Result<std::vector<IPAddressDetails>> GetIPAddresses() const noexcept;
		inline const std::vector<EthernetInterface>& GetEthernetInterfaces() const noexcept { return m_EthernetInterfaces; }

		const std::vector<BinaryIPAddress>* GetCachedIPAddresses() const noexcept;

		String GetIPAddressesString() const noexcept;
		String GetMACAddressesString() const noexcept;

		[[nodiscard]] const bool AddPublicIPEndpoint(const IPEndpoint& pub_endpoint,
													 const IPEndpoint& rep_peer,
													 const PeerConnectionType rep_con_type,
													 const bool trusted) noexcept;
	
	private:
		[[nodiscard]] const bool UpdateCachedIPAddresses() noexcept;

		static Result<String> OSGetHostname() noexcept;
		static Result<String> OSGetUsername() noexcept;
		static Result<std::vector<EthernetInterface>> OSGetEthernetInterfaces() noexcept;
		static Result<std::vector<BinaryIPAddress>> OSGetIPAddresses(const String& hostname) noexcept;

	private:
		bool m_Initialized{ false };
		String m_Hostname;
		String m_Username;
		std::vector<EthernetInterface> m_EthernetInterfaces;
		
		PublicIPEndpoints m_PublicIPEndpoints;

		CachedIPAddresses_ThS m_CachedIPAddresses;
	};

	using LocalEnvironment_ThS = Concurrency::ThreadSafe<LocalEnvironment, std::shared_mutex>;
}