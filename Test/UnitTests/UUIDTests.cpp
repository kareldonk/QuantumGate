// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "CppUnitTest.h"
#include "Common\UUID.h"
#include "Common\Util.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(UUIDTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Test invalid UUIDs
			{
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L""); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"123456"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"3df5b8e4-50d2"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a- b9db-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9db-838d32ba892e908f"); });

				// Too short
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4-b30a-b9db-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30-b9db-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9d-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9db-838-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9db-838d-32ba892e908"); });

				// Too long
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d1-b30a-b9db-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a1-b9db-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9db1-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9db-838d1-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9db-838d-32ba892e908f1"); });

				// Invalid characters
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbz4d-b30a-b9db-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30n-b9db-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9dm-838d-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9db-838o-32ba892e908f"); });
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9db-838d-32ba892p908f"); });

				// Invalid version
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-c3db-838d-32ba892e908f"); });

				// Invalid variant
				Assert::ExpectException<std::invalid_argument>([] { QuantumGate::UUID(L"e14dbc4d-b30a-b9db-ec8d-32ba892e908f"); });
			}

			// Default constructor
			QuantumGate::UUID uuid;
			Assert::AreEqual(true, uuid.GetString() == L"00000000-0000-0000-0000-000000000000");
			Assert::AreEqual(false, uuid.IsValid());
			Assert::AreEqual(true, uuid.GetType() == QuantumGate::UUID::Type::Unknown);
			Assert::AreEqual(true, uuid.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::None);

			// String constructor
			QuantumGate::UUID uuid2(L"081c5330-5b28-9920-cb1d-f24966b127da");
			Assert::AreEqual(true, uuid2.GetString() == L"081c5330-5b28-9920-cb1d-f24966b127da");
			Assert::AreEqual(false, uuid2.GetType() == QuantumGate::UUID::Type::Unknown);
			Assert::AreEqual(false, uuid2.GetType() == QuantumGate::UUID::Type::Extender);
			Assert::AreEqual(true, uuid2.GetType() == QuantumGate::UUID::Type::Peer);
			Assert::AreEqual(true, uuid2.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519);

			// Integer constructor
			QuantumGate::UUID uuid2a(0x1a2015f1, 0x812b, 0x0927, 0x4b6173950597ca6d);
			Assert::AreEqual(true, uuid2a.GetString() == L"1a2015f1-812b-0927-4b61-73950597ca6d");
			Assert::AreEqual(true, uuid2a.GetType() == QuantumGate::UUID::Type::Peer);
			Assert::AreEqual(true, uuid2a.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519);

			// String constructor
			QuantumGate::UUID uuid2b(L"af61a26e-be52-b98a-662f-4f620d9558e7");
			Assert::AreEqual(true, uuid2b.GetString() == L"af61a26e-be52-b98a-662f-4f620d9558e7");
			Assert::AreEqual(true, uuid2b.GetType() == QuantumGate::UUID::Type::Extender);
			Assert::AreEqual(true, uuid2b.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::None);

			// Equal to empty
			Assert::AreEqual(true, uuid == QuantumGate::UUID());

			// Not equal
			Assert::AreEqual(true, uuid != uuid2a);
			Assert::AreEqual(true, uuid2 != uuid2b);

			// TryParse for String
			Assert::AreEqual(false, QuantumGate::UUID::TryParse(L"081c5330-5b28-920-cb1d-f24966b127da", uuid));
			Assert::AreEqual(true, QuantumGate::UUID::TryParse(L"081c5330-5b28-9920-cb1d-f24966b127da", uuid));

			// Equal to other
			Assert::AreEqual(true, uuid == uuid2);

			// Copy constructor
			QuantumGate::UUID uuid3(uuid2);
			Assert::AreEqual(true, uuid3 == uuid2);
			Assert::AreEqual(true, uuid3.GetString() == L"081c5330-5b28-9920-cb1d-f24966b127da");

			// Copy assignment
			QuantumGate::UUID uuid4;
			uuid4 = uuid2;
			Assert::AreEqual(true, uuid4 == uuid2);
			Assert::AreEqual(true, uuid4.GetString() == L"081c5330-5b28-9920-cb1d-f24966b127da");

			// Move constructor
			QuantumGate::UUID uuid5(std::move(uuid2));
			Assert::AreEqual(true, uuid5 == uuid3);
			Assert::AreEqual(true, uuid5.GetString() == L"081c5330-5b28-9920-cb1d-f24966b127da");

			// Move assignment
			QuantumGate::UUID uuid6;
			uuid6 = std::move(uuid5);
			Assert::AreEqual(true, uuid6 == uuid3);
			Assert::AreEqual(true, uuid6.GetString() == L"081c5330-5b28-9920-cb1d-f24966b127da");

			// Create extender UUID
			const auto[success, uuid7, keys] = QuantumGate::UUID::Create(QuantumGate::UUID::Type::Extender,
																		 QuantumGate::UUID::SignAlgorithm::None);
			Assert::AreEqual(true, success);
			Assert::AreEqual(true, uuid7.IsValid());
			Assert::AreEqual(false, keys.has_value());
			Assert::AreEqual(true, uuid7.GetType() == QuantumGate::UUID::Type::Extender);
			Assert::AreEqual(true, uuid7.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::None);
		}

		TEST_METHOD(Constexpr)
		{
			constexpr QuantumGate::UUID uuid(0x081c5330, 0x5b28, 0x9920, 0xcb1df24966b127da);
			constexpr auto type = uuid.GetType();
			constexpr auto algorithm = uuid.GetSignAlgorithm();
			constexpr auto valid = uuid.IsValid();
			Assert::AreEqual(true, uuid.GetString() == L"081c5330-5b28-9920-cb1d-f24966b127da");
			Assert::AreEqual(false, type == QuantumGate::UUID::Type::Unknown);
			Assert::AreEqual(false, type == QuantumGate::UUID::Type::Extender);
			Assert::AreEqual(true, type == QuantumGate::UUID::Type::Peer);
			Assert::AreEqual(true, uuid.GetType() == QuantumGate::UUID::Type::Peer);
			Assert::AreEqual(true, algorithm == QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519);
			Assert::AreEqual(true, uuid.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519);
			Assert::AreEqual(true, valid);

			constexpr QuantumGate::UUID uuid2(0x1a2015f1, 0x812b, 0x0927, 0x4b6173950597ca6d);
			Assert::AreEqual(true, uuid2.GetString() == L"1a2015f1-812b-0927-4b61-73950597ca6d");
			Assert::AreEqual(true, uuid2.GetType() == QuantumGate::UUID::Type::Peer);
			Assert::AreEqual(true, uuid2.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519);

			constexpr QuantumGate::UUID uuid3(0xaf61a26e, 0xbe52, 0xb98a, 0x662f4f620d9558e7);
			Assert::AreEqual(true, uuid3.GetString() == L"af61a26e-be52-b98a-662f-4f620d9558e7");
			Assert::AreEqual(true, uuid3.GetType() == QuantumGate::UUID::Type::Extender);
			Assert::AreEqual(true, uuid3.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::None);

			static_assert(uuid != uuid2, "Should not be equal");
			static_assert(!(uuid < uuid2), "Should not be smaller");
			static_assert(uuid2 < uuid3, "Should be smaller");

			Assert::AreEqual(true, uuid != uuid2);
			Assert::AreEqual(false, uuid < uuid2);
			Assert::AreEqual(true, uuid2 < uuid3);
		}

		TEST_METHOD(Verify)
		{
			{
				QuantumGate::UUID uuid(L"1a2015f1-812b-0927-4b61-73950597ca6d");
				String pub_key_b64 = L"AMNkUKupuRiCzdi2iYEegJqG6yPl+8bGYZFFb+lPdis=";
				const auto buffer = Util::FromBase64(pub_key_b64);
				ProtectedBuffer pub_key = BufferView(*buffer);
				Assert::AreEqual(true, uuid.Verify(pub_key));
				Assert::AreEqual(true, uuid.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519);
			}

			{
				QuantumGate::UUID uuid(L"03e131bc-694b-f958-1318-89fe960dc7e3");
				String pub_key_b64 = L"iUegJ8xbaefVRrjrsVpRp9ysYulVJo1ispNt0WDvrmqg+hzQQp5IXv9VRjnCAMQYqDH9eXAEwNqA";
				const auto buffer = Util::FromBase64(pub_key_b64);
				ProtectedBuffer pub_key = BufferView(*buffer);
				Assert::AreEqual(true, uuid.Verify(pub_key));
				Assert::AreEqual(true, uuid.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::EDDSA_ED448);
			}

			// Create peer UUID
			{
				const auto[success, uuid, keys] = QuantumGate::UUID::Create(QuantumGate::UUID::Type::Peer,
																			QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519);
				Assert::AreEqual(true, success);
				Assert::AreEqual(true, uuid.IsValid());
				Assert::AreEqual(true, keys.has_value());
				Assert::AreEqual(true, !keys->PrivateKey.IsEmpty());
				Assert::AreEqual(true, !keys->PublicKey.IsEmpty());
				Assert::AreEqual(true, uuid.Verify(keys->PublicKey));
				Assert::AreEqual(true, uuid.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519);
			}

			// Create peer UUID
			{
				const auto[success, uuid, keys] = QuantumGate::UUID::Create(QuantumGate::UUID::Type::Peer,
																			QuantumGate::UUID::SignAlgorithm::EDDSA_ED448);
				Assert::AreEqual(true, success);
				Assert::AreEqual(true, uuid.IsValid());
				Assert::AreEqual(true, keys.has_value());
				Assert::AreEqual(true, !keys->PrivateKey.IsEmpty());
				Assert::AreEqual(true, !keys->PublicKey.IsEmpty());
				Assert::AreEqual(true, uuid.Verify(keys->PublicKey));
				Assert::AreEqual(true, uuid.GetSignAlgorithm() == QuantumGate::UUID::SignAlgorithm::EDDSA_ED448);
			}

			// UUID and public key mismatch
			{
				QuantumGate::UUID uuid(L"081c5330-5b28-9920-cb1d-f24966b127da");
				String pub_key_b64 = L"AMNkUKupuRiCzdi2iYEegJqG6yPl+8bGYZFFb+lPdis=";
				const auto buffer = Util::FromBase64(pub_key_b64);
				ProtectedBuffer pub_key = BufferView(*buffer);
				Assert::AreEqual(false, uuid.Verify(pub_key));
			}

			// Empty public key buffer
			{
				QuantumGate::UUID uuid(L"34249c3e-120b-c939-8bea-578b2b12104b");
				ProtectedBuffer pub_key;
				Assert::AreEqual(false, uuid.Verify(pub_key));
			}
		}
	};
}