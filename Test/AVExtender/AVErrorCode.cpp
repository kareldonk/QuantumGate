// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "AVErrorCode.h"

namespace QuantumGate::AVExtender
{
	std::error_code make_error_code(const AVResultCode code) noexcept
	{
		static AVErrorCategory AVErrorCategory;
		return std::error_code(static_cast<int>(code), AVErrorCategory);
	}
}