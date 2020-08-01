// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Concurrency
{
    class CriticalSection final
    {
    public:
        CriticalSection() noexcept
        {
            ::InitializeCriticalSection(&m_CriticalSection);
        }

        CriticalSection(const CriticalSection&) = delete;
        CriticalSection(CriticalSection&&) = delete;

        ~CriticalSection()
        {
            ::DeleteCriticalSection(&m_CriticalSection);
        }

        CriticalSection& operator=(const CriticalSection&) = delete;
        CriticalSection& operator=(CriticalSection&&) = delete;

        inline CRITICAL_SECTION& GetNative() noexcept { return m_CriticalSection; }

        inline void lock() noexcept
        {
            ::EnterCriticalSection(&m_CriticalSection);
        }

        [[nodiscard]] inline bool try_lock() noexcept
        {
            return ::TryEnterCriticalSection(&m_CriticalSection);
        }

        inline void unlock() noexcept
        {
            ::LeaveCriticalSection(&m_CriticalSection);
        }

    private:
        CRITICAL_SECTION m_CriticalSection;
    };
}