// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common\Traits.h"

#include <iostream>  
#include <system_error>

namespace QuantumGate::Implementation
{
	// For background info on error_code use see CppCon2017 presentation by Charles Bay: 
	// https://www.youtube.com/watch?v=hNaLf8lYLDo 

	enum class ResultCode
	{
		Succeeded = 0,

		Failed = 1,
		FailedRetry = 2,
		NotRunning = 3,
		InvalidArgument = 4,
		NotAllowed = 5,
		TimedOut = 6,
		Aborted = 7,

		FailedListenerManagerStartup = 10,
		FailedPeerManagerStartup = 11,
		FailedRelayManagerStartup = 12,
		FailedExtenderManagerStartup = 13,
		FailedKeyGenerationManagerStartup = 14,

		NoPeersForRelay = 50,

		PeerNotFound = 100,
		PeerNotReady = 101,
		PeerNoExtender = 102,
		PeerAlreadyExists = 103,

		AddressInvalid = 200,
		AddressMaskInvalid = 201,
		AddressNotFound = 202,

		ExtenderNotFound = 300,
		ExtenderAlreadyPresent = 301,
		ExtenderObjectDifferent = 302,
		ExtenderAlreadyRemoved = 303,
		ExtenderTooMany = 304,
		ExtenderHasNoLocalInstance = 305,

		ExtenderModuleAlreadyPresent = 400,
		ExtenderModuleLoadFailure = 401,
		ExtenderModuleNotFound = 402
	};

	Export std::ostream& operator<<(std::ostream& os, const ResultCode code);
	Export std::wostream& operator<<(std::wostream& os, const ResultCode code);

	Export const std::error_category& GetErrorCategory() noexcept;

	// The following function overload is needed for the enum conversion 
	// in one of the constructors for class error_code
	Export std::error_code make_error_code(const ResultCode code) noexcept;

	struct NoResultValue final {};

	template<typename E, E DefaultErrorCode, typename T = NoResultValue>
	class ResultImpl
	{
		static_assert(std::is_error_code_enum<E>::value, "E has to be an enum usable with std::error_code.");
		static_assert(static_cast<int>(DefaultErrorCode) != 0,
					  "Default error code shouldn't be zero. Zero indicates no error.");

		template<typename U>
		static constexpr bool has_value_type = !std::is_same_v<std::remove_cv_t<std::decay_t<U>>, NoResultValue>;

		template<typename U>
		using no_argument_func_t = decltype(std::declval<U>()());

	public:
		using ValueType = std::conditional_t<has_value_type<T>, T, void>;

		constexpr ResultImpl() noexcept {}

		ResultImpl(const E code) noexcept(!has_value_type<T>) : ResultImpl(std::error_code(code)) {}

		ResultImpl(const std::error_code& code) noexcept(!has_value_type<T>) : m_ErrorCode(code)
		{
			if constexpr (has_value_type<T>)
			{
				if (!m_ErrorCode)
				{
					Clear();
					throw std::invalid_argument("Result should contain a value upon successful completion.");
				}
			}
		}

		template<typename U = T, typename = std::enable_if_t<has_value_type<U>>>
		ResultImpl(const T& value) noexcept(std::is_nothrow_copy_constructible_v<std::optional<T>>) :
			m_ErrorCode(static_cast<E>(0)), m_Value(value) {}

		template<typename U = T, typename = std::enable_if_t<has_value_type<U>>>
		ResultImpl(T&& value) noexcept(std::is_nothrow_move_constructible_v<std::optional<T>>) :
			m_ErrorCode(static_cast<E>(0)), m_Value(std::forward<T>(value)) {}

		ResultImpl(const ResultImpl&) = delete;
		ResultImpl(ResultImpl&&) = default;
		~ResultImpl() = default;
		ResultImpl& operator=(const ResultImpl&) = delete;
		ResultImpl& operator=(ResultImpl&&) = default;

		constexpr explicit operator bool() const noexcept
		{
			return Succeeded();
		}

		constexpr const bool operator==(const std::error_code& code) const noexcept
		{
			return (m_ErrorCode == code);
		}

