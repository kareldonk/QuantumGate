// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Callback.h"
#include <optional>

namespace QuantumGate::Implementation
{
	template<typename T, bool NoExcept = false>
	class DispatcherBase;

	template<typename Ret, typename... Args, bool NoExcept>
	class DispatcherBase<Ret(Args...), NoExcept>
	{
	public:
		using FunctionType = std::conditional_t<NoExcept, Callback<Ret(Args...) noexcept>, Callback<Ret(Args...)>>;
	private:
		using FunctionList = std::list<FunctionType>;
	public:
		using FunctionHandle = std::optional<typename FunctionList::const_iterator>;

		DispatcherBase() = default;
		DispatcherBase(const DispatcherBase&) = delete;
		DispatcherBase(DispatcherBase&&) = default;
		virtual ~DispatcherBase() = default;
		DispatcherBase& operator=(const DispatcherBase&) = delete;
		DispatcherBase& operator=(DispatcherBase&&) = default;

		ForceInline void operator()(Args... args) noexcept(NoExcept)
		{
			for (auto& function : m_Functions)
			{
				function(std::forward<Args>(args)...);
			}
		}

		ForceInline void operator()(Args... args) const noexcept(NoExcept)
		{
			for (const auto& function : m_Functions)
			{
				function(std::forward<Args>(args)...);
			}
		}

		explicit operator bool() const noexcept
		{
			// If we have any valid targets
			return (!m_Functions.empty());
		}

		const FunctionHandle Add(FunctionType&& function) noexcept
		{
			// If we have a valid target
			if (function)
			{
				try
				{
					m_Functions.emplace_back(std::move(function));
					auto it = m_Functions.end();
					--it;
					return it;
				}
				catch (...) {}
			}

			return std::nullopt;
		}

		const void Remove(FunctionHandle& func_handle) noexcept
		{
			if (func_handle)
			{
				m_Functions.erase(*func_handle);
				func_handle = std::nullopt;
			}
		}

		void Clear() noexcept { m_Functions.clear(); }

	private:
		std::list<FunctionType> m_Functions;
	};

	template<typename Sig>
	class Dispatcher : public DispatcherBase<function_signature_rm_const_noexcept_t<Sig>,
											function_signature_is_noexcept<Sig>>
	{};
}