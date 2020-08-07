// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CriticalSection.h"

#include <chrono>

namespace QuantumGate::Implementation::Concurrency
{
    class ConditionVariable final
    {
    public:
        ConditionVariable() noexcept
        {
            ::InitializeConditionVariable(&m_ConditionVariable);
        }

        ConditionVariable(const ConditionVariable&) = delete;
        ConditionVariable(ConditionVariable&&) = delete;
        ~ConditionVariable() = default;
        ConditionVariable& operator=(const ConditionVariable&) = delete;
        ConditionVariable& operator=(ConditionVariable&&) = delete;

        template<typename Pred>
        bool Wait(CriticalSection& critical_section, const std::chrono::milliseconds time, Pred&& pred) noexcept
        {
            while (!pred())
            {
                if (!::SleepConditionVariableCS(&m_ConditionVariable, &critical_section.GetNative(),
                                                static_cast<DWORD>(time.count())))
                {
                    return false;
                }
            }

            return true;
        }

        template<typename Pred>
        bool Wait(CriticalSection& critical_section, Pred&& pred) noexcept
        {
            while (!pred())
            {
                if (!::SleepConditionVariableCS(&m_ConditionVariable, &critical_section.GetNative(), INFINITE))
                {
                    return false;
                }
            }

            return true;
        }

        void NotifyOne() noexcept
        {
            ::WakeConditionVariable(&m_ConditionVariable);
        }

        void NotifyAll() noexcept
        {
            ::WakeAllConditionVariable(&m_ConditionVariable);
        }

    private:
        CONDITION_VARIABLE m_ConditionVariable{ nullptr };
    };
}