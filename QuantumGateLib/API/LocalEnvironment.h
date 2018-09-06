// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

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
		Result<std::vector<EthernetInterface>> GetEthernetInterfaces() const noexcept;

	private:
		LocalEnvironment(const void* localenv) noexcept;

	private:
		const void* m_LocalEnvironment{ nullptr };
	};
}