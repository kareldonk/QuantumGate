// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Result.h"
#include "Common\Util.h"

using namespace std::literals;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

Result<> SucceedTestFunction()
{
	return ResultCode::Succeeded;
}

Result<int> SucceedTestFunction2()
{
	return 2;
}

Result<> FailTestFunction()
{
	return ResultCode::Failed;
}

Result<int> FailTestFunction2()
{
	return ResultCode::InvalidArgument;
}

namespace UnitTests
{
	TEST_CLASS(ResultTests)
	{
	public:
		TEST_METHOD(General)
		{
			// Default constructor
			{
				Result<void> result;
				Assert::AreEqual(false, result.Succeeded());
				Assert::AreEqual(false, result.operator bool());
				Assert::AreEqual(true, result.Failed());
				Assert::AreEqual(true, result.GetErrorValue() == static_cast<int>(ResultCode::Failed));

				Result<int> result2;
				Assert::AreEqual(false, result2.Succeeded());
				Assert::AreEqual(false, result2.operator bool());
				Assert::AreEqual(true, result2.Failed());
				Assert::AreEqual(true, result2.GetErrorValue() == static_cast<int>(ResultCode::Failed));
			}

			// Code constructor
			{
				Result<> result{ ResultCode::AddressInvalid };
				Assert::AreEqual(false, result.Succeeded());
				Assert::AreEqual(false, result.operator bool());
				Assert::AreEqual(true, result.Failed());
				Assert::AreEqual(true, result.GetErrorValue() == static_cast<int>(ResultCode::AddressInvalid));

				Result<> result2{ ResultCode::Succeeded };
				Assert::AreEqual(true, result2.Succeeded());
				Assert::AreEqual(true, result2.operator bool());
				Assert::AreEqual(false, result2.Failed());
				Assert::AreEqual(true, result2.GetErrorValue() == static_cast<int>(ResultCode::Succeeded));

				// Result with value cannot be constructed with success code without a value
				{
					Assert::ExpectException<std::invalid_argument>([] {
						Result<int> result3{ ResultCode::Succeeded };
					});

					Assert::ExpectException<std::invalid_argument>([] {
						Result<int> result3{ std::error_code(0, std::system_category()) };
					});
				}

				Result<int> result4{ ResultCode::Failed };
				Result<int> result5{ std::error_code(-1, std::system_category()) };
			}

			// Move constructor
			{
				Result<int> result{ 2 };

				Result<int> result1{ std::move(result) };

				Assert::AreEqual(true, result1.Succeeded());
				Assert::AreEqual(true, result1.operator bool());
				Assert::AreEqual(false, result1.Failed());
				Assert::AreEqual(2, result1.GetValue());
				Assert::AreEqual(2, *result1);
			}

			// Move assignment
			{
				Result<int> result{ 2 };
				Result<int> result1{ ResultCode::Failed };
				
				Assert::AreEqual(false, result1.Succeeded());
				Assert::AreEqual(false, result1.operator bool());
				Assert::AreEqual(true, result1.Failed());
				Assert::AreEqual(false, result1.HasValue());

				result1 = std::move(result);

				Assert::AreEqual(true, result1.Succeeded());
				Assert::AreEqual(true, result1.operator bool());
				Assert::AreEqual(false, result1.Failed());
				Assert::AreEqual(true, result1.HasValue());
				Assert::AreEqual(2, result1.GetValue());
				Assert::AreEqual(2, *result1);
			}

			// Operator == and !=
			{
				auto result = SucceedTestFunction();
				Assert::AreEqual(true, result == ResultCode::Succeeded);
				Assert::AreEqual(true, result.operator bool());

				auto result1 = FailTestFunction();
				Assert::AreEqual(true, result1 == ResultCode::Failed);
				Assert::AreEqual(false, result1.operator bool());

				auto result2 = FailTestFunction2();
				Assert::AreEqual(true, result2 == ResultCode::InvalidArgument);
				Assert::AreEqual(true, result2 != ResultCode::Succeeded);
				Assert::AreEqual(false, result2.operator bool());
			}

			// HasValue and Clear
			{
				auto result = SucceedTestFunction2();
				Assert::AreEqual(true, result == ResultCode::Succeeded);
				Assert::AreEqual(true, result.operator bool());
				Assert::AreEqual(false, result.Failed());
				Assert::AreEqual(true, result.Succeeded());
				Assert::AreEqual(true, result.HasValue());
				Assert::AreEqual(2, result.GetValue());
				Assert::AreEqual(2, *result);

				result.Clear();

				Assert::AreEqual(false, result.Succeeded());
				Assert::AreEqual(false, result.operator bool());
				Assert::AreEqual(false, result.HasValue());

				auto result2 = FailTestFunction2();
				Assert::AreEqual(true, result2 == ResultCode::InvalidArgument);
				Assert::AreEqual(true, result2.Failed());
				Assert::AreEqual(false, result2.Succeeded());
				Assert::AreEqual(false, result2.operator bool());
				Assert::AreEqual(false, result2.HasValue());

				result2.Clear();

				Assert::AreEqual(false, result2.Succeeded());
				Assert::AreEqual(false, result2.operator bool());
				Assert::AreEqual(false, result2.HasValue());

				auto result3 = SucceedTestFunction();
				Assert::AreEqual(true, result3 == ResultCode::Succeeded);
				Assert::AreEqual(true, result3.operator bool());
				Assert::AreEqual(false, result3.Failed());
				Assert::AreEqual(true, result3.Succeeded());

				result3.Clear();

				Assert::AreEqual(false, result3.Succeeded());
				Assert::AreEqual(true, result3.Failed());
				Assert::AreEqual(false, result3.operator bool());
			}

			// Description, Category and Code
			{
				auto result = SucceedTestFunction();
				Assert::AreEqual(true, result.GetErrorValue() == static_cast<int>(ResultCode::Succeeded));
				Assert::AreEqual(true, result.GetErrorCategory() == Util::ToStringW(GetErrorCategory().name()));
				Assert::AreEqual(true, result.GetErrorDescription() == Util::ToStringW(GetErrorCategory().message(result.GetErrorValue())));

				auto result2 = FailTestFunction2();
				Assert::AreEqual(true, result2.GetErrorValue() == static_cast<int>(ResultCode::InvalidArgument));
				Assert::AreEqual(true, result2.GetErrorCategory() == Util::ToStringW(GetErrorCategory().name()));
				Assert::AreEqual(true, result2.GetErrorDescription() == Util::ToStringW(GetErrorCategory().message(result2.GetErrorValue())));
			}
		}

		TEST_METHOD(Functions)
		{
			{
				auto x = 0u;

				SucceedTestFunction().Succeeded([&]() { x = 10u; });
				SucceedTestFunction().Failed([&]() { x = 5u; });

				Assert::AreEqual(true, x == 10);
			}

			{
				auto x = 0u;

				SucceedTestFunction().Succeeded([&](auto& result)
				{
					Assert::AreEqual(true, result == ResultCode::Succeeded);
					Assert::AreEqual(true, result.operator bool());
					Assert::AreEqual(true, result.Succeeded());
					Assert::AreEqual(false, result.Failed());

					x = 10u;
				});

				SucceedTestFunction().Failed([&]() { x = 5u; });

				Assert::AreEqual(true, x == 10);
			}

			{
				auto x = 0u;

				FailTestFunction().Succeeded([&]() { x = 10u; });
				FailTestFunction().Failed([&]() { x = 5u; });

				Assert::AreEqual(true, x == 5);
			}

			{
				auto x = 0u;

				FailTestFunction().Succeeded([&]() { x = 10u; });
				FailTestFunction().Failed([&](auto& result)
				{
					Assert::AreEqual(true, result == ResultCode::Failed);
					Assert::AreEqual(false, result.operator bool());
					Assert::AreEqual(false, result.Succeeded());
					Assert::AreEqual(true, result.Failed());

					x = 5u;
				});

				Assert::AreEqual(true, x == 5);
			}
		}
	};
}