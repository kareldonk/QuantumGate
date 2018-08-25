// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation
{
	// Inspired by ScopeGuard by Andrei Alexandrescu
	// https://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C

	template<typename F>
	class ScopeGuard
	{
	public:
		ScopeGuard() = delete;
		constexpr ScopeGuard(std::nullptr_t) noexcept {}
		constexpr ScopeGuard(F&& func) noexcept : m_Function(std::move(func)), m_Active(true) {}
		ScopeGuard(const ScopeGuard&) = delete;
		
		constexpr ScopeGuard(ScopeGuard&& other) noexcept :
			m_Function(std::move(other.m_Function)), m_Active(other.m_Active)
		{
			other.m_Active = false;
		}
		
		ScopeGuard& operator=(const ScopeGuard&) = delete;

		constexpr ScopeGuard& operator=(ScopeGuard&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Function = std::move(other.m_Function);
			m_Active = std::exchange(other.m_Active, false);
			return *this;
		}

		~ScopeGuard()
		{
			if (m_Active)
			{
				try { m_Function(); }
				catch (...) {}
			}
		}

		constexpr const bool IsActive() const noexcept { return m_Active; }
		constexpr void Activate() noexcept { m_Active = true; }
		constexpr void Deactivate() noexcept { m_Active = false; }

	private:
		F m_Function;
		bool m_Active{ false };
	};

	template<typename F>
	constexpr ScopeGuard<F> MakeScopeGuard(F&& func) noexcept { return ScopeGuard<F>(std::move(func)); }
}