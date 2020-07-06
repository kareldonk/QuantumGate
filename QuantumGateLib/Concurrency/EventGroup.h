// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <thread>
#include <chrono>

#include "SharedSpinMutex.h"
#include "..\Common\ScopeGuard.h"

namespace QuantumGate::Implementation::Concurrency
{
	class EventGroup final
	{
		static constexpr Size MaxNumEvents{ MAXIMUM_WAIT_OBJECTS };

		using EventHandle = HANDLE;
		using EventHandles = Vector<EventHandle>;
		using EventHandles_ThS = ThreadSafe<EventHandles, SharedSpinMutex>;

		class EventSubgroup final
		{		
		public:
			EventSubgroup() noexcept = delete;
			EventSubgroup(const EventHandle handle) noexcept : m_MainEvent(handle) {}
			EventSubgroup(const EventSubgroup&) = delete;
			EventSubgroup(EventSubgroup&&) = delete;
			~EventSubgroup() { Deinitialize(); };
			EventSubgroup& operator=(const EventSubgroup&) = delete;
			EventSubgroup& operator=(EventSubgroup&&) = delete;

			[[nodiscard]] bool Initialize() noexcept
			{
				assert(m_ShutdownEvent == nullptr);

				m_ShutdownEvent = ::CreateEvent(nullptr, true, false, nullptr);
				if (m_ShutdownEvent != nullptr)
				{
					try
					{
						m_SubEvents.WithUniqueLock([&](EventHandles& handles)
						{
							handles.reserve(MaxNumEvents);

							// The shutdown event should always be the first
							// event in the array and should always be present
							handles.emplace_back(m_ShutdownEvent);
						});
					}
					catch (const std::exception& e)
					{
						LogErr(L"Failed to allocate memory for event subgroup due to exception: %s",
							   Util::ToStringW(e.what()).c_str());
					}
					catch (...) {}

					return true;
				}
				else
				{
					LogErr(L"Couldn't create shutdown event for a event subgroup (%s)",
						   GetLastSysErrorString().c_str());
				}

				return false;
			}

			void Deinitialize() noexcept
			{
				try
				{
					if (m_ShutdownEvent != nullptr)
					{
						// Set shutdown event to get the thread to exit
						// so we can join it below
						::SetEvent(m_ShutdownEvent);

						if (m_EventThread.joinable()) m_EventThread.join();

						m_SubEvents.WithUniqueLock()->clear();

						::CloseHandle(m_ShutdownEvent);

						m_ShutdownEvent = nullptr;
					}
				}
				catch (...) {}
			}

			[[nodiscard]] bool AddEvent(const EventHandle handle) noexcept
			{
				assert(m_ShutdownEvent != nullptr);

				auto success = false;

				try
				{
					m_SubEvents.WithUniqueLock([&](EventHandles& handles)
					{
						if (handles.size() < (MaxNumEvents - 1)) handles.emplace_back(handle);
						success = true;
					});

					if (success && !m_EventThread.joinable())
					{
						::ResetEvent(m_ShutdownEvent);
						m_EventThread = std::thread(EventSubgroup::ThreadProc, this);
					}
				}
				catch (...) { success = false; }

				return success;
			}

			void RemoveEvent(const EventHandle handle) noexcept
			{
				assert(m_ShutdownEvent != nullptr);

				try
				{
					auto stop_thread = false;

					m_SubEvents.WithUniqueLock([&](EventHandles& handles)
					{
						const auto it = std::find_if(handles.begin(), handles.end(), [&](const auto chandle)
						{
							return (chandle == handle);
						});

						if (it != handles.end())
						{
							handles.erase(it);

							// Stop the thread if just the shutdown event
							// is left in the array; the thread will get
							// started again once another event is added
							stop_thread = (handles.size() == 1);
						}
					});

					if (stop_thread)
					{
						// Set shutdown event to get the thread to exit
						// so we can join it below
						::SetEvent(m_ShutdownEvent);
						m_EventThread.join();

						::ResetEvent(m_MainEvent);
					}
				}
				catch (...) {}
			}

