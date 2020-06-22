// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "EventCondition.h"
#include "..\Common\Console.h"
#include "..\Common\Callback.h"
#include "..\Common\Containers.h"
#include "..\Common\Util.h"

#include <thread>

namespace QuantumGate::Implementation::Concurrency
{
	struct NoThreadPoolData final {};
	struct NoThreadData final {};

	template<typename ThPData = NoThreadPoolData, typename ThData = NoThreadData>
	class ThreadPool final
	{
	public:
		struct ThreadCallbackResult
		{
			bool Success{ false };
			bool DidWork{ false };
		};

	private:
		template<typename U>
		static constexpr bool has_threadpool_data = !std::is_same_v<std::remove_cv_t<std::decay_t<U>>, NoThreadPoolData>;

		template<typename U>
		static constexpr bool has_thread_data = !std::is_same_v<std::remove_cv_t<std::decay_t<U>>, NoThreadData>;

		template<bool thpdata = has_threadpool_data<ThPData>, bool thdata = has_thread_data<ThData>>
		struct thread_callback
		{
			using type = Callback<ThreadCallbackResult(const EventCondition&)>;
		};

		template<>
		struct thread_callback<true, false>
		{
			using type = Callback<ThreadCallbackResult(ThPData&, const EventCondition&)>;
		};

		template<>
		struct thread_callback<false, true>
		{
			using type = Callback<ThreadCallbackResult(ThData&, const EventCondition&)>;
		};

		template<>
		struct thread_callback<true, true>
		{
			using type = Callback<ThreadCallbackResult(ThPData&, ThData&, const EventCondition&)>;
		};

	public:
		using ThreadCallbackType = typename thread_callback<>::type;

	private:
		struct ThreadCtrl final
		{
			ThreadCtrl(const String& thname, ThreadCallbackType&& thcallback,
					   ThData&& thdata, EventCondition* thevent = nullptr) :
				ThreadName(thname),
				ThreadCallback(std::move(thcallback)),
				ThreadData(std::move(thdata)),
				ThreadEvent(thevent),
				ShutdownEvent(std::make_unique<EventCondition>())
			{}

			String ThreadName;
			ThreadCallbackType ThreadCallback;
			ThData ThreadData;
			EventCondition* ThreadEvent{ nullptr };
			std::unique_ptr<EventCondition> ShutdownEvent{ nullptr };
			std::thread Thread;
		};

		using ThreadList = Containers::List<ThreadCtrl>;
		using ThreadIterator = typename ThreadList::iterator;

	public:
		class Thread final
		{
			friend class ThreadPool;

		public:
			Thread() = delete;
			Thread(const ThreadIterator it) noexcept : m_ThreadIterator(it) {}

			const std::thread::id GetID() const noexcept { return m_ThreadIterator->Thread.get_id(); }
			const String& GetName() const noexcept { return m_ThreadIterator->ThreadName; }
			[[nodiscard]] bool IsRunning() const noexcept { return m_ThreadIterator->Thread.joinable(); }

			template<typename U = ThData, typename = std::enable_if_t<has_thread_data<U>>>
			ThData& GetData() noexcept { return m_ThreadIterator->ThreadData; }

		private:
			ThreadIterator m_ThreadIterator;
		};

		template<typename... Args>
		ThreadPool(Args&&... args) noexcept : m_Data(std::forward<Args>(args)...) {}

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool(ThreadPool&&) noexcept = default;
		~ThreadPool() = default;
		ThreadPool& operator=(const ThreadPool&) = delete;
		ThreadPool& operator=(ThreadPool&&) noexcept = default;

		inline void SetWorkerThreadsMaxBurst(Size max_burst) noexcept
		{
			m_WorkerThreadsMaxBurst = max_burst;
		}

		inline void SetWorkerThreadsMaxSleep(std::chrono::milliseconds max_sleep) noexcept
		{
			m_WorkerThreadsMaxSleep = max_sleep;
		}

		inline bool IsRunning() const noexcept
		{
			// If at least one thread is active
			// then threadpool is running
			for (const auto& threadctrl : m_Threads)
			{
				if (threadctrl.Thread.joinable()) return true;
			}

			return false;
		}

