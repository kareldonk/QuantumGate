// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Concurrency\EventComposite.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Concurrency;
using namespace QuantumGate::Implementation;

namespace UnitTests
{
	TEST_CLASS(EventCompositeTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Constructor
			EventComposite<2, EventCompositeOperatorType::OR> ec;
			Assert::AreEqual(true, ec.GetOperatorType() == EventCompositeOperatorType::OR);
			Assert::AreEqual(true, ec.GetHandle() != nullptr);
			Assert::AreEqual(false, ec.IsSet());
			Assert::AreEqual(true, ec.GetSubEvent(0).operator bool());
			Assert::AreEqual(true, ec.GetSubEvent(1).operator bool());

			// Move constructor
			EventComposite<2, EventCompositeOperatorType::OR> ec2(std::move(ec));
			Assert::AreEqual(true, ec2.GetHandle() != nullptr);
			Assert::AreEqual(false, ec2.IsSet());
			Assert::AreEqual(true, ec2.GetSubEvent(0).operator bool());
			Assert::AreEqual(true, ec2.GetSubEvent(1).operator bool());
			Assert::AreEqual(true, ec.GetHandle() == nullptr);
			Assert::AreEqual(false, ec.IsSet());
			Assert::AreEqual(false, ec.GetSubEvent(0).operator bool());
			Assert::AreEqual(false, ec.GetSubEvent(1).operator bool());

			Assert::AreEqual(true, ec2.GetSubEvent(0).Set());
			Assert::AreEqual(true, ec2.IsSet());

			// Move assignment
			auto ec3 = std::move(ec2);
			Assert::AreEqual(true, ec3.GetHandle() != nullptr);
			Assert::AreEqual(true, ec3.IsSet());
			Assert::AreEqual(true, ec3.GetSubEvent(0).operator bool());
			Assert::AreEqual(true, ec3.GetSubEvent(1).operator bool());
			Assert::AreEqual(true, ec2.GetHandle() == nullptr);
			Assert::AreEqual(false, ec2.IsSet());
			Assert::AreEqual(false, ec2.GetSubEvent(0).operator bool());
			Assert::AreEqual(false, ec2.GetSubEvent(1).operator bool());
		}

		TEST_METHOD(OperatorTypeAND)
		{
			EventComposite<2, EventCompositeOperatorType::AND> ec;
			Assert::AreEqual(true, ec.GetOperatorType() == EventCompositeOperatorType::AND);
			Assert::AreEqual(true, ec.GetHandle() != nullptr);
			Assert::AreEqual(false, ec.IsSet());
			Assert::AreEqual(true, ec.GetSubEvent(0).operator bool());
			Assert::AreEqual(true, ec.GetSubEvent(1).operator bool());

			// Set first subevent; composite should test false
			Assert::AreEqual(true, ec.GetSubEvent(0).Set());
			Assert::AreEqual(true, ec.GetSubEvent(0).IsSet());
			Assert::AreEqual(false, ec.IsSet());

			// Set second subevent; composite should test true
			Assert::AreEqual(true, ec.GetSubEvent(1).Set());
			Assert::AreEqual(true, ec.GetSubEvent(1).IsSet());
			Assert::AreEqual(true, ec.IsSet());

			// Reset second subevent; composite should test false
			Assert::AreEqual(true, ec.GetSubEvent(1).Reset());
			Assert::AreEqual(false, ec.GetSubEvent(1).IsSet());
			Assert::AreEqual(false, ec.IsSet());

			// Set second subevent; composite should test true again
			Assert::AreEqual(true, ec.GetSubEvent(1).Set());
			Assert::AreEqual(true, ec.GetSubEvent(1).IsSet());
			Assert::AreEqual(true, ec.IsSet());

			// Reset
			Assert::AreEqual(true, ec.Reset());
			Assert::AreEqual(false, ec.IsSet());
			Assert::AreEqual(false, ec.GetSubEvent(0).IsSet());
			Assert::AreEqual(false, ec.GetSubEvent(1).IsSet());

			// Set
			Assert::AreEqual(true, ec.Set());
			Assert::AreEqual(true, ec.IsSet());
			Assert::AreEqual(true, ec.GetSubEvent(0).IsSet());
			Assert::AreEqual(true, ec.GetSubEvent(1).IsSet());
		}

