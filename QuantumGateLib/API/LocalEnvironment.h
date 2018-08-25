// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core
{
	class LocalEnvironment;
}

namespace QuantumGate::API
{
	class Export LocalEnvironment
	{
		friend class Local;

	public:
		LocalEnvironment() = delete;
		LocalEnvironment(const LocalEnvironment&) = delete;
		LocalEnvironment(LocalEnvironment&&) = default;
		virtual ~LocalEnvironment() = default;
		LocalEnvironment& operator=(const LocalEnvironment&) = delete;
		LocalEnvironment& operator=(LocalEnvironment&&) = default;

		Result<String> GetHostname() const noexcept;
		Result<String> GetUsername() const noexcept;
		Result<const std::vector<EthernetInterface>*> GetEthernetInterfaces() const noexcept;

	private:
		LocalEnvironment(const QuantumGate::Implementation::Core::LocalEnvironment* localenv) noexcept;

	private:
		const QuantumGate::Implementation::Core::LocalEnvironment* m_LocalEnvironment{ nullptr };
	};
}