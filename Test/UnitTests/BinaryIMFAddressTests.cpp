// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network\BinaryIMFAddress.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace QuantumGate::Implementation::Network;

namespace UnitTests
{
	constexpr bool CheckConstructor() noexcept
	{
		BinaryIMFAddress addr;
		auto success = (addr.AddressFamily == BinaryIMFAddress::Family::Unspecified);
		success &= (addr.GetChars() == nullptr);
		success &= (addr.GetSize() == 0);
		success &= (addr.GetStringView() == L"");
		success &= (addr.GetStringView().size() == 0);
		return success;
	}

	constexpr bool CheckCopyConstructor() noexcept
	{
		BinaryIMFAddress addr(BinaryIMFAddress::Family::IMF, L"test@example.com");
		auto success = (addr.AddressFamily == BinaryIMFAddress::Family::IMF);
		success &= (addr.GetChars() != nullptr);
		success &= (StringView(addr.GetChars()) == StringView(L"test@example.com"));
		success &= (addr.GetSize() == 17);
		success &= (addr.GetStringView() == L"test@example.com");
		success &= (addr.GetStringView().size() == 16);

		BinaryIMFAddress addr2(addr);
		success &= (addr2.AddressFamily == BinaryIMFAddress::Family::IMF);
		success &= (addr2.GetChars() != nullptr);
		success &= (StringView(addr2.GetChars()) == StringView(L"test@example.com"));
		success &= (addr2.GetSize() == 17);

		success &= (addr == addr2);

		return success;
	}

	constexpr bool CheckCopyAssignment() noexcept
	{
		BinaryIMFAddress addr(BinaryIMFAddress::Family::IMF, L"test@example.com");
		auto success = (addr.AddressFamily == BinaryIMFAddress::Family::IMF);
		success &= (addr.GetChars() != nullptr);
		success &= (StringView(addr.GetChars()) == StringView(L"test@example.com"));
		success &= (addr.GetSize() == 17);

		BinaryIMFAddress addr2 = addr;
		success &= (addr2.AddressFamily == BinaryIMFAddress::Family::IMF);
		success &= (addr2.GetChars() != nullptr);
		success &= (StringView(addr2.GetChars()) == StringView(L"test@example.com"));
		success &= (addr2.GetSize() == 17);
		success &= (addr2.GetStringView() == L"test@example.com");
		success &= (addr2.GetStringView().size() == 16);

		success &= (addr == addr2);

		return success;
	}

	constexpr bool CheckMoveConstructor() noexcept
	{
		BinaryIMFAddress addr(BinaryIMFAddress::Family::IMF, L"test@example.com");
		auto success = (addr.AddressFamily == BinaryIMFAddress::Family::IMF);
		success &= (addr.GetChars() != nullptr);
		success &= (StringView(addr.GetChars()) == StringView(L"test@example.com"));
		success &= (addr.GetSize() == 17);

		BinaryIMFAddress addr2(std::move(addr));
		success &= (addr2.AddressFamily == BinaryIMFAddress::Family::IMF);
		success &= (addr2.GetChars() != nullptr);
		success &= (StringView(addr2.GetChars()) == StringView(L"test@example.com"));
		success &= (addr2.GetSize() == 17);
		success &= (addr2.GetStringView() == L"test@example.com");
		success &= (addr2.GetStringView().size() == 16);

		success &= (addr.AddressFamily == BinaryIMFAddress::Family::Unspecified);
		success &= (addr.GetChars() == nullptr);
		success &= (addr.GetSize() == 0);
		success &= (addr.GetStringView() == L"");
		success &= (addr.GetStringView().size() == 0);

		success &= (addr != addr2);

		return success;
	}

	constexpr bool CheckMoveAssignment() noexcept
	{
		BinaryIMFAddress addr(BinaryIMFAddress::Family::IMF, L"test@example.com");
		auto success = (addr.AddressFamily == BinaryIMFAddress::Family::IMF);
		success &= (addr.GetChars() != nullptr);
		success &= (StringView(addr.GetChars()) == StringView(L"test@example.com"));
		success &= (addr.GetSize() == 17);

		BinaryIMFAddress addr2 = std::move(addr);
		success &= (addr2.AddressFamily == BinaryIMFAddress::Family::IMF);
		success &= (addr2.GetChars() != nullptr);
		success &= (StringView(addr2.GetChars()) == StringView(L"test@example.com"));
		success &= (addr2.GetSize() == 17);
		success &= (addr2.GetStringView() == L"test@example.com");
		success &= (addr2.GetStringView().size() == 16);

		success &= (addr.AddressFamily == BinaryIMFAddress::Family::Unspecified);
		success &= (addr.GetChars() == nullptr);
		success &= (addr.GetSize() == 0);
		success &= (addr.GetStringView() == L"");
		success &= (addr.GetStringView().size() == 0);

		success &= (addr != addr2);

		return success;
	}

	TEST_CLASS(BinaryIMFAddressTests)
	{
	public:
		TEST_METHOD(Constexpr)
		{
			static_assert(CheckConstructor() == true);
			static_assert(CheckCopyConstructor() == true);
			static_assert(CheckMoveConstructor() == true);
			static_assert(CheckCopyAssignment() == true);
			static_assert(CheckMoveAssignment() == true);
		}
	};
}