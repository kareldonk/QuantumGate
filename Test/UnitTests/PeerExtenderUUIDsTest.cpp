// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Core\Peer\PeerExtenderUUIDs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Core::Peer;

namespace UnitTests
{
	TEST_CLASS(PeerExtenderUUIDsTests)
	{
	public:
		TEST_METHOD(Set)
		{
			// Empty vector
			{
				Vector<ExtenderUUID> uuids;

				ExtenderUUIDs extuuids;
				const auto retval = extuuids.Set(std::move(uuids));

				Assert::AreEqual(true, retval);
				Assert::AreEqual(true, extuuids.Current().size() == 0);
				Assert::AreEqual(false, extuuids.HasExtender(QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")));
			}

			// Single element
			{
				Vector<ExtenderUUID> uuids
				{
					QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")
				};

				ExtenderUUIDs extuuids;
				const auto retval = extuuids.Set(std::move(uuids));

				Assert::AreEqual(true, retval);
				Assert::AreEqual(true, extuuids.Current().size() == 1);
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")));
			}

			// Multiple elements
			{
				Vector<ExtenderUUID> uuids
				{
					QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467"),
					QuantumGate::UUID(L"0e511a53-c886-a9b5-e63c-cd5552e45aa0"),
					QuantumGate::UUID(L"720d1977-c186-a981-4691-19ea9dcff055"),
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
					QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524")
				};

				ExtenderUUIDs extuuids;
				const auto retval = extuuids.Set(std::move(uuids));

				Assert::AreEqual(true, retval);
				Assert::AreEqual(true, extuuids.Current().size() == 5);

				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"0e511a53-c886-a9b5-e63c-cd5552e45aa0")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"720d1977-c186-a981-4691-19ea9dcff055")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524")));

				Vector<ExtenderUUID> uuids2
				{
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
					QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524")
				};

				const auto retval2 = extuuids.Set(std::move(uuids2));

				Assert::AreEqual(true, retval2);
				Assert::AreEqual(true, extuuids.Current().size() == 2);

				Assert::AreEqual(false, extuuids.HasExtender(QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524")));
			}

			// Duplicate elements
			{
				Vector<ExtenderUUID> uuids
				{
					QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467"),
					QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")
				};

				ExtenderUUIDs extuuids;
				const auto retval = extuuids.Set(std::move(uuids));

				Assert::AreEqual(false, retval);
			}

			// Duplicate elements
			{
				Vector<ExtenderUUID> uuids
				{
					QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467"),
					QuantumGate::UUID(L"0e511a53-c886-a9b5-e63c-cd5552e45aa0"),
					QuantumGate::UUID(L"720d1977-c186-a981-4691-19ea9dcff055"),
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
					QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524"),
					QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")
				};

				ExtenderUUIDs extuuids;
				const auto retval = extuuids.Set(std::move(uuids));

				Assert::AreEqual(false, retval);
			}
		}

		TEST_METHOD(Update)
		{
			// Empty vector
			{
				Vector<ExtenderUUID> uuids;

				ExtenderUUIDs extuuids;
				const auto retval = extuuids.Update(std::move(uuids));

				Assert::AreEqual(true, retval.Succeeded());
				Assert::AreEqual(true, extuuids.Current().size() == 0);
				Assert::AreEqual(false, extuuids.HasExtender(QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")));
			}

			// Single element
			{
				Vector<ExtenderUUID> uuids
				{
					QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")
				};

				ExtenderUUIDs extuuids;
				const auto retval = extuuids.Update(std::move(uuids));

				Assert::AreEqual(true, retval.Succeeded());
				Assert::AreEqual(true, extuuids.Current().size() == 1);
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")));
			}

			// Multiple elements
			{
				Vector<ExtenderUUID> uuids
				{
					QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467"),
					QuantumGate::UUID(L"0e511a53-c886-a9b5-e63c-cd5552e45aa0"),
					QuantumGate::UUID(L"720d1977-c186-a981-4691-19ea9dcff055"),
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
					QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524")
				};

				ExtenderUUIDs extuuids;
				const auto retval = extuuids.Update(std::move(uuids));

				Assert::AreEqual(true, retval.Succeeded());
				Assert::AreEqual(true, extuuids.Current().size() == 5);

				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"0e511a53-c886-a9b5-e63c-cd5552e45aa0")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"720d1977-c186-a981-4691-19ea9dcff055")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524")));

				// Remove 3 and add 1
				Vector<ExtenderUUID> uuids2
				{
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
					QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524"),
					QuantumGate::UUID(L"3651d05f-eacb-09ea-be11-e50bb1fce0e4")
				};

				const auto retval2 = extuuids.Update(std::move(uuids2));

				Assert::AreEqual(true, retval2.Succeeded());
				Assert::AreEqual(true, retval2.GetValue().first.size() == 1);
				Assert::AreEqual(true, std::find(retval2.GetValue().first.begin(),
												 retval2.GetValue().first.end(),
												 QuantumGate::UUID(L"3651d05f-eacb-09ea-be11-e50bb1fce0e4")) !=
								 retval2.GetValue().first.end());

				Assert::AreEqual(true, retval2.GetValue().second.size() == 3);
				Assert::AreEqual(true, std::find(retval2.GetValue().second.begin(),
												 retval2.GetValue().second.end(),
												 QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")) !=
								 retval2.GetValue().second.end());
				Assert::AreEqual(true, std::find(retval2.GetValue().second.begin(),
												 retval2.GetValue().second.end(),
												 QuantumGate::UUID(L"0e511a53-c886-a9b5-e63c-cd5552e45aa0")) !=
								 retval2.GetValue().second.end());
				Assert::AreEqual(true, std::find(retval2.GetValue().second.begin(),
												 retval2.GetValue().second.end(),
												 QuantumGate::UUID(L"720d1977-c186-a981-4691-19ea9dcff055")) !=
								 retval2.GetValue().second.end());

				Assert::AreEqual(true, extuuids.Current().size() == 3);
				Assert::AreEqual(false, extuuids.HasExtender(QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524")));
				Assert::AreEqual(true, extuuids.HasExtender(QuantumGate::UUID(L"3651d05f-eacb-09ea-be11-e50bb1fce0e4")));
			}

			// Duplicate elements
			{
				Vector<ExtenderUUID> uuids
				{
					QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467"),
					QuantumGate::UUID(L"0e511a53-c886-a9b5-e63c-cd5552e45aa0"),
					QuantumGate::UUID(L"720d1977-c186-a981-4691-19ea9dcff055"),
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
					QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524")
				};

				ExtenderUUIDs extuuids;
				const auto retval = extuuids.Update(std::move(uuids));

				Assert::AreEqual(true, retval.Succeeded());
				Assert::AreEqual(true, extuuids.Current().size() == 5);

				Vector<ExtenderUUID> uuids2
				{
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
				};

				const auto retval2 = extuuids.Update(std::move(uuids2));

				Assert::AreEqual(false, retval2.Succeeded());

				Vector<ExtenderUUID> uuids3
				{
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
					QuantumGate::UUID(L"d5375501-3b71-d9cc-0689-1aab49b4f524"),
					QuantumGate::UUID(L"3651d05f-eacb-09ea-be11-e50bb1fce0e4"),
					QuantumGate::UUID(L"23043d05-c3d7-89b8-be93-04db663d1d42"),
				};

				const auto retval3 = extuuids.Update(std::move(uuids3));

				Assert::AreEqual(false, retval3.Succeeded());
			}
		}

		TEST_METHOD(Copy)
		{
			Vector<ExtenderUUID> uuids
			{
				QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467"),
				QuantumGate::UUID(L"0e511a53-c886-a9b5-e63c-cd5552e45aa0"),
				QuantumGate::UUID(L"720d1977-c186-a981-4691-19ea9dcff055")
			};

			ExtenderUUIDs extuuids;
			const auto retval = extuuids.Set(std::move(uuids));

			Assert::AreEqual(true, retval);
			Assert::AreEqual(true, extuuids.Current().size() == 3);

			ExtenderUUIDs extuuids2;
			Assert::AreEqual(true, extuuids2.Current().size() == 0);

			const auto retval2 = extuuids2.Copy(extuuids);
			Assert::AreEqual(true, retval2);
			Assert::AreEqual(true, extuuids2.Current().size() == 3);

			Assert::AreEqual(true, extuuids2.HasExtender(QuantumGate::UUID(L"0db99db5-ed96-49ff-46d4-75dcf455b467")));
			Assert::AreEqual(true, extuuids2.HasExtender(QuantumGate::UUID(L"0e511a53-c886-a9b5-e63c-cd5552e45aa0")));
			Assert::AreEqual(true, extuuids2.HasExtender(QuantumGate::UUID(L"720d1977-c186-a981-4691-19ea9dcff055")));

			ExtenderUUIDs extuuids3;
			const auto retval3 = extuuids2.Copy(extuuids3);
			Assert::AreEqual(true, retval3);
			Assert::AreEqual(true, extuuids2.Current().size() == 0);
		}
	};
}