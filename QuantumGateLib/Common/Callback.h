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

	template<typename T, bool NoExcept = false>
	class CallbackImpl;

	template<typename R, typename... Args, bool NoExcept>
	class CallbackImpl<R(Args...), NoExcept> : public CallbackImplBase
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
			FreeCallbackFunction(F&& function) noexcept : m_Function(std::forward<F>(function)) {}

		private:
			ForceInline R operator()(Args... args) const noexcept(NoExcept)override
			{
				if constexpr (std::is_pointer_v<F>)
				{
					assert(m_Function != nullptr);
				}

				return (m_Function)(std::forward<Args>(args)...);
			}

		private:
			F m_Function{ nullptr };
		};

		template<typename T, typename F>
		class MemberCallbackFunction : public CallbackFunction
		{
		public:
			MemberCallbackFunction(T* obj, F&& function) noexcept :
				m_Object(obj), m_Function(std::forward<F>(function)) {}

		private:
			ForceInline R operator()(Args... args) const noexcept(NoExcept)override
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
			if constexpr (sizeof(FreeCallbackFunction<F>) > sizeof(m_FunctionStorage))
			{
				m_Function = new FreeCallbackFunction<F>(std::forward<F>(function));
				m_Heap = true;
			}
			else m_Function = new (&m_FunctionStorage) FreeCallbackFunction<F>(std::forward<F>(function));
		}

		template<typename T, typename F>
		CallbackImpl(T* obj, F&& function) noexcept
		{
			static_assert(std::is_member_function_pointer_v<R(T::*)(Args...)>,
						  "Object does not have callable member function with expected signature.");

			static_assert(sizeof(MemberCallbackFunction<T, F>) <= sizeof(m_FunctionStorage),
						  "Type is too large for FunctionStorage variable; increase size.");

			m_Function = new (&m_FunctionStorage) MemberCallbackFunction<T, F>(obj, std::forward<F>(function));
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
	class Callback : public CallbackImpl<function_signature_rm_noexcept_t<Sig>, function_signature_is_noexcept<Sig>>
	{
	public:
		Callback() noexcept {}
		Callback(std::nullptr_t) noexcept {}

		template<typename F, typename = std::enable_if_t<!std::is_base_of_v<CallbackImplBase, F>>>
		Callback(F&& function) noexcept(noexcept(CallbackImpl<function_signature_rm_noexcept_t<Sig>,
												 function_signature_is_noexcept<Sig>>(std::forward<F>(function)))) :
			CallbackImpl<function_signature_rm_noexcept_t<Sig>,
			function_signature_is_noexcept<Sig>>(std::forward<F>(function))
		{
			static_assert(!std::is_base_of_v<CallbackImplBase, F>,
						  "Attempt to pass in Callback object which is not allowed.");

			if constexpr (std::is_pointer_v<F>)
			{
				// For function pointers
				static_assert(std::is_same_v<Sig, function_signature_t<std::decay_t<F>>>,
							  "Function parameter does not have the expected signature.");
			}
			else
			{
				// For lambdas and other objects overloading operator()
				using objtype = typename std::decay_t<F>;
				using funcsig = function_signature_t<decltype(&objtype::operator())>;
				static_assert(std::is_same_v<Sig, funcsig>,
							  "Function parameter does not have the expected signature.");
			}
		}

		template<typename T, typename F>
		Callback(T* object, F&& member_function) noexcept :
			CallbackImpl<function_signature_rm_noexcept_t<Sig>,
			function_signature_is_noexcept<Sig>>(object, std::forward<F>(member_function))
		{
			static_assert(std::is_same_v<Sig, function_signature_t<std::decay_t<F>>>,
						  "Function parameter does not have the expected signature.");
		}

		~Callback() = default;
		Callback(const Callback&) = delete;
		Callback(Callback&& other) noexcept = default;
		Callback& operator=(const Callback&) = delete;
		Callback& operator=(Callback&& other) noexcept = default;
	};

	template<typename T, typename F>
	inline auto MakeCallback(T* object, F&& member_function) noexcept
	{
		return Callback<function_signature_t<F>>(object, std::forward<F>(member_function));
	}

	template<typename F>
	inline auto MakeCallback(F&& function) noexcept(std::is_pointer_v<F>)
	{
		if constexpr (std::is_pointer_v<F>)
		{
			// For function pointers
			return Callback<function_signature_t<F>>(std::forward<F>(function));
		}
		else
		{
			// For lambdas and other objects overloading operator()
			using objtype = typename std::decay_t<F>;
			using funcsig = function_signature_t<decltype(&objtype::operator())>;
			return Callback<funcsig>(std::forward<F>(function));
		}
	}
}
