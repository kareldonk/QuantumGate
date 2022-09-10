// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Common\Util.h"
#include "Concurrency\EventGroup.h"
#include "Common\DiffTimer.h"

#include <thread>
#include <random>

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Concurrency;
using namespace QuantumGate::Implementation;

void WaitFunc5s(Event* event) noexcept; // Implemented in EventTests.cpp

namespace UnitTests
{
	TEST_CLASS(EventGroupTests)
	{
	public:
		TEST_METHOD(Basic)
		{
			Event event1;
			Event event2;

			EventGroup eventgroup;
			Assert::AreEqual(true, eventgroup.Initialize());
			
			const auto result = eventgroup.Wait(1s);
			Assert::AreEqual(false, result.Waited);
			Assert::AreEqual(false, result.HadEvent);

			Assert::AreEqual(true, eventgroup.AddEvent(event1));
			Assert::AreEqual(true, eventgroup.HasEvent(event1));
			Assert::AreEqual(true, eventgroup.AddEvent(event2));
			Assert::AreEqual(true, eventgroup.HasEvent(event2));
			
			const auto result2 = eventgroup.Wait(1s);
			Assert::AreEqual(true, result2.Waited);
			Assert::AreEqual(false, result2.HadEvent);

			DiffTimer<1> timer;
			auto measurement = timer.GetNewMeasurement(1);
			measurement.Start();

			// This thread will set the event within 5 seconds
			auto thread = std::thread(&WaitFunc5s, &event1);

			const auto result3 = eventgroup.Wait(10s);

			measurement.End();

			thread.join();

			Assert::AreEqual(true, result3.Waited);
			Assert::AreEqual(true, result3.HadEvent);
			Assert::AreEqual(true, measurement.GetElapsedTime() >= 5s);

			eventgroup.RemoveEvent(event1);
			Assert::AreEqual(false, eventgroup.HasEvent(event1));

			const auto result4 = eventgroup.Wait(1s);
			Assert::AreEqual(true, result4.Waited);
			Assert::AreEqual(false, result4.HadEvent);

			Event event3;
			Assert::AreEqual(true, event3.Set());
			Assert::AreEqual(true, eventgroup.AddEvent(event3));

			const auto result5 = eventgroup.Wait(0s);
			Assert::AreEqual(true, result5.Waited);
			Assert::AreEqual(true, result5.HadEvent);

			eventgroup.Deinitialize();
		}

		TEST_METHOD(MultipleEvents)
		{
			for (int x = 0; x < 10; ++x)
			{
				EventGroup eventgroup;
				Assert::AreEqual(true, eventgroup.Initialize());

				std::vector<Event> events(EventGroup::MaximumNumberOfUserEvents);
				for (const auto& event : events)
				{
					Assert::AreEqual(true, eventgroup.AddEvent(event));
				}

				std::random_device dev;
				std::mt19937_64 rng(dev());

				// Randomly set events and remove them until
				// they have all been removed
				while (!events.empty())
				{
					const auto result = eventgroup.Wait(0s);
					Assert::AreEqual(true, result.Waited);
					Assert::AreEqual(false, result.HadEvent);

					std::uniform_int_distribution<std::size_t> dist(0, events.size() - 1);
					const auto idx = dist(rng);
					events[idx].Set();

					const auto result2 = eventgroup.Wait(1s);
					Assert::AreEqual(true, result2.Waited);
					Assert::AreEqual(true, result2.HadEvent);

					eventgroup.RemoveEvent(events[idx]);
					events.erase(events.begin() + idx);
				}

				eventgroup.Deinitialize();
			}
		}

		TEST_METHOD(MaximumEvents)
		{
			EventGroup eventgroup;
			Assert::AreEqual(true, eventgroup.Initialize());

			std::vector<Event> events(EventGroup::MaximumNumberOfUserEvents);
			for (const auto& event : events)
			{
				Assert::AreEqual(true, eventgroup.AddEvent(event));
			}
			
			Event levent;
			// Should return false because the group is maxed out
			Assert::AreEqual(false, eventgroup.AddEvent(levent));

			eventgroup.RemoveEvent(events[0]);
			Assert::AreEqual(false, eventgroup.HasEvent(events[0]));
			events.erase(events.begin());

			Assert::AreEqual(true, eventgroup.AddEvent(levent));
			Assert::AreEqual(true, eventgroup.HasEvent(levent));
			
			eventgroup.Deinitialize();
		}
	};
}