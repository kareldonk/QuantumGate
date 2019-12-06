// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Containers.h"
#include "Callback.h"

#include <optional>

namespace QuantumGate::Implementation
{
	template<typename Sig>
	class Dispatcher final
	{
	public:
		using FunctionType = Callback<Sig>;
	private:
		using FunctionList = Containers::List<FunctionType>;
	public:
		using FunctionHandle = std::optional<typename FunctionList::const_iterator>;

		Dispatcher() = default;
		Dispatcher(const Dispatcher&) = delete;
		Dispatcher(Dispatcher&&) = default;
		~Dispatcher() = default;
		Dispatcher& operator=(const Dispatcher&) = delete;
		Dispatcher& operator=(Dispatcher&&) = default;

		template<typename... Args, typename Sig2 = Sig,
			typename = std::enable_if_t<!FunctionSignatureIsConstV<Sig2>>>
		ForceInline void operator()(Args... args) noexcept(FunctionSignatureIsNoexceptV<Sig2>)
		{
			for (auto& function : m_Functions)
			{
				function(std::forward<Args>(args)...);
			}
		}

		template<typename... Args, typename Sig2 = Sig,
			typename = std::enable_if_t<FunctionSignatureIsConstV<Sig2>>>
		ForceInline void operator()(Args... args) const noexcept(FunctionSignatureIsNoexceptV<Sig2>)
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

		FunctionHandle Add(FunctionType&& function) noexcept
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

		void Remove(FunctionHandle& func_handle) noexcept
		{
			if (func_handle)
			{
				m_Functions.erase(*func_handle);
				func_handle = std::nullopt;
			}
		}

		void Clear() noexcept { m_Functions.clear(); }

	private:
		FunctionList m_Functions;
	};
}