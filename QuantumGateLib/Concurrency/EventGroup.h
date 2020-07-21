// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <thread>
#include <chrono>

#include "Event.h"
#include "ThreadSafe.h"
#include "SharedSpinMutex.h"
#include "..\Common\ScopeGuard.h"

namespace QuantumGate::Implementation::Concurrency
{
	class EventGroup final
	{
		static constexpr Size MaxNumEvents{ MAXIMUM_WAIT_OBJECTS };

		using EventHandle = Event::HandleType;
		using EventHandles = Vector<EventHandle>;
		using EventHandles_ThS = ThreadSafe<EventHandles, SharedSpinMutex>;

		class EventSubgroup final
		{		
		public:
			EventSubgroup() noexcept = delete;

			EventSubgroup(const EventHandle main_event, const EventHandle barrier_event) noexcept :
				m_MainEvent(main_event), m_BarrierEvent(barrier_event) {}

			EventSubgroup(const EventSubgroup&) = delete;
			EventSubgroup(EventSubgroup&&) = delete;
			~EventSubgroup() { Deinitialize(); };
			EventSubgroup& operator=(const EventSubgroup&) = delete;
			EventSubgroup& operator=(EventSubgroup&&) = delete;

			EventHandle GetMainEvent() const noexcept { return m_MainEvent; }

			[[nodiscard]] bool Initialize() noexcept
			{
				assert(m_ShutdownEvent == nullptr);

				m_ShutdownEvent = ::CreateEvent(nullptr, true, false, nullptr);
				if (m_ShutdownEvent != nullptr)
				{
					auto sg = MakeScopeGuard([&]
					{
						::CloseHandle(m_ShutdownEvent);
						m_ShutdownEvent = nullptr;
					});

					try
					{
						m_SubEvents.WithUniqueLock([&](EventHandles& handles)
						{
							handles.reserve(MaxNumEvents);

							// The shutdown event should always be the first
							// event in the array and should always be present
							handles.emplace_back(m_ShutdownEvent);
						});

						sg.Deactivate();

						return true;
					}
					catch (const std::exception& e)
					{
						LogErr(L"Failed to allocate memory for event subgroup due to exception: %s",
							   Util::ToStringW(e.what()).c_str());
					}
					catch (...) {}
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

						::ResetEvent(m_MainEvent);

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
						if (handles.size() < MaxNumEvents)
						{
							handles.emplace_back(handle);

							UpdateMainEvent(handles);

							if (!m_EventThread.joinable())
							{
								::ResetEvent(m_ShutdownEvent);
								m_EventThread = std::thread(EventSubgroup::ThreadProc, this);
							}

							success = true;
						}
					});
				}
				catch (...) { success = false; }

				return success;
			}

