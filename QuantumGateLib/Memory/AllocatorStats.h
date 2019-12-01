// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Concurrency\ThreadSafe.h"
#include "..\Concurrency\SharedSpinMutex.h"

#include <map>

namespace QuantumGate::Implementation::Memory
{
	struct AllocatorStats final
	{
		static constexpr std::size_t SizeGranularity{ 16 };

		std::set<std::size_t> Sizes;
		std::map<std::uintptr_t, std::size_t> MemoryInUse;

		void AddAllocation(const void* p, const std::size_t len)
		{
			Sizes.insert(len - (len % SizeGranularity));

			if (p != nullptr) MemoryInUse.insert({ reinterpret_cast<std::uintptr_t>(p), len });
		}

		void RemoveAllocation(const void* p, const std::size_t len)
		{
			MemoryInUse.erase(reinterpret_cast<std::uintptr_t>(p));
		}

		[[nodiscard]] std::wstring GetMemoryInUse() const
		{
			std::vector<std::size_t> sizes;
			sizes.reserve(MemoryInUse.size());
			
			// Get all sizes from memory in use
			std::transform(MemoryInUse.begin(), MemoryInUse.end(), std::back_inserter(sizes),
						   [](const auto& kv)
			{
				return kv.second;
			});

			// Get unique sizes
			std::sort(sizes.begin(), sizes.end());
			const auto last = std::unique(sizes.begin(), sizes.end());
			sizes.erase(last, sizes.end());

			std::wstring output;
			std::size_t total{ 0 };

			// Count allocations per size
			for (auto it = sizes.begin(); it != sizes.end(); ++it)
			{
				const auto num = std::count_if(MemoryInUse.begin(), MemoryInUse.end(),
											[&](const auto& kv)
				{
					return kv.second == *it;
				});

				output += FormatString(L"%8zu buffers of %8zu bytes each\r\n", num, *it);
				total += *it;
			}

			output += FormatString(L"\r\nTotal: %zu bytes in %zu allocations\r\n", total, MemoryInUse.size());

			return output;
		}

		[[nodiscard]] std::wstring GetAllSizes() const
		{
			std::wstring output;

			for (const auto size : Sizes)
			{
				output += FormatString(L"%8zu bytes\r\n", size);
			}

			return output;
		}

		static std::wstring FormatString(const wchar_t* format, va_list arglist) noexcept
		{
			try
			{
				const std::size_t size = static_cast<std::size_t>(_vscwprintf(format, arglist)) + 1; // include space for '\0'

				std::wstring txt;
				txt.resize(size - 1); // exclude space for '\0'
				std::vswprintf(txt.data(), size, format, arglist);

				return txt;
			}
			catch (...) {}

			return {};
		}

		static std::wstring FormatString(const wchar_t* format, ...) noexcept
		{
			va_list argptr = nullptr;
			va_start(argptr, format);

			auto fstr = FormatString(format, argptr);

			va_end(argptr);

			return fstr;
		}
	};

	using AllocatorStats_ThS = Concurrency::ThreadSafe<AllocatorStats, std::shared_mutex>;
}