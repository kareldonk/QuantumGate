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
		OutOfMemory = 8,

		FailedTCPListenerManagerStartup = 10,
		FailedPeerManagerStartup = 11,
		FailedRelayManagerStartup = 12,
		FailedExtenderManagerStartup = 13,
		FailedKeyGenerationManagerStartup = 14,
		FailedUDPConnectionManagerStartup = 15,
		FailedUDPListenerManagerStartup = 16,

		NoPeersForRelay = 50,

		PeerNotFound = 100,
		PeerNotReady = 101,
		PeerNoExtender = 102,
		PeerAlreadyExists = 103,
		PeerSendBufferFull = 104,
		PeerSuspended = 105,

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

	[[nodiscard]] Export const std::error_category& GetResultCodeErrorCategory() noexcept;

	// The following function overload is needed for the enum conversion 
	// in one of the constructors for class std::error_code
	[[nodiscard]] Export std::error_code make_error_code(const ResultCode code) noexcept;
}

namespace std
{
	// Needed to make the std::error_code class work with the ResultCode enum
	template<>
	struct is_error_code_enum<QuantumGate::Implementation::ResultCode> : std::true_type {};
}

namespace QuantumGate::Implementation
{
	struct NoResultValue final {};

	template<typename E, E DefaultErrorCode, typename T = NoResultValue>
	class ResultImpl
	{
		static_assert(std::is_error_code_enum_v<E>, "E has to be an enum usable with std::error_code.");
		static_assert(static_cast<int>(DefaultErrorCode) != 0,
					  "Default error code shouldn't be zero. Zero indicates no error.");

		template<typename U>
		static constexpr bool HasValueType = !std::is_same_v<std::remove_cv_t<std::decay_t<U>>, NoResultValue>;

		template<typename U>
		using NoArgumentFunction = decltype(std::declval<U>()());

		using ValueStorageType = std::conditional_t<HasValueType<T>, std::optional<T>, T>;

	public:
		using ValueType = std::conditional_t<HasValueType<T>, T, void>;

		ResultImpl() noexcept {}

		ResultImpl(const E code) noexcept(!HasValueType<T>) : ResultImpl(std::error_code(code)) {}

		// This constructor accepts any enum that works with std::error_code
		template<typename Enum, typename = std::enable_if_t<std::is_error_code_enum_v<Enum>>>
		ResultImpl(const Enum code) noexcept(!HasValueType<T>) : ResultImpl(std::error_code(code)) {}

		ResultImpl(const std::error_code& code) noexcept(!HasValueType<T>) : m_ErrorCode(code)
		{
			if constexpr (HasValueType<T>)
			{
				if (!m_ErrorCode)
				{
					throw std::invalid_argument("Result should contain a value upon successful completion.");
				}
			}
		}

		template<typename U = T, typename = std::enable_if_t<HasValueType<U>>>
		ResultImpl(const T& value) noexcept(std::is_nothrow_copy_constructible_v<ValueStorageType>) :
			m_ErrorCode(static_cast<E>(0)), m_Value(value) {}

		template<typename U, typename = std::enable_if_t<std::is_same_v<U, T> && HasValueType<U>>>
		ResultImpl(U&& value) noexcept(std::is_nothrow_move_constructible_v<ValueStorageType>) :
			m_ErrorCode(static_cast<E>(0)), m_Value(std::forward<U>(value)) {}

		ResultImpl(const ResultImpl&) = delete;

		template<typename U = T>
		ResultImpl(std::enable_if_t<HasValueType<U>, ResultImpl&&> other) noexcept(std::is_nothrow_move_constructible_v<ValueStorageType>) :
			m_ErrorCode(std::move(other.m_ErrorCode)), m_Value(std::move(other.m_Value))
		{
			other.m_ErrorCode = DefaultErrorCode;
		}

		template<typename U = T>
		ResultImpl(std::enable_if_t<!HasValueType<U>, ResultImpl&&> other) noexcept :
			m_ErrorCode(std::move(other.m_ErrorCode))
		{
			other.m_ErrorCode = DefaultErrorCode;
		}

		~ResultImpl() = default;

		ResultImpl& operator=(const ResultImpl&) = delete;

		ResultImpl& operator=(ResultImpl&& other) noexcept(!HasValueType<T> ||
														   (HasValueType<T> && std::is_nothrow_move_assignable_v<ValueStorageType>))
		{
			m_ErrorCode = std::move(other.m_ErrorCode);

			if constexpr (HasValueType<T>)
			{
				m_Value = std::move(other.m_Value);
			}
			
			other.m_ErrorCode = DefaultErrorCode;

			return *this;
		}

		inline explicit operator bool() const noexcept
		{
			return Succeeded();
		}

