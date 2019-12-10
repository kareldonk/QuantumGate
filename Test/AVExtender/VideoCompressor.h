// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Compressor.h"

#include <wmcodecdsp.h>

namespace QuantumGate::AVExtender
{
	class VideoCompressor final : public Compressor
	{
	public:
		VideoCompressor(const Type type) noexcept;
		VideoCompressor(const VideoCompressor&) = delete;
		VideoCompressor(VideoCompressor&&) noexcept = default;
		virtual ~VideoCompressor();
		VideoCompressor& operator=(const VideoCompressor&) = delete;
		VideoCompressor& operator=(VideoCompressor&&) noexcept = default;

		void SetFormat(const UInt16 width, const UInt16 height, const GUID& video_format) noexcept;

	private:
		[[nodiscard]] void OnClose() noexcept override;
		[[nodiscard]] UInt64 GetDuration(const Size sample_size) noexcept override;
		[[nodiscard]] bool OnCreateMediaTypes(IMFMediaType* input_type, IMFMediaType* output_type) noexcept override;
		[[nodiscard]] bool OnSetMediaTypes(IMFTransform* transform, IMFMediaType* input_type, IMFMediaType* output_type) noexcept override;

	private:
		inline static const CLSID CLSID_CMSH264EncoderMFT{ __uuidof(CMSH264EncoderMFT) };
		inline static const CLSID CLSID_CMSH264DecoderMFT{ __uuidof(CMSH264DecoderMFT) };

	private:
		UInt16 m_Width{ 0 };
		UInt16 m_Height{ 0 };
		GUID m_VideoFormat{ GUID_NULL };
		UInt8 m_FrameRate{ 30 };

		ICodecAPI* m_ICodecAPI{ nullptr };
	};
}