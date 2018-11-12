// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Traits.h"

#include <functional>
#include <algorithm>

namespace QuantumGate::Implementation
{
	class CallbackImplBase
	{};

	template<typename T, bool Const, bool NoExcept>
	class CallbackImpl;

	template<typename R, typename... Args, bool Const, bool NoExcept>
	class CallbackImpl<R(Args...), Const, NoExcept> : public CallbackImplBase
	{
		class CallbackFunction
		{
		public:
			virtual ~CallbackFunction() = default;

			virtual R operator()(Args...) const = 0;
		};

		template<typename F>
		class FreeCallbackFunction : public CallbackFunction
		{
		public:
			FreeCallbackFunction(F&& function) noexcept : m_Function(std::move(function)) {}

		private:
			ForceInline R operator()(Args... args) const noexcept(NoExcept) override
			{
				if constexpr (std::is_pointer_v<F>)
				{
					assert(m_Function != nullptr);
				}

				if constexpr (std::is_pointer_v<F> || Const)
				{
					return (m_Function)(std::forward<Args>(args)...);
				}
				else if constexpr (!Const)
				{
					// Allowed to modify because of non-const signature
					return (const_cast<F&>(m_Function))(std::forward<Args>(args)...);
				}
			}

		private:
			F m_Function{ nullptr };
		};

		template<typename T, typename F>
		class MemberCallbackFunction : public CallbackFunction
		{
		public:
			MemberCallbackFunction(T* object, F member_function_ptr) noexcept :
				m_Object(object), m_Function(member_function_ptr) {}

		private:
			ForceInline R operator()(Args... args) const noexcept(NoExcept) override
			{
				assert(m_Object != nullptr && m_Function != nullptr);

				return (m_Object->*m_Function)(std::forward<Args>(args)...);
			}

		private:
			T* m_Object{ nullptr };
			F m_Function{ nullptr };
		};

	public:
		CallbackImpl() noexcept {}
		CallbackImpl(std::nullptr_t) noexcept {}

		template<typename F>
		CallbackImpl(F&& function) noexcept(sizeof(FreeCallbackFunction<F>) <= sizeof(m_FunctionStorage))
		{
			if constexpr (NoExcept)
			{
				static_assert(std::is_nothrow_invocable_r_v<R,
							  std::conditional_t<Const, std::add_const_t<F>, F>, Args...>,
							  "Function parameter does not have the expected signature.");
			}
			else
			{
				static_assert(std::is_invocable_r_v<R,
							  std::conditional_t<Const, std::add_const_t<F>, F>, Args...>,
							  "Function parameter does not have the expected signature.");
			}

			if constexpr (sizeof(FreeCallbackFunction<F>) > sizeof(m_FunctionStorage))
			{
				m_Function = new FreeCallbackFunction<F>(std::move(function));
				m_Heap = true;
			}
			else m_Function = new (&m_FunctionStorage) FreeCallbackFunction<F>(std::move(function));
		}

		template<typename T, typename F>
		CallbackImpl(T* object, F member_function_ptr) noexcept
		{
			if constexpr (NoExcept)
			{
				static_assert(std::is_nothrow_invocable_r_v<R,
							  std::conditional_t<Const, std::add_const_t<F>, F>, T*, Args...>,
							  "Function parameter does not have the expected signature.");
			}
			else
			{
				static_assert(std::is_invocable_r_v<R,
							  std::conditional_t<Const, std::add_const_t<F>, F>, T*, Args...>,
							  "Function parameter does not have the expected signature.");
			}

			static_assert(sizeof(MemberCallbackFunction<T, F>) <= sizeof(m_FunctionStorage),
						  "Type is too large for FunctionStorage variable; increase size.");

			m_Function = new (&m_FunctionStorage) MemberCallbackFunction<T, F>(object, member_function_ptr);
		}

		virtual ~CallbackImpl()
		{
			Release();
		}

		CallbackImpl(const CallbackImpl&) = delete;

		CallbackImpl(CallbackImpl&& other) noexcept { *this = std::move(other); }

		CallbackImpl& operator=(const CallbackImpl&) = delete;

		CallbackImpl& operator=(CallbackImpl&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			// Release current before copying
			Release();

			if (other)
			{
				if (other.m_Heap)
				{
					m_Function = other.m_Function;
					m_Heap = other.m_Heap;
				}
				else
				{
					m_FunctionStorage = other.m_FunctionStorage;
					m_Function = reinterpret_cast<CallbackFunction*>(&m_FunctionStorage);
					m_Heap = false;
				}

				other.Reset();
			}
			else Reset();

			return *this;
		}