		constexpr const bool operator!=(const std::error_code& code) const noexcept
		{
			return (m_ErrorCode != code);
		}

		[[nodiscard]] const std::error_code& GetErrorCode() const noexcept { return m_ErrorCode; }

		[[nodiscard]] constexpr int GetErrorValue() const noexcept { return m_ErrorCode.value(); }

		[[nodiscard]] String GetErrorDescription() const noexcept
		{
			const auto msg = m_ErrorCode.message();
			return String(msg.begin(), msg.end());
		}

		[[nodiscard]] String GetErrorCategory() const noexcept
		{
			const std::string name = m_ErrorCode.category().name();
			return String(name.begin(), name.end());
		}

		[[nodiscard]] String GetErrorString() const noexcept
		{
			return GetErrorCategory() + L" : " + std::to_wstring(GetErrorValue()) +
				L" : " + GetErrorDescription();
		}

		template<typename U = T, typename = std::enable_if_t<has_value_type<U>>>
		T& GetValue() noexcept { assert(m_Value); return m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<has_value_type<U>>>
		const T& GetValue() const noexcept { assert(m_Value); return m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<has_value_type<U>>>
		[[nodiscard]] constexpr const bool HasValue() const noexcept { return m_Value.has_value(); }

		template<typename U = T, typename = std::enable_if_t<has_value_type<U>>>
		constexpr T* operator->() noexcept { assert(m_Value); return &m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<has_value_type<U>>>
		constexpr const T* operator->() const noexcept { assert(m_Value); return &m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<has_value_type<U>>>
		constexpr T& operator*() noexcept { assert(m_Value); return m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<has_value_type<U>>>
		constexpr const T& operator*() const noexcept { assert(m_Value); return m_Value.value(); }

		void Clear() noexcept
		{
			m_ErrorCode = DefaultErrorCode;
			m_Value.reset();
		}

		[[nodiscard]] constexpr const bool Succeeded() const noexcept
		{
			if constexpr (!has_value_type<T>)
			{
				return (!m_ErrorCode);
			}
			else
			{
				return (!m_ErrorCode && HasValue());
			}
		}

		template<typename F>
		void Succeeded(F&& function) const noexcept
		{
			if (Succeeded())
			{
				if constexpr (is_detected<no_argument_func_t, F>)
				{
					function();
				}
				else function(*this);
			}
		}

		[[nodiscard]] constexpr const bool Failed() const noexcept { return !Succeeded(); }

		template<typename F>
		void Failed(F&& function) const noexcept
		{
			if (Failed())
			{
				if constexpr (is_detected<no_argument_func_t, F>)
				{
					function();
				}
				else function(*this);
			}
		}

		friend std::ostream& operator<<(std::ostream& stream, const ResultImpl& result)
		{
			stream << result.m_ErrorCode.category().name() << " : " << result.GetErrorValue() <<
				" : " << result.m_ErrorCode.message();
			return stream;
		}

		friend std::wostream& operator<<(std::wostream& stream, const ResultImpl& result)
		{
			stream << result.GetErrorCategory() << L" : " << result.GetErrorValue() << L" : " << result.GetErrorDescription();
			return stream;
		}

	private:
		std::error_code m_ErrorCode{ DefaultErrorCode };
		std::optional<T> m_Value;
	};

	template<typename E, E DefaultErrorCode, typename T = void>
	class [[nodiscard]] ResultBase final : public ResultImpl<E, DefaultErrorCode, T>
	{
	public:
		using ResultImpl<E, DefaultErrorCode, T>::ResultImpl;
	};

	template<typename E, E DefaultErrorCode>
	class [[nodiscard]] ResultBase<E, DefaultErrorCode, void> final : public ResultImpl<E, DefaultErrorCode, NoResultValue>
	{
	public:
		using ResultImpl<E, DefaultErrorCode, NoResultValue>::ResultImpl;
	};

	template<typename T = void>
	using Result = ResultBase<ResultCode, ResultCode::Failed, T>;
}

namespace std
{
	// Needed to make the std::error_code class work with the ResultCode enum
	template<>
	struct is_error_code_enum<QuantumGate::Implementation::ResultCode> : std::true_type {};
}