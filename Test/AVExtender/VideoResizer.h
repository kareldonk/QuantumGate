// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"
#include "CaptureDevice.h"

#include <wmcodecdsp.h>

namespace QuantumGate::AVExtender
{
	class VideoResizer final
	{
	public:
		VideoResizer() noexcept;
		VideoResizer(const VideoResizer&) = delete;
		VideoResizer(VideoResizer&&) = default;
		~VideoResizer();
		VideoResizer& operator=(const VideoResizer&) = delete;
		VideoResizer& operator=(VideoResizer&&) = default;

		[[nodiscard]] bool Create(const VideoFormat& in_video_format, const Size out_width, const Size out_height) noexcept;
		void Close() noexcept;
		[[nodiscard]] inline bool IsOpen() const noexcept { return m_Open; }

		[[nodiscard]] inline const VideoFormat& GetOutputFormat() const noexcept { return m_OutputFormat; }

		[[nodiscard]] bool Resize(IMFSample* in_sample, IMFSample* out_sample) noexcept;
		[[nodiscard]] IMFSample* Resize(IMFSample* in_sample) noexcept;

	private:
		inline static const CLSID CLSID_CResizerDMO{ __uuidof(CResizerDMO) };

	private:
		bool m_Open{ false };

		VideoFormat m_OutputFormat;

		IMFTransform* m_IMFTransform{ nullptr };
		IMFMediaType* m_InputMediaType{ nullptr };
		IMFMediaType* m_OutputMediaType{ nullptr };
		IMFSample* m_OutputSample{ nullptr };
		IMFMediaBuffer* m_OutputBuffer{ nullptr };
	};

	using VideoResizer_ThS = Concurrency::ThreadSafe<VideoResizer, std::shared_mutex>;
}