		template<bool Const2 = Const, typename = std::enable_if_t<!Const2>>
		ForceInline R operator()(Args... args) noexcept(NoExcept)
		{
			assert(m_Function != nullptr);

			return (*m_Function)(std::forward<Args>(args)...);
		}

		template<bool Const2 = Const, typename = std::enable_if_t<Const2>>
		ForceInline R operator()(Args... args) const noexcept(NoExcept)
		{
			assert(m_Function != nullptr);

			return (*m_Function)(std::forward<Args>(args)...);
		}

		ForceInline explicit operator bool() const noexcept
		{
			return (m_Function != nullptr);
		}

		inline void Clear() noexcept
		{
			Release();
			Reset();
		}

	private:
		ForceInline void Release() noexcept
		{
			if (m_Function != nullptr)
			{
				if (m_Heap) delete m_Function;
				//else m_Function->~CallbackFunction(); No need to call; there's nothing to destroy
			}
		}

		ForceInline void Reset() noexcept
		{
			if (!m_Heap) std::memset(&m_FunctionStorage, 0, sizeof(m_FunctionStorage));

			m_Function = nullptr;
			m_Heap = false;
		}

	private:
		CallbackFunction* m_Function{ nullptr };
		typename std::aligned_storage<24>::type m_FunctionStorage{ 0 };
		bool m_Heap{ false };
	};

	template<typename Sig>
	class Callback : public CallbackImpl<FunctionSignatureRemoveConstNoexceptT<Sig>,
		FunctionSignatureIsConstV<Sig>, FunctionSignatureIsNoexceptV<Sig>>
	{
	public:
		Callback() noexcept {}
		Callback(std::nullptr_t) noexcept {}

		template<typename F>
		Callback(F&& function) noexcept(noexcept(CallbackImpl<FunctionSignatureRemoveConstNoexceptT<Sig>,
												 FunctionSignatureIsConstV<Sig>,
												 FunctionSignatureIsNoexceptV<Sig>>(std::move(function)))) :
			CallbackImpl<FunctionSignatureRemoveConstNoexceptT<Sig>,
			FunctionSignatureIsConstV<Sig>, FunctionSignatureIsNoexceptV<Sig>>(std::move(function))
		{
			static_assert(!std::is_base_of_v<CallbackImplBase, F>,
						  "Attempt to pass in Callback object which is not allowed.");

			if constexpr (std::is_pointer_v<F>)
			{
				static_assert(std::is_same_v<Sig, FunctionSignatureT<std::decay_t<F>>>,
							  "Function parameter does not have the expected signature. "
							  "Remember to check for matching const and noexcept specifiers.");
			}
			else
			{
				static_assert(CheckFunctionCallOperatorSignature<Sig, F>(),
							  "Function parameter does not have an operator() overload with the expected signature. "
							  "Remember to check for matching const and noexcept specifiers. Lambda functions need "
							  "to be defined as mutable if the function signature does not have the const specifier.");
			}
		}

		template<typename T, typename F>
		Callback(T* object, F member_function_ptr) noexcept :
			CallbackImpl<FunctionSignatureRemoveConstNoexceptT<Sig>,
			FunctionSignatureIsConstV<Sig>, FunctionSignatureIsNoexceptV<Sig>>(object, member_function_ptr)
		{
			static_assert(std::is_same_v<Sig, FunctionSignatureT<std::decay_t<F>>>,
						  "Function parameter does not have the expected signature. "
						  "Remember to check for matching const and noexcept specifiers.");
		}

		~Callback() = default;
		Callback(const Callback&) = delete;
		Callback(Callback&& other) noexcept = default;
		Callback& operator=(const Callback&) = delete;
		Callback& operator=(Callback&& other) noexcept = default;
	};

	template<typename F>
	inline auto MakeCallback(F&& function) noexcept(std::is_pointer_v<F>)
	{
		if constexpr (std::is_pointer_v<F>)
		{
			// For function pointers
			return Callback<FunctionSignatureT<F>>(std::move(function));
		}
		else
		{
			// For lambdas and other objects overloading operator()
			using objtype = typename std::decay_t<F>;
			using funcsig = FunctionSignatureT<decltype(&objtype::operator())>;
			return Callback<funcsig>(std::move(function));
		}
	}

	template<typename T, typename F>
	inline auto MakeCallback(T* object, F member_function_ptr) noexcept
	{
		return Callback<FunctionSignatureT<F>>(object, member_function_ptr);
	}
}
