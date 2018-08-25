// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "LocalEnvironment.h"
#include "..\Core\LocalEnvironment.h"

namespace QuantumGate::API
{
	LocalEnvironment::LocalEnvironment(const QuantumGate::Implementation::Core::LocalEnvironment* localenv) noexcept :
		m_LocalEnvironment(localenv)
	{
		assert(m_LocalEnvironment != nullptr);
	}

	Result<String> LocalEnvironment::GetHostname() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		if (m_LocalEnvironment->IsInitialized()) return m_LocalEnvironment->Hostname();

		return ResultCode::Failed;
	}

	Result<String> LocalEnvironment::GetUsername() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		if (m_LocalEnvironment->IsInitialized()) return m_LocalEnvironment->Username();

		return ResultCode::Failed;
	}
	
	Result<const std::vector<EthernetInterface>*> LocalEnvironment::GetEthernetInterfaces() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		if (m_LocalEnvironment->IsInitialized()) return &m_LocalEnvironment->EthernetInterfaces();

		return ResultCode::Failed;
	}
}