// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "AVErrorCode.h"

#include <QuantumGate.h>
#include <Concurrency\ThreadSafe.h>

namespace QuantumGate::AVExtender
{
	struct BGRAPixel
	{
		QuantumGate::Byte B{ 0 };
		QuantumGate::Byte G{ 0 };
		QuantumGate::Byte R{ 0 };
		QuantumGate::Byte A{ 0 };
	};

	struct BGRPixel
	{
		QuantumGate::Byte B{ 0 };
		QuantumGate::Byte G{ 0 };
		QuantumGate::Byte R{ 0 };
	};

	template <bool top_down>
	inline void BGR24ToBGRA32(BGRAPixel* dest_buffer, const BGRPixel* source_buffer, UInt width, UInt height) noexcept
	{
		if (dest_buffer == nullptr || source_buffer == nullptr)
		{
			return;
		}

		const UInt num_pixels{ width * height };

		if constexpr (top_down)
		{
			for (UInt c = 0; c < num_pixels; ++c)
			{
				dest_buffer[c].B = source_buffer[c].B;
				dest_buffer[c].G = source_buffer[c].G;
				dest_buffer[c].R = source_buffer[c].R;
				dest_buffer[c].A = Byte{ 255 };
			}
		}
		else
		{
			UInt c2 = num_pixels - 1;
			for (UInt c = 0; c < num_pixels; ++c)
			{
				dest_buffer[c].B = source_buffer[c2].B;
				dest_buffer[c].G = source_buffer[c2].G;
				dest_buffer[c].R = source_buffer[c2].R;
				dest_buffer[c].A = Byte{ 255 };
				c2--;
			}
		}
	}

	void BGR24ToBGRA32(BGRAPixel* dest_buffer, const BGRPixel* source_buffer, UInt width, UInt height, Int stride) noexcept;

	template <class T>
	inline void SafeRelease(T** ppT) noexcept
	{
		if (*ppT)
		{
			try
			{
				(*ppT)->Release();
				*ppT = nullptr;
			}
			catch (...) {}
		}
	}

	struct VideoFormat
	{
		enum PixelFormat : UInt8 { Unknown, RGB24, BGR24, BGRA32 };

		PixelFormat Format{ PixelFormat::Unknown };
		UINT32 Width{ 0 };
		UINT32 Height{ 0 };
		UINT32 BytesPerPixel{ 0 };
		LONG Stride{ 0 };
	};

	using VideoFormat_ThS = QuantumGate::Implementation::Concurrency::ThreadSafe<VideoFormat, std::shared_mutex>;

	struct AudioFormat
	{
		UINT32 NumChannels{ 0 };
		UINT32 SamplesPerSecond{ 0 };
		UINT32 AvgBytesPerSecond{ 0 };
		UINT32 BlockAlignment{ 0 };
		UINT32 BitsPerSample{ 0 };
	};

	using AudioFormat_ThS = QuantumGate::Implementation::Concurrency::ThreadSafe<AudioFormat, std::shared_mutex>;
}