// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate;

	enum class MessageType : UInt16
	{
		Unknown = 0,
		CallRequest,
		CallAccept,
		CallDecline,
		CallHangup,
		AudioSample,
		VideoSample,
		GeneralFailure
	};

#pragma pack(push, 1) // Disable padding bytes
	struct AudioFormatData
	{
		UInt8 NumChannels{ 0 };
		UInt32 SamplesPerSecond{ 0 };
		UInt32 AvgBytesPerSecond{ 0 };
		UInt8 BlockAlignment{ 0 };
		UInt8 BitsPerSample{ 0 };
	};

	struct VideoFormatData
	{
		VideoFormat::PixelFormat Format{ VideoFormat::PixelFormat::Unknown };
		UInt16 Width{ 0 };
		UInt16 Height{ 0 };
		UInt8 BytesPerPixel{ 0 };
	};
#pragma pack(pop)
}