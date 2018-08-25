// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation
{
	template<typename T, typename T2, typename... Tn>
	struct are_same
	{
		static constexpr bool value = are_same<T, T2>::value && are_same<T, Tn...>::value;
	};

	template<typename T, typename T2>
	struct are_same<T, T2>
	{
		static constexpr bool value = std::is_same<T, std::decay_t<T2>>::value;
	};

	template<typename V, template<typename...> typename D, typename... T>
	struct detected_t : std::false_type {};

	template<template<typename...> typename D, typename... T>
	struct detected_t<std::void_t<D<T...>>, D, T...> : std::true_type {};

	template <template<typename...> typename D, typename... T>
	static constexpr bool is_detected = detected_t<void, D, T...>::value;

	template<typename T>
	struct function_signature_rm_noexcept;

	template<typename R, typename... Args>
	struct function_signature_rm_noexcept<R(Args...)>
	{
		using type = R(Args...);
	};

	template<typename R, typename... Args>
	struct function_signature_rm_noexcept<R(Args...) noexcept>
	{
		using type = R(Args...);
	};

	template<typename T, bool Const, bool NoExcept>
	struct function_signature_base;

	template<typename R, typename... Args, bool Const, bool NoExcept>
	struct function_signature_base<R(Args...), Const, NoExcept>
	{
		using return_type = R;
		static constexpr bool is_const = Const;
		static constexpr bool is_noexcept = NoExcept;

		// With help from https://functionalcpp.wordpress.com/2013/08/05/function-traits/
		static constexpr std::size_t arity = sizeof...(Args);
		static constexpr bool has_arguments = (arity > 0);

		template<std::size_t N>
		struct argument
		{
			static_assert(N < arity, "Invalid argument index");
			using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
		};
	};

	template<typename T>
	struct function_signature;

	template<typename R, typename... Args>
	struct function_signature<R(*)(Args...)> :
		public function_signature_base<R(Args...), false, false>
	{
		using type = R(Args...);
	};

	template<typename R, typename... Args>
	struct function_signature<R(*)(Args...) noexcept> :
		public function_signature_base<R(Args...), false, true>
	{
		using type = R(Args...) noexcept;
	};

	template<typename R, typename... Args>
	struct function_signature<R(Args...)> :
		public function_signature_base<R(Args...), false, false>
	{
		using type = R(Args...);
	};

	template<typename R, typename... Args>
	struct function_signature<R(Args...) noexcept> :
		public function_signature_base<R(Args...), false, true>
	{
		using type = R(Args...) noexcept;
	};

	template<typename R, typename T, typename... Args>
	struct function_signature<R(T::*)(Args...)> :
		public function_signature_base<R(Args...), false, false>
	{
		using type = R(Args...);
	};

	template<typename R, typename T, typename... Args>
	struct function_signature<R(T::*)(Args...) const> :
		public function_signature_base<R(Args...), true, false>
	{
		using type = R(Args...);
	};

	template<typename R, typename T, typename... Args>
	struct function_signature<R(T::*)(Args...) noexcept> :
		public function_signature_base<R(Args...), false, true>
	{
		using type = R(Args...) noexcept;
	};

	template<typename R, typename T, typename... Args>
	struct function_signature<R(T::*)(Args...) const noexcept> :
		public function_signature_base<R(Args...), true, true>
	{
		using type = R(Args...) noexcept;
	};

	template<typename T>
	using function_signature_t = typename function_signature<T>::type;

	template<typename T>
	using function_signature_rt = typename function_signature<T>::return_type;

	template<typename T>
	static constexpr bool function_signature_is_noexcept = function_signature<T>::is_noexcept;

	template<typename T>
	using function_signature_rm_noexcept_t = typename function_signature_rm_noexcept<T>::type;
}