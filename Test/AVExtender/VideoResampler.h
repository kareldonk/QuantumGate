// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"
#include "CaptureDevice.h"

#include <dshow.h>
#include <dmo.h>
#include <Wmcodecdsp.h>

#pragma comment(lib, "strmiids.lib")

namespace QuantumGate::AVExtender
{
	class VideoResampler final
	{
		struct DMOData
		{
			DMOData() = default;

			DMOData(const Size width, const Size height, const Size bitcount,
					const DWORD compression, const GUID mfsubtype, const GUID dmosubtype) noexcept
			{
				MFSubtype = mfsubtype;

				VideoInfoHeader.bmiHeader.biSize = sizeof(VideoInfoHeader.bmiHeader);
				VideoInfoHeader.bmiHeader.biWidth = static_cast<LONG>(width);
				VideoInfoHeader.bmiHeader.biHeight = static_cast<LONG>(height);
				VideoInfoHeader.bmiHeader.biPlanes = 1;
				VideoInfoHeader.bmiHeader.biBitCount = static_cast<WORD>(bitcount);
				VideoInfoHeader.bmiHeader.biCompression = compression;
				VideoInfoHeader.bmiHeader.biSizeImage = static_cast<LONG>(height * width * bitcount / 8);

				DMOMediaType.majortype = MEDIATYPE_Video;
				DMOMediaType.subtype = dmosubtype;
				DMOMediaType.bFixedSizeSamples = TRUE;
				DMOMediaType.bTemporalCompression = FALSE;
				DMOMediaType.lSampleSize = VideoInfoHeader.bmiHeader.biSizeImage;
				DMOMediaType.formattype = FORMAT_VideoInfo;
				DMOMediaType.cbFormat = sizeof(VIDEOINFOHEADER);
				DMOMediaType.pbFormat = (BYTE*)&VideoInfoHeader;
			}

			VideoFormat GetVideoFormat() const noexcept
			{
				VideoFormat fmt;
				fmt.Format = CaptureDevices::GetVideoFormat(MFSubtype);
				fmt.Width = VideoInfoHeader.bmiHeader.biWidth;
				fmt.Height = VideoInfoHeader.bmiHeader.biHeight;
				fmt.BytesPerPixel = VideoInfoHeader.bmiHeader.biBitCount / 8;
				fmt.Stride = VideoInfoHeader.bmiHeader.biWidth * VideoInfoHeader.bmiHeader.biBitCount / 8;
				return fmt;
			}

			GUID MFSubtype{ GUID_NULL };
			VIDEOINFOHEADER VideoInfoHeader{ 0 };
			DMO_MEDIA_TYPE DMOMediaType{ 0 };
		};

	public:
		VideoResampler() noexcept;
		VideoResampler(const VideoResampler&) = delete;
		VideoResampler(VideoResampler&&) = default;
		~VideoResampler();
		VideoResampler& operator=(const VideoResampler&) = delete;
		VideoResampler& operator=(VideoResampler&&) = default;

		[[nodiscard]] bool Create(const Size width, const Size height,
								  const GUID in_video_format, const GUID out_video_format) noexcept;
		void Close() noexcept;
		[[nodiscard]] inline bool IsOpen() const noexcept { return m_Open; }

		[[nodiscard]] inline const VideoFormat& GetInputFormat() const noexcept { return m_InputFormat; }
		[[nodiscard]] inline const VideoFormat& GetOutputFormat() const noexcept { return m_OutputFormat; }

		[[nodiscard]] bool Resample(IMFSample* in_sample, IMFSample* out_sample) noexcept;

	private:
		DMOData GetMediaType(const Size width, const Size height, const GUID type) const noexcept;

	private:
		inline static const CLSID CLSID_CColorConvertDMO{ __uuidof(CColorConvertDMO) };

	private:
		bool m_Open{ false };

		IMFTransform* m_IMFTransform{ nullptr };
		IMediaObject* m_IMediaObject{ nullptr };
		VideoFormat m_InputFormat;
		VideoFormat m_OutputFormat;
	};

	using VideoResampler_ThS = Concurrency::ThreadSafe<VideoResampler, std::shared_mutex>;
}