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
		CallAVUpdate,
		AudioSample,
		VideoSample,
		GeneralFailure
	};

#pragma pack(push, 1) // Disable padding bytes
	struct CallAVFormatData
	{
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
			Int32 Stride{ 0 };
		};

		UInt8 SendAudio{ 0 };
		AudioFormatData AudioFormat;
		UInt8 SendVideo{ 0 };
		VideoFormatData VideoFormat;
	};
#pragma pack(pop)
}