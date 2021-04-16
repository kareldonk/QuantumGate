// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "AVErrorCode.h"

#include <QuantumGate.h>
#include <Concurrency\ThreadSafe.h>
#include <Concurrency\ThreadLocalCache.h>

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

	template<bool flip>
	inline void RGB24ToBGRA32(BGRAPixel* dest_buffer, const BGRPixel* source_buffer, UInt width, UInt height) noexcept
	{
		if (dest_buffer == nullptr || source_buffer == nullptr)
		{
			return;
		}

		const UInt num_pixels{ width * height };

		if constexpr (flip)
		{
			UInt c2 = num_pixels - 1;
			for (UInt c = 0; c < num_pixels; ++c)
			{
				dest_buffer[c].B = source_buffer[c2].B;
				dest_buffer[c].G = source_buffer[c2].G;
				dest_buffer[c].R = source_buffer[c2].R;
				dest_buffer[c].A = Byte{ 255 };
				--c2;
			}
		}
		else
		{
			for (UInt c = 0; c < num_pixels; ++c)
			{
				dest_buffer[c].B = source_buffer[c].B;
				dest_buffer[c].G = source_buffer[c].G;
				dest_buffer[c].R = source_buffer[c].R;
				dest_buffer[c].A = Byte{ 255 };
			}
		}
	}

	template<bool flip>
	inline void ARGB32ToBGRA32(BGRAPixel* dest_buffer, const BGRAPixel* source_buffer, UInt width, UInt height) noexcept
	{
		if (dest_buffer == nullptr || source_buffer == nullptr)
		{
			return;
		}

		const UInt num_pixels{ width * height };
		if constexpr (flip)
		{
			UInt c2 = num_pixels - 1;
			for (UInt c = 0; c < num_pixels; ++c)
			{
				dest_buffer[c].B = source_buffer[c2].B;
				dest_buffer[c].G = source_buffer[c2].G;
				dest_buffer[c].R = source_buffer[c2].R;
				dest_buffer[c].A = source_buffer[c2].A;
				--c2;
			}
		}
		else
		{
			std::memcpy(dest_buffer, source_buffer, num_pixels * sizeof(BGRAPixel));
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
		enum class PixelFormat : UInt8 { Unknown, RGB24, RGB32, NV12, YV12, I420 };

		PixelFormat Format{ PixelFormat::Unknown };
		UInt32 Width{ 0 };
		UInt32 Height{ 0 };
		UInt32 BytesPerPixel{ 0 };

		constexpr bool operator==(const VideoFormat& other) const noexcept
		{
			return (Format == other.Format &&
					Width == other.Width &&
					Height == other.Height &&
					BytesPerPixel == other.BytesPerPixel);
		}

		constexpr bool operator!=(const VideoFormat& other) const noexcept
		{
			return !(*this == other);
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

		constexpr bool operator==(const AudioFormat& other) const noexcept
		{
			return (NumChannels == other.NumChannels &&
					SamplesPerSecond == other.SamplesPerSecond &&
					AvgBytesPerSecond == other.AvgBytesPerSecond &&
					BlockAlignment == other.BlockAlignment &&
					BitsPerSample == other.BitsPerSample);
		}

		constexpr bool operator!=(const AudioFormat& other) const noexcept
		{
			return !(*this == other);
		}
	};

	using AudioFormat_ThS = QuantumGate::Implementation::Concurrency::ThreadSafe<AudioFormat, std::shared_mutex>;

	struct Settings
	{
		bool UseCompression{ true };
		bool UseVideoCompression{ true };
		bool UseAudioCompression{ true };
		bool FillVideoScreen{ false };
	};

	using Settings_ThS = QuantumGate::Implementation::Concurrency::ThreadLocalCache<Settings>;
}