			[[nodiscard]] bool HasEvent(const EventHandle handle) noexcept
			{
				assert(m_ShutdownEvent != nullptr);

				auto found = false;

				try
				{
					m_SubEvents.WithSharedLock([&](const EventHandles& handles)
					{
						const auto it = std::find_if(handles.begin(), handles.end(), [&](const auto chandle)
						{
							return (chandle == handle);
						});

						found = (it != handles.end());
					});
				}
				catch (...) {}

				return found;
			}

			[[nodiscard]] bool CanAddEvent() noexcept
			{
				assert(m_ShutdownEvent != nullptr);

				return ((MaxNumEvents - 1) > m_SubEvents.WithSharedLock()->size());
			}

			[[nodiscard]] bool IsEmpty() noexcept
			{
				assert(m_ShutdownEvent != nullptr);

				return (m_SubEvents.WithSharedLock()->size() == 1);
			}

		private:
			static void ThreadProc(EventSubgroup* subgroup) noexcept
			{
				LogDbg(L"Event subgroup thread (%u) starting", std::this_thread::get_id());

				while (true)
				{
					DWORD num_handles{ 0 };

					std::array<EventHandle, MaxNumEvents> event_handles{ nullptr };

					subgroup->m_SubEvents.WithSharedLock([&](const EventHandles& handles)
					{
						num_handles = static_cast<DWORD>(handles.size());
						std::memcpy(event_handles.data(), handles.data(), num_handles * sizeof(EventHandle));
					});

					const auto ret = ::WaitForMultipleObjectsEx(num_handles, event_handles.data(), false, 1, false);
					if (ret == WAIT_OBJECT_0)
					{
						break;
					}
					else if (ret > WAIT_OBJECT_0 and ret < (WAIT_OBJECT_0 + num_handles))
					{
						::SetEvent(subgroup->m_MainEvent);
					}
					else if (ret == WAIT_FAILED)
					{
						::ResetEvent(subgroup->m_MainEvent);

						LogErr(L"Wait() failed for event subgroup (%s)", GetLastSysErrorString().c_str());

						std::this_thread::sleep_for(std::chrono::milliseconds(1));
					}
					else
					{
						::ResetEvent(subgroup->m_MainEvent);
					}
				}

				LogDbg(L"Event subgroup thread (%u) exiting", std::this_thread::get_id());
			}

		private:
			EventHandle m_ShutdownEvent{ nullptr };
			EventHandle m_MainEvent{ nullptr };
			EventHandles_ThS m_SubEvents;
			std::thread m_EventThread;
		};

		using EventSubgroups = Vector<std::unique_ptr<EventSubgroup>>;

		struct Data final
		{
			EventHandles MainHandles;
			EventSubgroups EventSubgroups;
		};

		using Data_ThS = ThreadSafe<Data, std::shared_mutex>;

	public:
		struct WaitResult final
		{
			bool Waited{ false };
			bool HadEvent{ false };
		};

		EventGroup() noexcept {}
		EventGroup(const EventGroup&) = delete;
		EventGroup(EventGroup&&) = delete;
		~EventGroup() { Shutdown(); }
		EventGroup& operator=(const EventGroup&) = delete;
		EventGroup& operator=(EventGroup&&) = delete;

		[[nodiscard]] bool Initialize() noexcept
		{
			auto success = false;

			try
			{
				m_Data.WithUniqueLock([](Data& data)
				{
					data.MainHandles.reserve(MaxNumEvents);
				});

				success = true;
			}
			catch (const std::exception& e)
			{
				LogErr(L"Failed to allocate memory for eventgroup due to exception: %s",
					   Util::ToStringW(e.what()).c_str());
			}
			catch (...)
			{
				LogErr(L"Failed to initialize eventgroup due to an unknown exception");
			}

			return success;
		}

		void Shutdown() noexcept
		{
			try
			{
				m_Data.WithUniqueLock([](Data& data)
				{
					data.EventSubgroups.clear();

					for (const auto& handle : data.MainHandles)
					{
						::CloseHandle(handle);
					}

					data.MainHandles.clear();
				});
			}
			catch (...) {}
		}

