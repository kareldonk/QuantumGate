// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Concurrency
{
	class Event final
	{
	public:
		using HandleType = HANDLE;

		Event()
		{
			assert(m_Event == nullptr);

			m_Event = ::CreateEvent(nullptr, true, false, nullptr);
			if (m_Event == nullptr)
			{
				throw std::system_error(::GetLastError(), std::generic_category());
			}
		}

		Event(HandleType handle)
		{
			assert(m_Event == nullptr);

			if (handle == nullptr)
			{
				throw std::invalid_argument("The handle may not be null.");
			}
			else m_Event = handle;
		}

		Event(const Event&) = delete;
		
		Event(Event&& other) noexcept :
			m_Event(std::exchange(other.m_Event, nullptr))
		{}
		
		~Event()
		{
			Release();
		}

		Event& operator=(const Event&) = delete;

		Event& operator=(Event&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			Release();

			m_Event = std::exchange(other.m_Event, nullptr);

			return *this;
		}

		[[nodiscard]] inline HandleType GetHandle() const noexcept { return m_Event; }

		inline operator HandleType() const noexcept { return m_Event; }

		inline bool Set() noexcept
		{
			assert(m_Event != nullptr);

			return ::SetEvent(m_Event);
		}

		inline bool Reset() noexcept
		{
			assert(m_Event != nullptr);

			return ::ResetEvent(m_Event);
		}

		inline void Release() noexcept
		{
			if (m_Event != nullptr)
			{
				::CloseHandle(m_Event);
				m_Event = nullptr;
			}
		}

		[[nodiscard]] inline bool IsSet() const noexcept
		{
			assert(m_Event != nullptr);

			if (::WaitForSingleObject(m_Event, 0) == WAIT_OBJECT_0) return true;
			return false;
		}
		
		inline bool Wait(const std::chrono::milliseconds& ms) const noexcept
		{
			assert(m_Event != nullptr);

			if (::WaitForSingleObject(m_Event, static_cast<DWORD>(ms.count())) == WAIT_OBJECT_0) return true;
			return false;
		}

		inline void Wait() const noexcept
		{
			assert(m_Event != nullptr);

			::WaitForSingleObject(m_Event, INFINITE);
		}

	private:
		HandleType m_Event{ nullptr };
	};
}
