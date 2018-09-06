// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "LocalEnvironment.h"
#include "..\Core\LocalEnvironment.h"

using namespace QuantumGate::Implementation::Core;

namespace QuantumGate::API
{
	LocalEnvironment::LocalEnvironment(const void* localenv) noexcept :
		m_LocalEnvironment(localenv)
	{
		assert(m_LocalEnvironment != nullptr);
	}

	Result<String> LocalEnvironment::GetHostname() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		try
		{
			const auto local_env = reinterpret_cast<const LocalEnvironment_ThS*>(m_LocalEnvironment)->WithSharedLock();
			if (local_env->IsInitialized()) return local_env->GetHostname();
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<String> LocalEnvironment::GetUsername() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		try
		{
			const auto local_env = reinterpret_cast<const LocalEnvironment_ThS*>(m_LocalEnvironment)->WithSharedLock();
			if (local_env->IsInitialized()) return local_env->GetUsername();
		}
		catch (...) {}

		return ResultCode::Failed;
	}
	
	Result<std::vector<EthernetInterface>> LocalEnvironment::GetEthernetInterfaces() const noexcept
	{
		assert(m_LocalEnvironment != nullptr);

		try
		{
			const auto local_env = reinterpret_cast<const LocalEnvironment_ThS*>(m_LocalEnvironment)->WithSharedLock();
			if (local_env->IsInitialized()) return local_env->GetEthernetInterfaces();
		}
		catch (...) {}

		return ResultCode::Failed;
	}
}