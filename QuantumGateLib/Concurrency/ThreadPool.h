// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "EventCondition.h"
#include "..\Common\Console.h"
#include "..\Common\Callback.h"

#include <vector>
#include <thread>

namespace QuantumGate::Implementation::Concurrency
{
	template<typename ThPData, typename ThData>
	class ThreadPool
	{
		using ThreadCallbackType = Callback<const std::pair<bool, bool>(ThPData&, ThData&, const EventCondition&)>;

		struct ThreadCtrl
		{
			ThreadCtrl(const String& thname, ThreadCallbackType&& thcallback,
					   ThData&& thdata, EventCondition* thevent = nullptr) :
				ThreadName(thname), ThreadCallback(std::move(thcallback)),
				ThreadData(std::move(thdata)), ThreadEvent(thevent) {}

			String ThreadName;
			ThreadCallbackType ThreadCallback;
			ThData ThreadData;
			EventCondition* ThreadEvent{ nullptr };
			std::thread Thread;
		};

	public:
		template<typename... Args>
		ThreadPool(Args&&... args) noexcept : m_Data(std::forward<Args>(args)...) {}

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool(ThreadPool&&) = default;
		virtual ~ThreadPool() = default;
		ThreadPool& operator=(const ThreadPool&) = delete;
		ThreadPool& operator=(ThreadPool&&) = default;

		inline void SetWorkerThreadsMaxBurst(Size max_burst) noexcept
		{
			m_WorkerThreadsMaxBurst = max_burst;
		}

		inline void SetWorkerThreadsMaxSleep(std::chrono::milliseconds max_sleep) noexcept
		{
			m_WorkerThreadsMaxSleep = max_sleep;
		}

		inline const bool IsRunning() const noexcept
		{
			// If at least one thread is active
			// then threadpool is running
			for (const auto& threadctrl : m_Threads)
			{
				if (threadctrl.Thread.joinable()) return true;
			}

			return false;
		}

		[[nodiscard]] const bool AddThread(const String& thname, ThreadCallbackType&& thcallback,
										   ThData&& thdata, EventCondition* thevent = nullptr) noexcept
		{
			assert(thcallback);

			try
			{
				m_Threads.emplace_back(thname, std::move(thcallback), std::move(thdata), thevent);

				return true;
			}
			catch (...) {}

			return false;
		}

		void Clear() noexcept
		{
			assert(!IsRunning());

			m_Threads.clear();
		}

		[[nodiscard]] const bool Startup() noexcept
		{
			assert(!IsRunning());

			m_ShutdownEvent.Reset();

			// Start all threads
			for (auto& threadctrl : m_Threads)
			{
				try
				{
					threadctrl.Thread = std::thread(&ThreadPool::WorkerThreadLoop, std::ref(*this), std::ref(threadctrl),
													std::ref(m_ShutdownEvent));
				}
				catch (const std::exception& e)
				{
					LogErr(L"Unable to start worker thread \"%s\" due to exception: %s",
						   threadctrl.ThreadName.c_str(), Util::ToStringW(e.what()).c_str());
				}
			}

			return IsRunning();
		}

		void Shutdown() noexcept
		{
			assert(IsRunning());

			// Set the shutdown event to notify threads
			// that we're shutting down
			m_ShutdownEvent.Set();

			// Wait for all threads to shut down
			for (auto& threadctrl : m_Threads)
			{
				if (threadctrl.Thread.joinable())
				{
					try
					{
						if (threadctrl.ThreadEvent != nullptr)
						{
							// Set event to wake up worker thread
							threadctrl.ThreadEvent->Set();
						}

						threadctrl.Thread.join();
					}
					catch (const std::exception& e)
					{
						LogErr(L"Unable to stop worker thread \"%s\" due to exception: %s",
							   threadctrl.ThreadName.c_str(), Util::ToStringW(e.what()).c_str());
					}
				}
			}
		}

		inline ThPData& Data() noexcept { return m_Data; }
		inline const ThPData& Data() const noexcept { return m_Data; }

	private:
		static void WorkerThreadLoop(ThreadPool& thpool, ThreadCtrl& thctrl,
									 EventCondition& shutdown_event) noexcept
		{
			LogDbg(L"Worker thread \"%s\" (%u) starting", thctrl.ThreadName.c_str(), std::this_thread::get_id());

			Util::SetCurrentThreadName(thctrl.ThreadName);

			auto sleepms = std::chrono::milliseconds(1);
			Size workburst{ 0 };

			while (true)
			{
				try
				{
					// Thread with event to wait for work
					if (thctrl.ThreadEvent) thctrl.ThreadEvent->Wait();

					// If the shutdown event is set exit the loop
					if (shutdown_event.IsSet()) break;

					const auto ret = thctrl.ThreadCallback(thpool.m_Data, thctrl.ThreadData, shutdown_event);
					if (!ret.first)
					{
						// An error occured; exit the thread
						break;
					}

					// If we did work that means it's busy so sleep
					// less, otherwise sleep increasingly longer
					if (ret.second)
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
						if (sleepms < thpool.m_WorkerThreadsMaxSleep)
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
							if (shutdown_event.Wait(sleepms)) break;
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
		EventCondition m_ShutdownEvent{ false };

		Size m_WorkerThreadsMaxBurst{ 64 };
		std::chrono::milliseconds m_WorkerThreadsMaxSleep{ 1 };

		ThPData m_Data;
		std::vector<ThreadCtrl> m_Threads;
	};
}