			void RemoveEvent(const EventHandle handle) noexcept
			{
				assert(m_ShutdownEvent != nullptr);

				try
				{
					m_SubEvents.WithUniqueLock([&](EventHandles& handles)
					{
						const auto it = std::find_if(handles.begin(), handles.end(), [&](const auto chandle)
						{
							return (chandle == handle);
						});

						if (it != handles.end())
						{
							handles.erase(it);

							if (handles.size() > 1)
							{
								UpdateMainEvent(handles);
							}
							else
							{
								// Stop the thread if just the shutdown event
								// is left in the array; the thread will get
								// started again once another event is added
								::SetEvent(m_ShutdownEvent);
								m_EventThread.join();

								::ResetEvent(m_MainEvent);
							}
						}
					});
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

				return (MaxNumEvents > m_SubEvents.WithSharedLock()->size());
			}

			[[nodiscard]] bool IsEmpty() noexcept
			{
				assert(m_ShutdownEvent != nullptr);

				return (m_SubEvents.WithSharedLock()->size() == 1);
			}

		private:
			void UpdateMainEvent(const EventHandles& handles) noexcept
			{
				const auto ret = ::WaitForMultipleObjectsEx(static_cast<DWORD>(handles.size()),
															handles.data(), false, 0, false);
				if (ret > WAIT_OBJECT_0 && ret < (WAIT_OBJECT_0 + handles.size()))
				{
					::SetEvent(m_MainEvent);
				}
				else ::ResetEvent(m_MainEvent);
			}

			static void ThreadProc(EventSubgroup* subgroup) noexcept
			{
				assert(subgroup->m_ShutdownEvent != nullptr && subgroup->m_BarrierEvent != nullptr);

				LogDbg(L"Event subgroup thread (%u) starting", std::this_thread::get_id());

				auto exit = false;

				while (!exit)
				{
					std::array<EventHandle, 2> barrier_handles{
						subgroup->m_ShutdownEvent,
						subgroup->m_BarrierEvent
					};

					const auto ret = ::WaitForMultipleObjectsEx(static_cast<DWORD>(barrier_handles.size()),
																barrier_handles.data(), false, INFINITE, false);
					if (ret == WAIT_OBJECT_0)
					{
						// Shutdown event
						exit = true;
					}
					else if (ret == WAIT_FAILED)
					{
						::ResetEvent(subgroup->m_MainEvent);

						LogErr(L"WaitForMultipleObjectsEx() failed for event subgroup barrier (%s)",
							   GetLastSysErrorString().c_str());

						// This is to prevent this thread from spinning and hogging CPU 
						// in the case where the above wait fails repeatedly
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
					}
					else
					{
						auto handles = subgroup->m_SubEvents.TryWithSharedLock();
						if (handles)
						{
							const auto ret2 = ::WaitForMultipleObjectsEx(static_cast<DWORD>(handles->size()),
																		 handles->data(), false, 1, false);
							if (ret2 == WAIT_OBJECT_0)
							{
								// Shutdown event
								exit = true;
							}
							else if (ret2 > WAIT_OBJECT_0 && ret2 < (WAIT_OBJECT_0 + handles->size()))
							{
								// One or more of the events were set
								::SetEvent(subgroup->m_MainEvent);
							}
							else if (ret2 == WAIT_TIMEOUT)
							{
								// None of the events were set
								::ResetEvent(subgroup->m_MainEvent);
							}
							else if (ret2 == WAIT_FAILED)
							{
								::ResetEvent(subgroup->m_MainEvent);

								LogErr(L"WaitForMultipleObjectsEx() failed for event subgroup (%s)",
										GetLastSysErrorString().c_str());
							}
						}
					}
				}

				LogDbg(L"Event subgroup thread (%u) exiting", std::this_thread::get_id());
			}

		private:
			EventHandle m_ShutdownEvent{ nullptr };
			EventHandle m_MainEvent{ nullptr };
			EventHandle m_BarrierEvent{ nullptr };
			EventHandles_ThS m_SubEvents;
			std::thread m_EventThread;
		};

		using EventSubgroups = Vector<std::unique_ptr<EventSubgroup>>;

		struct MainEventHandle final
		{
			bool InUse{ false };
			EventHandle Handle{ nullptr };
		};

		using MainEventHandles = Vector<MainEventHandle>;

		struct Data final
		{
			MainEventHandles MainEvents;
			EventHandle EventSubgroupBarrier{ nullptr };
			EventSubgroups EventSubgroups;
		};

		using Init_ThS = ThreadSafe<bool, std::shared_mutex>;
		using Data_ThS = ThreadSafe<Data, std::shared_mutex>;

	public:
		static constexpr Size MaximumNumberOfUserEvents{ MaxNumEvents * (MaxNumEvents - 1) };

		struct WaitResult final
		{
			bool Waited{ false };
			bool HadEvent{ false };
		};

		EventGroup() noexcept {}
		EventGroup(const EventGroup&) = delete;
		EventGroup(EventGroup&&) = delete;
		~EventGroup() { Deinitialize(); }
		EventGroup& operator=(const EventGroup&) = delete;
		EventGroup& operator=(EventGroup&&) = delete;

		[[nodiscard]] bool Initialize() noexcept
		{
			auto success = false;

			try
			{
				m_Data.WithUniqueLock([&](Data& data)
				{
					data.MainEvents.reserve(MaxNumEvents);

					data.EventSubgroupBarrier = ::CreateEvent(nullptr, true, false, nullptr);
					if (data.EventSubgroupBarrier != nullptr)
					{
						success = true;
					}
					else
					{
						LogErr(L"Couldn't create event subgroup barrier (%s)", GetLastSysErrorString().c_str());
					}
				});
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

		void Deinitialize() noexcept
		{
			try
			{
				m_Data.WithUniqueLock([&](Data& data)
				{
					data.EventSubgroups.clear();

					for (const auto& main_event : data.MainEvents)
					{
						::CloseHandle(main_event.Handle);
					}

					data.MainEvents.clear();

					if (data.EventSubgroupBarrier != nullptr)
					{
						::CloseHandle(data.EventSubgroupBarrier);
						data.EventSubgroupBarrier = nullptr;
					}
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
						if (subgroup->AddEvent(handle))
						{
							success = true;
						}
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

		void RemoveEvent(const EventHandle handle) noexcept
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

							// If the subgroup is empty we can remove it
							if ((*it)->IsEmpty())
							{
								RemoveMainEvent(data, (*it)->GetMainEvent());

								data.EventSubgroups.erase(it);
							}

							return;
						}
					}

					LogErr(L"Couldn't remove an event from a event subgroup; the event wasn't found");
				});
			}
			catch (...) {}
		}

