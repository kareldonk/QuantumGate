// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Common.h"

namespace QuantumGate::AVExtender
{
	void BGR24ToBGRA32(BGRAPixel* dest_buffer, const BGRPixel* source_buffer, UInt width, UInt height, Int stride) noexcept
	{
		if (stride > 0)
		{
			BGR24ToBGRA32<true>(dest_buffer, source_buffer, width, height);
		}
		else BGR24ToBGRA32<false>(dest_buffer, source_buffer, width, height);
	}
}