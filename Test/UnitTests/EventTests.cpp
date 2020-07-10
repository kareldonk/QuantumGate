// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Concurrency\Event.h"
#include "Common\DiffTimer.h"

#include <thread>

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Concurrency;
using namespace QuantumGate::Implementation;

void WaitFunc1s(Event* event) noexcept
{
	std::this_thread::sleep_for(1s);

	event->Set();
}

void WaitFunc5s(Event* event) noexcept
{
	std::this_thread::sleep_for(5s);

	event->Set();
}

namespace UnitTests
{
	TEST_CLASS(EventTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Constructor
			Event event;
			Assert::AreEqual(true, event.GetHandle() != nullptr);
			Assert::AreEqual(false, event.IsSet());

			// Move constructor
			Event event2(std::move(event));
			Assert::AreEqual(true, event2.GetHandle() != nullptr);
			Assert::AreEqual(false, event2.IsSet());
			Assert::AreEqual(true, event.GetHandle() == nullptr);
			Assert::AreEqual(false, event.IsSet());

			Assert::AreEqual(true, event2.Set());
			Assert::AreEqual(true, event2.IsSet());

			// Move assignment
			auto event3 = std::move(event2);
			Assert::AreEqual(true, event3.GetHandle() != nullptr);
			Assert::AreEqual(true, event3.IsSet());
			Assert::AreEqual(true, event2.GetHandle() == nullptr);
			Assert::AreEqual(false, event2.IsSet());

			// Constructor for event handle
			const auto handle = ::WSACreateEvent();
			Event eventh(handle);
			Assert::AreEqual(true, eventh.GetHandle() != nullptr);
			Assert::AreEqual(false, eventh.IsSet());
			Assert::AreEqual(true, eventh.Set());
			Assert::AreEqual(true, eventh.IsSet());
			Assert::AreEqual(true, eventh.Reset());
			Assert::AreEqual(false, eventh.IsSet());
			Assert::AreEqual(true, eventh.Set());
			Assert::AreEqual(true, eventh.IsSet());

			// Release
			eventh.Release();
			Assert::AreEqual(true, eventh.GetHandle() == nullptr);
			Assert::AreEqual(false, eventh.IsSet());
		}

		TEST_METHOD(Wait)
		{
			Event event;
			Assert::AreEqual(true, event.GetHandle() != nullptr);
			Assert::AreEqual(false, event.IsSet());

			// This thread will set the event within 1 second
			auto thread = std::thread(&WaitFunc1s, &event);

			// Wait for event
			Assert::AreEqual(true, event.Wait(10s));

			thread.join();

			// Should be set
			Assert::AreEqual(true, event.IsSet());

			// Reset event
			Assert::AreEqual(true, event.Reset());
			Assert::AreEqual(false, event.IsSet());

			DiffTimer<1> timer;
			auto measurement = timer.GetNewMeasurement(1);
			measurement.Start();

			// This thread will set the event within 5 seconds
			auto thread2 = std::thread(&WaitFunc5s, &event);

			// Wait for event
			Assert::AreEqual(false, event.Wait(2s));

			// Should not yet be set
			Assert::AreEqual(false, event.IsSet());

			// Wait for event
			Assert::AreEqual(true, event.Wait(10s));

			measurement.End();

			thread2.join();

			// Should be set
			Assert::AreEqual(true, event.IsSet());
			Assert::AreEqual(true, measurement.GetElapsedTime() >= 5s);
		}

		TEST_METHOD(WaitInfinite)
		{
			Event event;
			Assert::AreEqual(true, event.GetHandle() != nullptr);
			Assert::AreEqual(false, event.IsSet());

			DiffTimer<1> timer;
			auto measurement = timer.GetNewMeasurement(1);
			measurement.Start();

			// This thread will set the event within 5 seconds
			auto thread = std::thread(&WaitFunc5s, &event);

			// Wait for event
			event.Wait();
			
			measurement.End();

			thread.join();

			// Should be set
			Assert::AreEqual(true, event.IsSet());
			Assert::AreEqual(true, measurement.GetElapsedTime() >= 5s);
		}
	};
}