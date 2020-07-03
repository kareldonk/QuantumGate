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
			m_Event = CreateEvent(NULL, true, false, NULL);
			if (m_Event == nullptr)
			{
				throw std::system_error(::GetLastError(), std::generic_category());
			}
		}

		Event(const Event&) = delete;
		Event(Event&&) = delete;
		
		~Event()
		{
			if (m_Event != nullptr)
			{
				::CloseHandle(m_Event);
			}
		}

		Event& operator=(const Event&) = delete;
		Event& operator=(Event&&) = delete;

		[[nodiscard]] inline HandleType GetHandle() const noexcept { return m_Event; }

		inline bool Set() noexcept
		{
			return ::SetEvent(m_Event);
		}

		inline bool Reset() noexcept
		{
			return ::ResetEvent(m_Event);
		}

		[[nodiscard]] inline bool IsSet() const noexcept
		{
			if (::WaitForSingleObject(m_Event, 0) == WAIT_OBJECT_0) return true;
			return false;
		}
		
		inline bool Wait(const std::chrono::milliseconds& ms) const noexcept
		{
			if (::WaitForSingleObject(m_Event, static_cast<DWORD>(ms.count())) == WAIT_OBJECT_0) return true;
			return false;
		}

		inline void Wait() const noexcept
		{
			::WaitForSingleObject(m_Event, INFINITE);
		}

	private:
		HandleType m_Event{ nullptr };
	};
}
