// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Event.h"
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
	private:
		template<typename U>
		static constexpr bool has_threadpool_data = !std::is_same_v<std::remove_cv_t<std::decay_t<U>>, NoThreadPoolData>;

		template<typename U>
		static constexpr bool has_thread_data = !std::is_same_v<std::remove_cv_t<std::decay_t<U>>, NoThreadData>;

		template<bool thpdata = has_threadpool_data<ThPData>, bool thdata = has_thread_data<ThData>>
		struct thread_callback
		{
			using type = Callback<void(const Event&)>;
		};

		template<>
		struct thread_callback<true, false>
		{
			using type = Callback<void(ThPData&, const Event&)>;
		};

		template<>
		struct thread_callback<false, true>
		{
			using type = Callback<void(ThData&, const Event&)>;
		};

		template<>
		struct thread_callback<true, true>
		{
			using type = Callback<void(ThPData&, ThData&, const Event&)>;
		};

		template<bool thpdata = has_threadpool_data<ThPData>, bool thdata = has_thread_data<ThData>>
		struct thread_wait_callback
		{
			using type = Callback<void(const Event&)>;
		};

		template<>
		struct thread_wait_callback<true, false>
		{
			using type = Callback<void(ThPData&, const Event&)>;
		};

		template<>
		struct thread_wait_callback<false, true>
		{
			using type = Callback<void(ThData&, const Event&)>;
		};

		template<>
		struct thread_wait_callback<true, true>
		{
			using type = Callback<void(ThPData&, ThData&, const Event&)>;
		};

		template<bool thpdata = has_threadpool_data<ThPData>, bool thdata = has_thread_data<ThData>>
		struct thread_wait_interrupt_callback
		{
			using type = Callback<void()>;
		};

		template<>
		struct thread_wait_interrupt_callback<true, false>
		{
			using type = Callback<void(ThPData&)>;
		};

		template<>
		struct thread_wait_interrupt_callback<false, true>
		{
			using type = Callback<void(ThData&)>;
		};

		template<>
		struct thread_wait_interrupt_callback<true, true>
		{
			using type = Callback<void(ThPData&, ThData&)>;
		};

	public:
		using ThreadCallbackType = typename thread_callback<>::type;
		using ThreadWaitCallbackType = typename thread_wait_callback<>::type;
		using ThreadWaitInterruptCallbackType = typename thread_wait_interrupt_callback<>::type;

	private:
		struct ThreadCtrl final
		{
			ThreadCtrl(const String& thname, ThData&& thdata, ThreadCallbackType&& thcallback,
					   ThreadWaitCallbackType&& thwaitcb = nullptr, ThreadWaitInterruptCallbackType&& thwaitintcb = nullptr) :
				ThreadName(thname),
				ThreadData(std::move(thdata)),
				ThreadCallback(std::move(thcallback)),
				ThreadWaitCallback(std::move(thwaitcb)),
				ThreadWaitInterruptCallback(std::move(thwaitintcb))
			{}

			String ThreadName;
			[[no_unique_address]] ThData ThreadData;
			Event ShutdownEvent;
			std::thread Thread;
			ThreadCallbackType ThreadCallback{ nullptr };
			ThreadWaitCallbackType ThreadWaitCallback{ nullptr };
			ThreadWaitInterruptCallbackType ThreadWaitInterruptCallback{ nullptr };
		};

		using ThreadList = Containers::List<ThreadCtrl>;

	public:
		template<typename Iterator>
		class Thread final
		{
			friend class ThreadPool;

		public:
			Thread() = delete;
			Thread(const Iterator it) noexcept : m_ThreadIterator(it) {}

			[[nodiscard]] std::thread::id GetID() const noexcept { return m_ThreadIterator->Thread.get_id(); }
			[[nodiscard]] const String& GetName() const noexcept { return m_ThreadIterator->ThreadName; }
			[[nodiscard]] bool IsRunning() const noexcept { return m_ThreadIterator->Thread.joinable(); }

			template<typename U = ThData, typename = std::enable_if_t<has_thread_data<U>>>
			[[nodiscard]] auto& GetData() noexcept { return m_ThreadIterator->ThreadData; }

		private:
			Iterator m_ThreadIterator;
		};

		using ThreadType = Thread<typename ThreadList::iterator>;
		using ConstThreadType = Thread<typename ThreadList::const_iterator>;

		template<typename... Args>
		ThreadPool(Args&&... args) noexcept : m_Data(std::forward<Args>(args)...) {}

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool(ThreadPool&&) noexcept = default;
		~ThreadPool() = default;
		ThreadPool& operator=(const ThreadPool&) = delete;
		ThreadPool& operator=(ThreadPool&&) noexcept = default;

		[[nodiscard]] inline bool IsRunning() const noexcept
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
											ThreadWaitCallbackType&& thwaitcb = nullptr,
											ThreadWaitInterruptCallbackType&& thwaitintcb = nullptr) noexcept
		{
			return AddThreadImpl(thname, NoThreadData{}, std::move(thcallback), std::move(thwaitcb), std::move(thwaitintcb));
		}

		template<typename U = ThData, typename = std::enable_if_t<has_thread_data<U>>>
		[[nodiscard]] inline bool AddThread(const String& thname, ThData&& thdata,
											ThreadCallbackType&& thcallback,
											ThreadWaitCallbackType&& thwaitcb = nullptr,
											ThreadWaitInterruptCallbackType&& thwaitintcb = nullptr) noexcept
		{
			return AddThreadImpl(thname, std::move(thdata), std::move(thcallback), std::move(thwaitcb), std::move(thwaitintcb));
		}

		[[nodiscard]] inline std::pair<bool, std::optional<ThreadType>> RemoveThread(ThreadType&& thread) noexcept
		{
			if (thread.IsRunning())
			{
				StopThread(*(thread.m_ThreadIterator));
			}

			auto next_it = m_Threads.erase(thread.m_ThreadIterator);
			if (next_it != m_Threads.end()) return std::make_pair(true, ThreadType(next_it));

			return std::make_pair(true, std::nullopt);
		}

		inline Size GetSize() const noexcept { return m_Threads.size(); }

		[[nodiscard]] inline std::optional<ThreadType> GetFirstThread() noexcept
		{
			if (m_Threads.size() > 0)
			{
				return ThreadType(m_Threads.begin());
			}

			return std::nullopt;
		}

		[[nodiscard]] inline std::optional<ConstThreadType> GetFirstThread() const noexcept
		{
			if (m_Threads.size() > 0)
			{
				return ConstThreadType(m_Threads.cbegin());
			}

			return std::nullopt;
		}

		[[nodiscard]] inline std::optional<ThreadType> GetNextThread(const ThreadType& thread) noexcept
		{
			auto it = std::next(thread.m_ThreadIterator, 1);
			if (it != m_Threads.end()) return ThreadType(it);

			return std::nullopt;
		}

		[[nodiscard]] inline std::optional<ConstThreadType> GetNextThread(const ConstThreadType& thread) const noexcept
		{
			auto it = std::next(thread.m_ThreadIterator, 1);
			if (it != m_Threads.cend()) return ConstThreadType(it);

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
				threadctrl.ShutdownEvent.Set();
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
		[[nodiscard]] bool AddThreadImpl(const String& thname, ThData&& thdata,
										 ThreadCallbackType&& thcallback,
										 ThreadWaitCallbackType&& thwaitcb,
										 ThreadWaitInterruptCallbackType&& thwaitintcb) noexcept
		{
			assert(thcallback);

			try
			{
				auto& threadctrl = m_Threads.emplace_back(thname, std::move(thdata), std::move(thcallback),
														  std::move(thwaitcb), std::move(thwaitintcb));
				if (IsRunning())
				{
					return StartThread(threadctrl);
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

		bool StartThread(ThreadCtrl& threadctrl) noexcept
		{
			try
			{
				threadctrl.ShutdownEvent.Reset();

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
					threadctrl.ShutdownEvent.Set();

					if (threadctrl.ThreadWaitInterruptCallback)
					{
						// Wake up thread
						if constexpr (has_threadpool_data<ThPData> && has_thread_data<ThData>)
						{
							threadctrl.ThreadWaitInterruptCallback(m_Data, threadctrl.ThreadData);
						}
						else if constexpr (has_threadpool_data<ThPData> && !has_thread_data<ThData>)
						{
							threadctrl.ThreadWaitInterruptCallback(m_Data);
						}
						else if constexpr (!has_threadpool_data<ThPData> && has_thread_data<ThData>)
						{
							threadctrl.ThreadWaitInterruptCallback(threadctrl.ThreadData);
						}
						else threadctrl.ThreadWaitInterruptCallback();
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

			while (true)
			{
				try
				{
					// Thread with callback to wait for work
					if (thctrl.ThreadWaitCallback)
					{
						if constexpr (has_threadpool_data<ThPData> && has_thread_data<ThData>)
						{
							thctrl.ThreadWaitCallback(thpool.m_Data, thctrl.ThreadData, thctrl.ShutdownEvent);
						}
						else if constexpr (has_threadpool_data<ThPData> && !has_thread_data<ThData>)
						{
							thctrl.ThreadWaitCallback(thpool.m_Data, thctrl.ShutdownEvent);
						}
						else if constexpr (!has_threadpool_data<ThPData> && has_thread_data<ThData>)
						{
							thctrl.ThreadWaitCallback(thctrl.ThreadData, thctrl.ShutdownEvent);
						}
						else thctrl.ThreadWaitCallback(thctrl.ShutdownEvent);
					}

					// If the shutdown event is set exit the loop
					if (thctrl.ShutdownEvent.IsSet()) break;

					// Execute thread function
					if constexpr (has_threadpool_data<ThPData> && has_thread_data<ThData>)
					{
						thctrl.ThreadCallback(thpool.m_Data, thctrl.ThreadData, thctrl.ShutdownEvent);
					}
					else if constexpr (has_threadpool_data<ThPData> && !has_thread_data<ThData>)
					{
						thctrl.ThreadCallback(thpool.m_Data, thctrl.ShutdownEvent);
					}
					else if constexpr (!has_threadpool_data<ThPData> && has_thread_data<ThData>)
					{
						thctrl.ThreadCallback(thctrl.ThreadData, thctrl.ShutdownEvent);
					}
					else thctrl.ThreadCallback(thctrl.ShutdownEvent);
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
		[[no_unique_address]] ThPData m_Data;
		ThreadList m_Threads;
	};
}
