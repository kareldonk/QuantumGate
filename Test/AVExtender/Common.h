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

	inline void RGB24ToBGRA32(BGRAPixel* dest_buffer, const BGRPixel* source_buffer, UInt width, UInt height) noexcept
	{
		if (dest_buffer == nullptr || source_buffer == nullptr)
		{
			return;
		}

		const UInt num_pixels{ width * height };

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

	inline void ARGB32ToBGRA32(BGRAPixel* dest_buffer, const BGRAPixel* source_buffer, UInt width, UInt height) noexcept
	{
		if (dest_buffer == nullptr || source_buffer == nullptr)
		{
			return;
		}

		const UInt num_pixels{ width * height };

		UInt c2 = num_pixels - 1;
		for (UInt c = 0; c < num_pixels; ++c)
		{
			dest_buffer[c].B = source_buffer[c2].B;
			dest_buffer[c].G = source_buffer[c2].G;
			dest_buffer[c].R = source_buffer[c2].R;
			dest_buffer[c].A = source_buffer[c2].A;
			c2--;
		}
	}

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
		enum class PixelFormat : UInt8 { Unknown, RGB24, RGB32, NV12 };

		PixelFormat Format{ PixelFormat::Unknown };
		UInt32 Width{ 0 };
		UInt32 Height{ 0 };
		UInt32 BytesPerPixel{ 0 };
		Long Stride{ 0 };

		Size GetFrameSize() const noexcept
		{
			switch (Format)
			{
				case PixelFormat::RGB24:
				case PixelFormat::RGB32:
					return BytesPerPixel * Width * Height;
				case PixelFormat::NV12:
					return static_cast<Size>(1.5 * Width * Height);
				default:
					assert(false);
					break;
			}

			return 0;
		}
	};

	using VideoFormat_ThS = QuantumGate::Implementation::Concurrency::ThreadSafe<VideoFormat, std::shared_mutex>;

	struct AudioFormat
	{
		UInt32 NumChannels{ 0 };
		UInt32 SamplesPerSecond{ 0 };
		UInt32 AvgBytesPerSecond{ 0 };
		UInt32 BlockAlignment{ 0 };
		UInt32 BitsPerSample{ 0 };
	};

	using AudioFormat_ThS = QuantumGate::Implementation::Concurrency::ThreadSafe<AudioFormat, std::shared_mutex>;
}