		inline bool operator==(const std::error_code& code) const noexcept
		{
			return (m_ErrorCode == code);
		}

		inline bool operator!=(const std::error_code& code) const noexcept
		{
			return (m_ErrorCode != code);
		}

		[[nodiscard]] inline const std::error_code& GetErrorCode() const noexcept { return m_ErrorCode; }

		[[nodiscard]] inline int GetErrorValue() const noexcept { return m_ErrorCode.value(); }

		// This overload accepts any enum that works with std::error_code
		template<typename Enum, typename = std::enable_if_t<std::is_error_code_enum_v<Enum>>>
		[[nodiscard]] inline Enum GetErrorValue() const noexcept { return static_cast<Enum>(m_ErrorCode.value()); }

		[[nodiscard]] String GetErrorDescription() const noexcept
		{
			try
			{
				const auto msg = m_ErrorCode.message();
				return String(msg.begin(), msg.end());
			}
			catch (...) {}

			return {};
		}

		[[nodiscard]] String GetErrorCategory() const noexcept
		{
			try
			{
				const std::string name{ m_ErrorCode.category().name() };
				return String(name.begin(), name.end());
			}
			catch (...) {}

			return {};
		}

		[[nodiscard]] String GetErrorString() const noexcept
		{
			try
			{
				String str{ GetErrorCategory() };
				str += L" : ";
				str += std::to_wstring(GetErrorValue()).c_str();
				str += L" : ";
				str += GetErrorDescription();

				return str;
			}
			catch (...) {}

			return {};
		}

		template<typename U = T, typename = std::enable_if_t<HasValueType<U>>>
		inline T& GetValue() noexcept { assert(m_Value); return m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<HasValueType<U>>>
		inline const T& GetValue() const noexcept { assert(m_Value); return m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<HasValueType<U>>>
		[[nodiscard]] inline bool HasValue() const noexcept { return m_Value.has_value(); }

		template<typename U = T, typename = std::enable_if_t<HasValueType<U>>>
		inline T* operator->() noexcept { assert(m_Value); return &m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<HasValueType<U>>>
		inline const T* operator->() const noexcept { assert(m_Value); return &m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<HasValueType<U>>>
		inline T& operator*() noexcept { assert(m_Value); return m_Value.value(); }

		template<typename U = T, typename = std::enable_if_t<HasValueType<U>>>
		inline const T& operator*() const noexcept { assert(m_Value); return m_Value.value(); }

		void Clear() noexcept
		{
			m_ErrorCode = DefaultErrorCode;

			if constexpr (HasValueType<T>)
			{
				m_Value.reset();
			}
		}

		[[nodiscard]] inline bool Succeeded() const noexcept
		{
			if constexpr (!HasValueType<T>)
			{
				return (!m_ErrorCode);
			}
			else
			{
				return (!m_ErrorCode && HasValue());
			}
		}

		template<typename F>
		std::enable_if_t<!IsDetectedV<NoArgumentFunction, F>, void>
			Succeeded(F&& function) noexcept(noexcept(function(*this)))
		{
			if (Succeeded())
			{
				function(*this);
			}
		}

		template<typename F>
		std::enable_if_t<IsDetectedV<NoArgumentFunction, F>, void>
			Succeeded(F&& function) const noexcept(noexcept(function()))
		{
			if (Succeeded())
			{
				function();
			}
		}

		[[nodiscard]] inline bool Failed() const noexcept { return !Succeeded(); }

		template<typename F>
		std::enable_if_t<!IsDetectedV<NoArgumentFunction, F>, void>
			Failed(F&& function) noexcept(noexcept(function(*this)))
		{
			if (Failed())
			{
				function(*this);
			}
		}

		template<typename F>
		std::enable_if_t<IsDetectedV<NoArgumentFunction, F>, void>
			Failed(F&& function) const noexcept(noexcept(function()))
		{
			if (Failed())
			{
				function();
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
			stream << result.GetErrorCategory() << L" : " << result.GetErrorValue() <<
				L" : " << result.GetErrorDescription();
			return stream;
		}

	private:
		std::error_code m_ErrorCode{ DefaultErrorCode };
		[[no_unique_address]] ValueStorageType m_Value;
	};

	template<typename T = void>
	using Result = ResultImpl<ResultCode, ResultCode::Failed, std::conditional_t<std::is_same_v<T, void>, NoResultValue, T>>;

	template<typename T>
	[[nodiscard]] inline bool IsResultCode(const T& result) noexcept
	{
		return (result.GetErrorCode().category() == GetResultCodeErrorCategory());
	}

	template<typename T>
	[[nodiscard]] inline ResultCode GetResultCode(const T& result) noexcept
	{
		assert(IsResultCode(result));
		return result.GetErrorValue<ResultCode>();
	}
}