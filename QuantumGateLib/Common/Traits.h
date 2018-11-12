// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation
{
	// AreSame
	namespace
	{
		template<typename T, typename... Ts>
		struct AreSame
		{
			static constexpr bool Value = std::conjunction_v<std::is_same<T, Ts>...>;
		};

		template<typename T, typename... Ts>
		static constexpr bool AreSameV = AreSame<T, Ts...>::Value;
	}

	// IsDetected
	namespace
	{
		template<typename V, template<typename...> typename D, typename... T>
		struct IsDetected : std::false_type {};

		template<template<typename...> typename D, typename... T>
		struct IsDetected<std::void_t<D<T...>>, D, T...> : std::true_type {};

		template <template<typename...> typename D, typename... T>
		static constexpr bool IsDetectedV = IsDetected<void, D, T...>::value;
	}

	// FunctionSignatureRemoveConstNoexcept
	namespace
	{
		template<typename T>
		struct FunctionSignatureRemoveConstNoexcept;

		template<typename R, typename... Args>
		struct FunctionSignatureRemoveConstNoexcept<R(Args...)>
		{
			using Type = R(Args...);
		};

		template<typename R, typename... Args>
		struct FunctionSignatureRemoveConstNoexcept<R(Args...) noexcept>
		{
			using Type = R(Args...);
		};

		template<typename R, typename... Args>
		struct FunctionSignatureRemoveConstNoexcept<R(Args...) const>
		{
			using Type = R(Args...);
		};

		template<typename R, typename... Args>
		struct FunctionSignatureRemoveConstNoexcept<R(Args...) const noexcept>
		{
			using Type = R(Args...);
		};

		template<typename T>
		using FunctionSignatureRemoveConstNoexceptT = typename FunctionSignatureRemoveConstNoexcept<T>::Type;
	}

	// FunctionSignatureAddConst
	namespace
	{
		template<typename T>
		struct FunctionSignatureAddConst;

		template<typename R, typename... Args>
		struct FunctionSignatureAddConst<R(Args...)>
		{
			using Type = R(Args...) const;
		};

		template<typename R, typename... Args>
		struct FunctionSignatureAddConst<R(Args...) const>
		{
			using Type = R(Args...) const;
		};

		template<typename R, typename... Args>
		struct FunctionSignatureAddConst<R(Args...) noexcept>
		{
			using Type = R(Args...) const noexcept;
		};

		template<typename R, typename... Args>
		struct FunctionSignatureAddConst<R(Args...) const noexcept>
		{
			using Type = R(Args...) const noexcept;
		};

		template<typename T>
		using FunctionSignatureAddConstT = typename FunctionSignatureAddConst<T>::Type;
	}

	// FunctionSignatureAddNoexcept
	namespace
	{
		template<typename T>
		struct FunctionSignatureAddNoexcept;

		template<typename R, typename... Args>
		struct FunctionSignatureAddNoexcept<R(Args...)>
		{
			using Type = R(Args...) noexcept;
		};

		template<typename R, typename... Args>
		struct FunctionSignatureAddNoexcept<R(Args...) const>
		{
			using Type = R(Args...) const noexcept;
		};

		template<typename R, typename... Args>
		struct FunctionSignatureAddNoexcept<R(Args...) noexcept>
		{
			using Type = R(Args...) noexcept;
		};

		template<typename R, typename... Args>
		struct FunctionSignatureAddNoexcept<R(Args...) const noexcept>
		{
			using Type = R(Args...) const noexcept;
		};

		template<typename T>
		using FunctionSignatureAddNoexceptT = typename FunctionSignatureAddNoexcept<T>::Type;
	}

	// MakeMemberFunctionPointer
	namespace
	{
		template<typename T, typename F>
		struct MakeMemberFunctionPointer;

		template<typename T, typename R, typename... Args>
		struct MakeMemberFunctionPointer<T, R(Args...)>
		{
			using Type = R(T::*)(Args...);
		};

		template<typename T, typename R, typename... Args>
		struct MakeMemberFunctionPointer<T, R(Args...) const>
		{
			using Type = R(T::*)(Args...) const;
		};

		template<typename T, typename R, typename... Args>
		struct MakeMemberFunctionPointer<T, R(Args...) noexcept>
		{
			using Type = R(T::*)(Args...) noexcept;
		};

		template<typename T, typename R, typename... Args>
		struct MakeMemberFunctionPointer<T, R(Args...) const noexcept>
		{
			using Type = R(T::*)(Args...) const noexcept;
		};

		template<typename T, typename F>
		using MakeMemberFunctionPointerT = typename MakeMemberFunctionPointer<T, F>::Type;
	}

	// FunctionSignature
	namespace
	{
		template<typename T, bool Const, bool NoExcept>
		struct FunctionSignatureBase;

		template<typename R, typename... Args, bool Const, bool NoExcept>
		struct FunctionSignatureBase<R(Args...), Const, NoExcept>
		{
			using ReturnType = R;
			static constexpr bool IsConst = Const;
			static constexpr bool IsNoexcept = NoExcept;

			// With help from https://functionalcpp.wordpress.com/2013/08/05/function-traits/
			static constexpr std::size_t Arity = sizeof...(Args);
			static constexpr bool HasArguments = (Arity > 0);

			template<std::size_t N>
			struct Argument
			{
				static_assert(N < Arity, "Invalid argument index");
				using Type = typename std::tuple_element<N, std::tuple<Args...>>::type;
			};
		};

		template<typename T>
		struct FunctionSignature;

		template<typename R, typename... Args>
		struct FunctionSignature<R(*)(Args...)> :
			public FunctionSignatureBase<R(Args...), false, false>
		{
			using Type = R(Args...);
		};

		template<typename R, typename... Args>
		struct FunctionSignature<R(*)(Args...) noexcept> :
			public FunctionSignatureBase<R(Args...), false, true>
		{
			using Type = R(Args...) noexcept;
		};

		template<typename R, typename... Args>
		struct FunctionSignature<R(Args...)> :
			public FunctionSignatureBase<R(Args...), false, false>
		{
			using Type = R(Args...);
		};

		template<typename R, typename... Args>
		struct FunctionSignature<R(Args...) noexcept> :
			public FunctionSignatureBase<R(Args...), false, true>
		{
			using Type = R(Args...) noexcept;
		};

		template<typename R, typename... Args>
		struct FunctionSignature<R(Args...) const> :
			public FunctionSignatureBase<R(Args...), true, false>
		{
			using Type = R(Args...) const;
		};

		template<typename R, typename... Args>
		struct FunctionSignature<R(Args...) const noexcept> :
			public FunctionSignatureBase<R(Args...), true, true>
		{
			using Type = R(Args...) const noexcept;
		};

		template<typename R, typename T, typename... Args>
		struct FunctionSignature<R(T::*)(Args...)> :
			public FunctionSignatureBase<R(Args...), false, false>
		{
			using Type = R(Args...);
		};

		template<typename R, typename T, typename... Args>
		struct FunctionSignature<R(T::*)(Args...) const> :
			public FunctionSignatureBase<R(Args...), true, false>
		{
			using Type = R(Args...) const;
		};

		template<typename R, typename T, typename... Args>
		struct FunctionSignature<R(T::*)(Args...) noexcept> :
			public FunctionSignatureBase<R(Args...), false, true>
		{
			using Type = R(Args...) noexcept;
		};

		template<typename R, typename T, typename... Args>
		struct FunctionSignature<R(T::*)(Args...) const noexcept> :
			public FunctionSignatureBase<R(Args...), true, true>
		{
			using Type = R(Args...) const noexcept;
		};

		template<typename T>
		using FunctionSignatureT = typename FunctionSignature<T>::Type;

		template<typename T>
		using FunctionSignatureRT = typename FunctionSignature<T>::ReturnType;

		template<typename T>
		static constexpr bool FunctionSignatureIsNoexceptV = FunctionSignature<T>::IsNoexcept;

		template<typename T>
		static constexpr bool FunctionSignatureIsConstV = FunctionSignature<T>::IsConst;
	}

	// HasFunctionCallOperator
	namespace
	{
		template<typename F, F Func>
		struct FunctionCallOperatorTest {};

		template<typename V, typename O, typename F>
		struct HasFunctionCallOperator : std::false_type {};

		template<typename O, typename F>
		struct HasFunctionCallOperator<std::void_t<FunctionCallOperatorTest<F, &O::operator()>>, O, F> : std::true_type {};

		template<typename O, typename F>
		static constexpr bool HasFunctionCallOperatorV = HasFunctionCallOperator<void, O, F>::value;

		template<typename Sig, typename O>
		static constexpr bool CheckFunctionCallOperatorSignature()
		{
			using objtype = typename std::decay_t<O>;
			using member_funcsig = MakeMemberFunctionPointerT<objtype, Sig>;

			return HasFunctionCallOperatorV<objtype, member_funcsig>;
		}
	}
}