		void RemoveAllEvents() noexcept
		{
			try
			{
				m_Data.WithUniqueLock([&](Data& data)
				{
					for (auto it = data.EventSubgroups.begin(); it != data.EventSubgroups.end(); ++it)
					{
						RemoveMainEvent(data, (*it)->GetMainEvent());
					}

					data.EventSubgroups.clear();
				});
			}
			catch (...) {}
		}

		WaitResult Wait(const std::chrono::milliseconds max_wait_time) noexcept
		{
			WaitResult result{ .Waited = false, .HadEvent = false };

			DWORD num_handles{ 0 };
			std::array<EventHandle, MaxNumEvents> event_handles{ nullptr };
			EventHandle barrier_handle{ nullptr };

			m_Data.WithUniqueLock([&](Data& data)
			{
				assert(data.MainEvents.size() <= event_handles.size());

				num_handles = static_cast<DWORD>(data.MainEvents.size());
				
				for (MainEventHandles::size_type x = 0; x < data.MainEvents.size(); ++x)
				{
					event_handles[x] = data.MainEvents[x].Handle;
				}

				barrier_handle = data.EventSubgroupBarrier;
			});

			if (num_handles > 0)
			{
				::SetEvent(barrier_handle);

				const auto ret = ::WaitForMultipleObjectsEx(num_handles, event_handles.data(), false,
															static_cast<DWORD>(max_wait_time.count()), false);

				::ResetEvent(barrier_handle);

				if (ret >= WAIT_OBJECT_0 && ret < (WAIT_OBJECT_0 + num_handles))
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
		[[nodiscard]] EventHandle AddMainEvent(Data& data) noexcept
		{
			// First we look for an existing handle that is not in use
			for (auto& main_event : data.MainEvents)
			{
				if (!main_event.InUse) return main_event.Handle;
			}

			String error;

			try
			{
				// Add another handle if possible
				if (data.MainEvents.size() < MaxNumEvents)
				{
					const auto main_handle = ::CreateEvent(nullptr, true, false, nullptr);
					if (main_handle != nullptr)
					{
						auto sg = MakeScopeGuard([&] { ::CloseHandle(main_handle); });

						try
						{
							const auto& handle = data.MainEvents.emplace_back(MainEventHandle{ true, main_handle });

							assert(data.MainEvents.size() <= MaxNumEvents);

							sg.Deactivate();

							return handle.Handle;
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
					else error = Util::FormatString(L"failed to create a new event");
				}
				else error = Util::FormatString(L"the maximum number of main events (%zu) has been reached", MaxNumEvents);
			}
			catch (...) {}

			LogErr(L"Failed to add a new main event; %s", error.c_str());

			return nullptr;
		}

		void RemoveMainEvent(Data& data, const EventHandle event_handle) noexcept
		{
			const auto mit = std::find_if(data.MainEvents.begin(), data.MainEvents.end(),
										  [&](const auto main_event)
			{
				return (main_event.Handle == event_handle);
			});

			// Should always find the main handle
			assert(mit != data.MainEvents.end());

			if (mit != data.MainEvents.end())
			{
				// We just mark the event as not in use because
				// another thread may be waiting on the event;
				// actual removal will happen during deinitialization
				mit->InUse = false;
			}
		}

		[[nodiscard]] EventSubgroup* GetSubgroup(Data& data) noexcept
		{
			// First we look for an existing subgroup
			// that will accept another event
			for (auto& subgroup : data.EventSubgroups)
			{
				if (subgroup->CanAddEvent()) return subgroup.get();
			}

			String error;

			try
			{
				// Add another subgroup if possible
				auto main_handle = AddMainEvent(data);
				if (main_handle != nullptr)
				{
					auto sg = MakeScopeGuard([&] { RemoveMainEvent(data, main_handle); });

					try
					{
						auto& subgroup = data.EventSubgroups.emplace_back(
							std::make_unique<EventSubgroup>(main_handle, data.EventSubgroupBarrier)
						);

						assert(data.EventSubgroups.size() <= MaxNumEvents);

						auto sg2 = MakeScopeGuard([&] { data.EventSubgroups.pop_back(); });

						if (subgroup->Initialize())
						{
							sg.Deactivate();
							sg2.Deactivate();

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
				else error = Util::FormatString(L"couldn't add a main event");
			}
			catch (...) {}

			LogErr(L"Failed to add a new event subgroup; %s", error.c_str());

			return nullptr;
		}

	private:
		Data_ThS m_Data;
	};
}