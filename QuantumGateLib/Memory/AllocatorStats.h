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
		std::set<Size> Sizes;
		std::map<std::uintptr_t, Size> MemoryInUse;

		String GetMemoryInUse(const WChar* header) const
		{
			assert(header != nullptr);

			String output{ header };

			Size total{ 0 };

			for (auto it = MemoryInUse.begin(); it != MemoryInUse.end(); ++it)
			{
				output += Util::FormatString(L"%u\r\n", it->second);
				total += it->second;
			}

			output += Util::FormatString(L"\r\nTotal: %u bytes in %u allocations\r\n", total, MemoryInUse.size());

			return output;
		}

		String GetAllSizes(const WChar* header) const
		{
			assert(header != nullptr);

			String output{ header };

			for (const auto size : Sizes)
			{
				output += Util::FormatString(L"%u\r\n", size);
			}

			return output;
		}
	};

	using AllocatorStats_ThS = Concurrency::ThreadSafe<AllocatorStats, Concurrency::SharedSpinMutex>;
}