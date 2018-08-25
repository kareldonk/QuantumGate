// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core
{
	class LocalEnvironment
	{
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

		inline const String& Hostname() const noexcept { return m_Hostname; }
		inline const String& Username() const noexcept { return m_Username; }
		inline const std::vector<EthernetInterface>& EthernetInterfaces() const noexcept { return m_Interfaces; }
		inline const std::vector<IPAddress>& IPAddresses() const noexcept { return m_IPAddresses; }

		String GetIPAddressesString() const noexcept;
		String GetMACAddressesString() const noexcept;
	
	protected:
		static Result<std::vector<IPAddress>> GetIPAddresses(const String& hostname) noexcept;
		static Result<std::vector<IPAddress>> GetIPAddresses(const std::vector<EthernetInterface>& eth_interfaces) noexcept;
		static Result<std::vector<EthernetInterface>> GetEthernetInterfaces() noexcept;
		static Result<String> GetHostname() noexcept;
		static Result<String> GetUsername() noexcept;

	private:
		bool m_Initialized{ false };
		String m_Hostname;
		String m_Username;
		std::vector<IPAddress> m_IPAddresses;
		std::vector<EthernetInterface> m_Interfaces;
	};
}