		[[nodiscard]] bool AddEvent(const EventHandle handle) noexcept
		{
			auto success = false;

			try
			{
				m_Data.WithUniqueLock([&](Data& data)
				{
					auto subgroup = GetSubgroup(data);
					if (subgroup != nullptr)
					{
						if (subgroup->AddEvent(handle)) success = true;
						else
						{
							LogErr(L"Failed to add event to subgroup");
						}
					}
					else
					{
						LogErr(L"Couldn't get a subgroup to add event");
					}
				});
			}
			catch (...) {}

			return success;
		}

		[[nodiscard]] void RemoveEvent(const EventHandle handle) noexcept
		{
			try
			{
				m_Data.WithUniqueLock([&](Data& data)
				{
					for (auto it = data.EventSubgroups.begin(); it != data.EventSubgroups.end(); ++it)
					{
						if ((*it)->HasEvent(handle))
						{
							(*it)->RemoveEvent(handle);

							if ((*it)->IsEmpty()) data.EventSubgroups.erase(it);

							return;
						}
					}

					LogErr(L"Couldn't remove an event from a event subgroup; the event wasn't found");
				});
			}
			catch (...) {}
		}

		WaitResult Wait(const std::chrono::milliseconds max_wait_time) noexcept
		{
			WaitResult result{ .Waited = false, .HadEvent = false };

			DWORD num_handles{ 0 };

			std::array<EventHandle, MaxNumEvents> event_handles{ nullptr };

			m_Data.WithSharedLock([&](const Data& data)
			{
				num_handles = static_cast<DWORD>(data.MainHandles.size());
				std::memcpy(event_handles.data(), data.MainHandles.data(), num_handles * sizeof(EventHandle));
			});

			if (num_handles > 0)
			{
				const auto ret = ::WaitForMultipleObjectsEx(num_handles, event_handles.data(), false,
															static_cast<DWORD>(max_wait_time.count()), false);
				if (ret >= WAIT_OBJECT_0 and ret < (WAIT_OBJECT_0 + num_handles))
				{
					result.Waited = true;
					result.HadEvent = true;
				}
				else if (ret == WAIT_TIMEOUT)
				{
					result.Waited = true;
				}
				else if (ret == WAIT_FAILED)
				{
					LogErr(L"Wait() failed for event group (%s)", GetLastSysErrorString().c_str());
				}
			}

			return result;
		}

	private:
		[[nodiscard]] EventSubgroup* GetSubgroup(Data& data) noexcept
		{
			// First we look for an existing subgroup
			// that will accept another event
			for (auto& subgroup : data.EventSubgroups)
			{
				if (subgroup->CanAddEvent()) return subgroup.get();
			}

			String error;

			// Add another subgroup if possible
			if (data.EventSubgroups.size() < MaxNumEvents)
			{
				const auto handle = ::CreateEvent(nullptr, true, false, L"");
				if (handle != nullptr)
				{
					auto sg = MakeScopeGuard([&] { ::CloseHandle(handle); });

					try
					{
						data.MainHandles.emplace_back(handle);

						auto sg2 = MakeScopeGuard([&] { data.MainHandles.pop_back(); });

						auto& subgroup = data.EventSubgroups.emplace_back(std::make_unique<EventSubgroup>(handle));

						auto sg3 = MakeScopeGuard([&] { data.EventSubgroups.pop_back(); });

						if (subgroup->Initialize())
						{
							sg.Deactivate();
							sg2.Deactivate();
							sg3.Deactivate();

							return subgroup.get();
						}
						else error = L"event subgroup initialization failed";
					}
					catch (const std::exception& e)
					{
						error = Util::FormatString(L"an exception occured: %s", Util::ToStringW(e.what()).c_str());
					}
					catch (...)
					{
						error = Util::FormatString(L"an unknown exception occured");
					}
				}
				else
				{
					error = Util::FormatString(L"couldn't create a new event (%s)", GetLastSysErrorString().c_str());
				}
			}
			else error = Util::FormatString(L"the maximum number of event subgroups (%zu) has been reached", MaxNumEvents);

			LogErr(L"Failed to add a new event subgroup; %s", error.c_str());

			return nullptr;
		}

	private:
		Data_ThS m_Data;
	};
}