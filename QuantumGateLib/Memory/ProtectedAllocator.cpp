// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "ProtectedAllocator.h"

namespace QuantumGate::Implementation::Memory
{
	Concurrency::SpinMutex& ProtectedAllocatorBase::GetProtectedAllocatorMutex() noexcept
	{
		static Concurrency::SpinMutex mutex;
		return mutex;
	}

	const bool ProtectedAllocatorBase::GetCurrentProcessWorkingSetSize(Size& minsize, Size& maxsize) noexcept
	{
		SIZE_T tminsize{ 0 };
		SIZE_T tmaxsize{ 0 };

		if (::GetProcessWorkingSetSize(GetCurrentProcess(), &tminsize, &tmaxsize))
		{
			minsize = tminsize;
			maxsize = tmaxsize;

			LogDbg(L"Process memory working set size is %llu (min) / %llu (max)",
				   static_cast<UInt64>(minsize), static_cast<UInt64>(maxsize));

			return true;
		}
		else LogDbg(L"Could not get process memory working set size");

		return false;
	}

	const bool ProtectedAllocatorBase::SetCurrentProcessWorkingSetSize(const Size minsize, const Size maxsize) noexcept
	{
		if (::SetProcessWorkingSetSize(::GetCurrentProcess(), minsize, maxsize))
		{
			LogDbg(L"Process memory working set size changed to %llu (min) / %llu (max)",
				   static_cast<UInt64>(minsize), static_cast<UInt64>(maxsize));
			return true;
		}
		else LogDbg(L"Could not change process memory working set size to %llu (min) / %llu (max)",
					static_cast<UInt64>(minsize), static_cast<UInt64>(maxsize));

		return false;
	}
}