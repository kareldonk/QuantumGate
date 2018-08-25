// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\API\Extender.h"

namespace QuantumGate::Implementation::Core::Extender
{
	using ExtenderModuleID = UInt64;
	using ExtenderModuleHandle = HMODULE;
	using ExtendersVector = std::vector<std::shared_ptr<QuantumGate::API::Extender>>;

	class Module final
	{
	public:
		Module(const Path& module_path) noexcept;
		Module(const Module&) = delete;
		Module(Module&& other) noexcept;
		virtual ~Module();
		Module& operator=(const Module&) = delete;
		Module& operator=(Module&& other) noexcept;

		[[nodiscard]] inline const bool IsLoaded() const noexcept { return m_Handle != nullptr; }

		const ExtenderModuleID GetID() const noexcept;

		const ExtendersVector& GetExtenders() const noexcept;

	private:
		const bool LoadModule(const Path& module_path) noexcept;
		void ReleaseModule() noexcept;

	private:
		using MakeExtenderFunctionType = Result<ExtendersVector>(*)();

		ExtenderModuleID m_ID{ 0 };
		ExtenderModuleHandle m_Handle{ nullptr };
		ExtendersVector m_Extenders;
	};
}