		template<typename U = ThData, typename = std::enable_if_t<!has_thread_data<U>>>
		[[nodiscard]] inline bool AddThread(const String& thname, ThreadCallbackType&& thcallback,
											EventCondition* thevent = nullptr, const bool event_reset = true) noexcept
		{
			return AddThreadImpl(thname, std::move(thcallback), NoThreadData{}, thevent, event_reset);
		}

		template<typename U = ThData, typename = std::enable_if_t<has_thread_data<U>>>
		[[nodiscard]] inline bool AddThread(const String& thname, ThreadCallbackType&& thcallback,
											ThData&& thdata, EventCondition* thevent = nullptr,
											const bool event_reset = true) noexcept
		{
			return AddThreadImpl(thname, std::move(thcallback), std::move(thdata), thevent, event_reset);
		}

		[[nodiscard]] inline std::pair<bool, std::optional<Thread>> RemoveThread(Thread&& thread) noexcept
		{
			if (thread.IsRunning())
			{
				StopThread(*(thread.m_ThreadIterator));
			}

			auto next_it = m_Threads.erase(thread.m_ThreadIterator);
			if (next_it != m_Threads.end()) return std::make_pair(true, Thread(next_it));

			return std::make_pair(true, std::nullopt);
		}

		inline Size GetSize() const noexcept { return m_Threads.size(); }

		inline std::optional<Thread> GetFirstThread() noexcept
		{
			if (m_Threads.size() > 0)
			{
				return Thread(m_Threads.begin());
			}

			return std::nullopt;
		}

		inline std::optional<Thread> GetNextThread(const Thread& thread) noexcept
		{
			auto it = std::next(thread.m_ThreadIterator, 1);
			if (it != m_Threads.end()) return Thread(it);

			return std::nullopt;
		}

		inline void Clear() noexcept
		{
			assert(!IsRunning());

			m_Threads.clear();
		}

		[[nodiscard]] bool Startup() noexcept
		{
			assert(!IsRunning());

			// Start all threads
			for (auto& threadctrl : m_Threads)
			{
				StartThread(threadctrl);
			}

			return IsRunning();
		}

		void Shutdown() noexcept
		{
			assert(IsRunning());

			// Set the shutdown event to notify threads
			// that we're shutting down
			for (auto& threadctrl : m_Threads)
			{
				threadctrl.ShutdownEvent->Set();
			}

			// Stop all threads
			for (auto& threadctrl : m_Threads)
			{
				StopThread(threadctrl);
			}
		}

		template<typename U = ThPData, typename = std::enable_if_t<has_threadpool_data<U>>>
		inline ThPData& GetData() noexcept { return m_Data; }

		template<typename U = ThPData, typename = std::enable_if_t<has_threadpool_data<U>>>
		inline const ThPData& GetData() const noexcept { return m_Data; }

	private:
		[[nodiscard]] bool AddThreadImpl(const String& thname, ThreadCallbackType&& thcallback,
										 ThData&& thdata, EventCondition* thevent, const bool event_reset) noexcept
		{
			assert(thcallback);

			try
			{
				auto& threadctrl = m_Threads.emplace_back(thname, std::move(thcallback), std::move(thdata), thevent);
				if (IsRunning())
				{
					return StartThread(threadctrl, event_reset);
				}
				else return true;
			}
			catch (const std::exception& e)
			{
				LogErr(L"Unable to add worker thread \"%s\" due to exception: %s",
					   thname.c_str(), Util::ToStringW(e.what()).c_str());
			}
			catch (...) {}

			return false;
		}

		bool StartThread(ThreadCtrl& threadctrl, const bool event_reset = true) noexcept
		{
			try
			{
				threadctrl.ShutdownEvent->Reset();

				if (threadctrl.ThreadEvent != nullptr && event_reset)
				{
					threadctrl.ThreadEvent->Reset();
				}

				threadctrl.Thread = std::thread(&ThreadPool::WorkerThreadLoop, std::ref(*this), std::ref(threadctrl));
				return true;
			}
			catch (const std::exception& e)
			{
				LogErr(L"Unable to start worker thread \"%s\" due to exception: %s",
					   threadctrl.ThreadName.c_str(), Util::ToStringW(e.what()).c_str());
			}
			catch (...) {}

			return false;
		}

