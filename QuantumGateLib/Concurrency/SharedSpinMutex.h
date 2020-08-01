// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <atomic>

namespace QuantumGate::Implementation::Concurrency
{
	class SharedSpinMutex final
	{
		using StateType = unsigned long;
		using SizeType = unsigned long;

	public:
		SharedSpinMutex() noexcept {}
		SharedSpinMutex(const SharedSpinMutex&) = delete;
		SharedSpinMutex(SharedSpinMutex&&) = delete;
		~SharedSpinMutex() = default;
		SharedSpinMutex& operator=(const SharedSpinMutex&) = delete;
		SharedSpinMutex& operator=(SharedSpinMutex&&) = delete;

		void lock() noexcept
		{
			// Set exclusive lock flag
			DoUntilSucceeded([&]() -> bool
			{
				auto state = m_State.load(std::memory_order_relaxed);
				if (!IsExclusiveLocked(state))
				{
					const auto new_state = SetExclusiveLocked(state);
					return m_State.compare_exchange_weak(state, new_state, std::memory_order_release,
														 std::memory_order_relaxed);
				}

				return false;
			});

			// Wait for shared locks to get to zero
			DoUntilSucceeded([&]() -> bool
			{
				if (GetSharedLocks(m_State.load(std::memory_order_relaxed)) == 0 &&
					GetSharedLocks(m_State.load(std::memory_order_acquire)) == 0)
				{
					return true;
				}

				return false;
			});
		}

		[[nodiscard]] bool try_lock() noexcept
		{
			auto state = m_State.load(std::memory_order_relaxed);
			if (HasNoLocks(state))
			{
				const auto new_state = SetExclusiveLocked(state);
				return m_State.compare_exchange_strong(state, new_state, std::memory_order_release, std::memory_order_relaxed);
			}

			return false;
		}

		void unlock() noexcept
		{
			auto state = m_State.load(std::memory_order_relaxed);
			while (true)
			{
				assert(IsExclusiveLocked(state));

				const auto new_state = UnsetExclusiveLocked(state);
				if (m_State.compare_exchange_weak(state, new_state, std::memory_order_release, std::memory_order_relaxed))
				{
					return;
				}
			}
		}

		void lock_shared() noexcept
		{
			DoUntilSucceeded([&]() -> bool { return try_lock_shared(); });
		};

		[[nodiscard]] bool try_lock_shared() noexcept
		{
			auto state = m_State.load(std::memory_order_relaxed);
			if (!IsExclusiveLocked(state))
			{
				auto scount = GetSharedLocks(state);

				assert(scount <= MaxNumSharedLocks);

				if (scount < MaxNumSharedLocks)
				{
					++scount;
					const auto new_state = MakeState(IsExclusiveLocked(state), scount);
					return m_State.compare_exchange_strong(state, new_state, std::memory_order_release,
														   std::memory_order_relaxed);
				}
			}

			return false;
		}

		void unlock_shared() noexcept
		{
			auto state = m_State.load(std::memory_order_relaxed);
			while (true)
			{
				auto scount = GetSharedLocks(state);

				assert(scount > 0);

				--scount;
				const auto new_state = MakeState(IsExclusiveLocked(state), scount);
				if (m_State.compare_exchange_weak(state, new_state, std::memory_order_release, std::memory_order_relaxed))
				{
					return;
				}
			}
		}

	private:
		template<typename Func> requires std::is_same_v<std::invoke_result_t<Func>, bool>
		void DoUntilSucceeded(Func&& func) noexcept
		{
			for (int spin_count = 0; !func(); ++spin_count)
			{
				if (spin_count < 16)
				{
					_mm_pause();
				}
				else
				{
					std::this_thread::yield();
					spin_count = 0;
				}
			}
		}

		[[nodiscard]] constexpr StateType MakeState(bool excl, SizeType shared_count) noexcept
		{
			StateType state = shared_count << 1;
			if (excl) state |= ExclusiveLockFlag;
			return state;
		}

		[[nodiscard]] constexpr bool HasNoLocks(StateType state) noexcept
		{
			return (state == 0);
		}

		[[nodiscard]] constexpr bool IsExclusiveLocked(StateType state) noexcept
		{
			return (state & ExclusiveLockFlag);
		}

		constexpr StateType SetExclusiveLocked(StateType state) noexcept
		{
			return (state | ExclusiveLockFlag);
		}

		constexpr StateType UnsetExclusiveLocked(StateType state) noexcept
		{
			state &= ~ExclusiveLockFlag;
			return state;
		}

		[[nodiscard]] constexpr bool HasSharedLocks(StateType state) noexcept
		{
			return (GetSharedLocks(state) > 0);
		}

		[[nodiscard]] constexpr SizeType GetSharedLocks(StateType state) noexcept
		{
			return (state >> 1);
		}

	private:
		static constexpr StateType ExclusiveLockFlag = 1ull;
		static constexpr SizeType MaxNumSharedLocks = ~ExclusiveLockFlag;

	private:
		std::atomic<StateType> m_State{ 0 };
	};
}