		TEST_METHOD(OperatorTypeOR)
		{
			EventComposite<2, EventCompositeOperatorType::OR> ec;
			Assert::AreEqual(true, ec.GetOperatorType() == EventCompositeOperatorType::OR);
			Assert::AreEqual(true, ec.GetHandle() != nullptr);
			Assert::AreEqual(false, ec.IsSet());
			Assert::AreEqual(true, ec.GetSubEvent(0).operator bool());
			Assert::AreEqual(true, ec.GetSubEvent(1).operator bool());

			// Set first subevent; composite should test true
			Assert::AreEqual(true, ec.GetSubEvent(0).Set());
			Assert::AreEqual(true, ec.GetSubEvent(0).IsSet());
			Assert::AreEqual(true, ec.IsSet());

			// Set second subevent; composite should test true
			Assert::AreEqual(true, ec.GetSubEvent(1).Set());
			Assert::AreEqual(true, ec.GetSubEvent(1).IsSet());
			Assert::AreEqual(true, ec.IsSet());

			// Reset second subevent; composite should test true
			Assert::AreEqual(true, ec.GetSubEvent(1).Reset());
			Assert::AreEqual(false, ec.GetSubEvent(1).IsSet());
			Assert::AreEqual(true, ec.IsSet());

			// Reset first subevent; composite should test false
			Assert::AreEqual(true, ec.GetSubEvent(0).Reset());
			Assert::AreEqual(false, ec.GetSubEvent(0).IsSet());
			Assert::AreEqual(false, ec.IsSet());

			Assert::AreEqual(true, ec.GetSubEvent(0).Set());
			Assert::AreEqual(true, ec.GetSubEvent(1).Set());
			Assert::AreEqual(true, ec.IsSet());

			// Reset
			Assert::AreEqual(true, ec.Reset());
			Assert::AreEqual(false, ec.IsSet());
			Assert::AreEqual(false, ec.GetSubEvent(0).IsSet());
			Assert::AreEqual(false, ec.GetSubEvent(1).IsSet());
		}

		TEST_METHOD(SubEvent)
		{
			EventComposite<2, EventCompositeOperatorType::OR> ec;

			// Constructor
			EventComposite<2, EventCompositeOperatorType::OR>::SubEventType sev;
			Assert::AreEqual(false, sev.operator bool());

			// Move constructor
			EventComposite<2, EventCompositeOperatorType::OR>::SubEventType sev2(ec.GetSubEvent(0));
			Assert::AreEqual(true, sev2.operator bool());
			Assert::AreEqual(false, sev2.IsSet());

			// Move assignment
			EventComposite<2, EventCompositeOperatorType::OR>::SubEventType sev3 = std::move(sev2);
			Assert::AreEqual(true, sev3.operator bool());
			Assert::AreEqual(false, sev3.IsSet());
			Assert::AreEqual(false, sev2.operator bool());

			sev3 = ec.GetSubEvent(0);
			Assert::AreEqual(true, sev3.operator bool());
			Assert::AreEqual(false, sev3.IsSet());

			// Set
			sev3.Set();

			Assert::AreEqual(true, sev3.IsSet());
			Assert::AreEqual(true, ec.GetSubEvent(0).IsSet());
			Assert::AreEqual(true, ec.IsSet());

			// Move assignment
			EventComposite<2, EventCompositeOperatorType::OR>::SubEventType sev4 = std::move(sev3);
			Assert::AreEqual(true, sev4.operator bool());
			Assert::AreEqual(true, sev4.IsSet());
			Assert::AreEqual(false, sev3.operator bool());

			// Reset
			sev4.Reset();

			Assert::AreEqual(false, sev4.IsSet());
			Assert::AreEqual(false, ec.GetSubEvent(0).IsSet());
			Assert::AreEqual(false, ec.IsSet());

			// Release
			sev4.Release();
			Assert::AreEqual(false, sev4.operator bool());
		}

		TEST_METHOD(SubEventConst)
		{
			const EventComposite<3, EventCompositeOperatorType::OR> ec;
			auto sev = ec.GetSubEvent(0);
			Assert::AreEqual(true, sev.operator bool());
			Assert::AreEqual(false, sev.IsSet());

			EventComposite<3, EventCompositeOperatorType::OR>::SubEventConstType sev2(std::move(sev));
			auto sev3 = std::move(sev2);
			sev3.Release();
			Assert::AreEqual(false, sev3.operator bool());
		}
	};
}