		void StopThread(ThreadCtrl& threadctrl) noexcept
		{
			if (threadctrl.Thread.joinable())
			{
				try
				{
					// Set the shutdown event to notify thread
					// that we're shutting down
					threadctrl.ShutdownEvent->Set();

					if (threadctrl.ThreadEvent != nullptr)
					{
						// Set event to wake up thread
						threadctrl.ThreadEvent->Set();
					}

					// Wait for thread to exit
					threadctrl.Thread.join();
				}
				catch (const std::exception& e)
				{
					LogErr(L"Unable to stop worker thread \"%s\" due to exception: %s",
						   threadctrl.ThreadName.c_str(), Util::ToStringW(e.what()).c_str());
				}
				catch (...) {}
			}
		}

		static void WorkerThreadLoop(ThreadPool& thpool, ThreadCtrl& thctrl) noexcept
		{
			LogDbg(L"Worker thread \"%s\" (%u) starting", thctrl.ThreadName.c_str(), std::this_thread::get_id());

			Util::SetCurrentThreadName(thctrl.ThreadName);

			auto sleepms = std::chrono::milliseconds(1);
			Size workburst{ 0 };
			ThreadCallbackResult result;

			while (true)
			{
				try
				{
					// Thread with event to wait for work
					if (thctrl.ThreadEvent != nullptr) thctrl.ThreadEvent->Wait();

					// If the shutdown event is set exit the loop
					if (thctrl.ShutdownEvent->IsSet()) break;

					// Execute thread function
					if constexpr (has_threadpool_data<ThPData> && has_thread_data<ThData>)
					{
						result = thctrl.ThreadCallback(thpool.m_Data, thctrl.ThreadData, *thctrl.ShutdownEvent);
					}
					else if constexpr (has_threadpool_data<ThPData> && !has_thread_data<ThData>)
					{
						result = thctrl.ThreadCallback(thpool.m_Data, *thctrl.ShutdownEvent);
					}
					else if constexpr (!has_threadpool_data<ThPData> && has_thread_data<ThData>)
					{
						result = thctrl.ThreadCallback(thctrl.ThreadData, *thctrl.ShutdownEvent);
					}
					else result = thctrl.ThreadCallback(*thctrl.ShutdownEvent);

					if (!result.Success)
					{
						// An error occured; exit the thread
						break;
					}

					// If we did work that means it's busy so sleep
					// less, otherwise sleep increasingly longer
					if (result.DidWork)
					{
						sleepms = std::chrono::milliseconds(1);
						++workburst;

						if (workburst > thpool.m_WorkerThreadsMaxBurst)
						{
							workburst = 0;
						}
					}
					else
					{
						if (!thctrl.ThreadEvent && sleepms < thpool.m_WorkerThreadsMaxSleep)
						{
							sleepms *= 2;

							if (sleepms > thpool.m_WorkerThreadsMaxSleep)
							{
								sleepms = thpool.m_WorkerThreadsMaxSleep;
							}
						}

						workburst = 0;
					}

					if (workburst == 0)
					{
						if (thctrl.ThreadEvent)
						{
							// Yield to other threads to allow them to do some work
							std::this_thread::yield();
						}
						else
						{
							// Sleep for a while, while waiting for shutdown event
							if (thctrl.ShutdownEvent->Wait(sleepms)) break;
						}
					}
				}
				catch (const std::exception& e)
				{
					LogErr(L"An unhandled exception occured in worker thread \"%s\" (%u): %s",
						   thctrl.ThreadName.c_str(), std::this_thread::get_id(), Util::ToStringW(e.what()).c_str());
				}
				catch (...)
				{
					LogErr(L"An unhandled unknown exception occured in worker thread \"%s\" (%u)",
						   thctrl.ThreadName.c_str(), std::this_thread::get_id());
				}
			}

			LogDbg(L"Worker thread \"%s\" (%u) exiting", thctrl.ThreadName.c_str(), std::this_thread::get_id());
		}

	private:
		Size m_WorkerThreadsMaxBurst{ 64 };
		std::chrono::milliseconds m_WorkerThreadsMaxSleep{ 1 };

		ThPData m_Data;
		ThreadList m_Threads;
	};
}
