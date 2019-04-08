// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Algorithms.h"

namespace QuantumGate::Implementation::Compression
{
	[[nodiscard]] Export bool Compress(const BufferView& inbuffer, Buffer& outbuffer,
									   const Algorithm::Compression ca) noexcept;
	[[nodiscard]] Export bool Decompress(BufferView inbuffer, Buffer& outbuffer, const Algorithm::Compression ca,
										 const std::optional<Size> maxsize = std::nullopt) noexcept;
}