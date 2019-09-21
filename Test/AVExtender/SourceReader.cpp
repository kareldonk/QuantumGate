// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "SourceReader.h"

#include <Common\Util.h>

namespace QuantumGate::AVExtender
{
	SourceReader::SourceReader(const CaptureDevice::Type type) noexcept :
		m_Type(type)
	{
		DiscardReturnValue(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	}

	SourceReader::~SourceReader()
	{
		CoUninitialize();
	}

	Result<CaptureDeviceVector> SourceReader::EnumCaptureDevices() const noexcept
	{
		switch (m_Type)
		{
			case CaptureDevice::Type::Video:
				return CaptureDevices::Enum(CaptureDevice::Type::Video);
			case CaptureDevice::Type::Audio:
				return CaptureDevices::Enum(CaptureDevice::Type::Audio);
			default:
				assert(false);
				break;
		}

		return AVResultCode::Failed;
	}
}