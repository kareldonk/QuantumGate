// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "ExtenderModule.h"
#include "..\..\Common\Hash.h"

namespace QuantumGate::Implementation::Core::Extender
{
	Module::Module(const Path& module_path) noexcept
	{
		LoadModule(module_path);
	}

	Module::Module(Module&& other) noexcept
	{
		*this = std::move(other);
	}

	Module::~Module()
	{
		ReleaseModule();
	}

	Module& Module::operator=(Module&& other) noexcept
	{
		m_ID = std::exchange(other.m_ID, 0);
		m_Handle = std::exchange(other.m_Handle, nullptr);
		m_Extenders = std::exchange(other.m_Extenders, ExtendersVector());

		return *this;
	}

	const ExtenderModuleID Module::GetID() const noexcept
	{
		assert(IsLoaded());

		return m_ID;
	}

	const ExtendersVector& Module::GetExtenders() const noexcept
	{
		assert(IsLoaded());

		return m_Extenders;
	}

	const bool Module::LoadModule(const Path& module_path) noexcept
	{
		assert(!IsLoaded());

		auto success = false;

		m_Handle = LoadLibrary(module_path.c_str());
		if (m_Handle != nullptr)
		{
			auto func = reinterpret_cast<MakeExtenderFunctionType>(GetProcAddress(m_Handle,
																				  "MakeQuantumGateExtenders"));
			if (func != nullptr)
			{
				try
				{
					auto result = func();
					if (result.Succeeded())
					{
						m_Extenders = std::move(*result);

						m_ID = Hash::GetPersistentHash(BufferView(reinterpret_cast<const Byte*>(&m_Handle),
																  sizeof(m_Handle)));

						success = true;
					}
					else
					{
						LogErr(L"Failed to get extenders from module %s (%s)",
							   module_path.c_str(), result.GetErrorString().c_str());
					}
				}
				catch (const std::exception& e)
				{
					LogErr(L"Exception while getting extenders from module %s: %s",
						   module_path.c_str(), Util::ToStringW(e.what()).c_str());
				}
			}
			else LogErr(L"Could not find address of MakeQuantumGateExtenders function in module %s", module_path.c_str());
		}
		else LogErr(L"Could not load module %s", module_path.c_str());

		if (!success) ReleaseModule();

		return success;
	}

	void Module::ReleaseModule() noexcept
	{
		if (IsLoaded())
		{
			// Release extenders if we have any
			// before releasing module, otherwise
			// explosions are highly likely
			m_Extenders.clear();

			FreeLibrary(m_Handle);
		}

		m_ID = 0;
		m_Handle = nullptr;
	}
}