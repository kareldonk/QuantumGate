// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"
#include "CaptureDevice.h"

#include <wmcodecdsp.h>

namespace QuantumGate::AVExtender
{
	class VideoCompressor final
	{
	public:
		enum class Type { Unknown, Encoder, Decoder };

		VideoCompressor(const Type type) noexcept;
		VideoCompressor(const VideoCompressor&) = delete;
		VideoCompressor(VideoCompressor&&) = default;
		~VideoCompressor();
		VideoCompressor& operator=(const VideoCompressor&) = delete;
		VideoCompressor& operator=(VideoCompressor&&) = default;

		[[nodiscard]] bool Create(const Size width, const Size height, const GUID video_format) noexcept;
		void Close() noexcept;
		[[nodiscard]] inline bool IsOpen() const noexcept { return m_Open; }

		[[nodiscard]] bool AddInput(const UInt64 in_timestamp, const BufferView data) noexcept;
		[[nodiscard]] bool AddInput(IMFSample* in_sample) noexcept;

		[[nodiscard]] IMFSample* GetOutput() noexcept;
		[[nodiscard]] bool GetOutput(Buffer& buffer) noexcept;

	private:
		[[nodiscard]] bool CreateMediaTypes(const Size width, const Size height, const GUID video_format) noexcept;

	private:
		inline static const CLSID CLSID_CMSH264EncoderMFT{ __uuidof(CMSH264EncoderMFT) };
		inline static const CLSID CLSID_CMSH264DecoderMFT{ __uuidof(CMSH264DecoderMFT) };

	private:
		const Type m_Type{ Type::Unknown };
		bool m_Open{ false };

		UInt8 m_FrameRate{ 30 };

		IMFTransform* m_IMFTransform{ nullptr };
		ICodecAPI* m_ICodecAPI{ nullptr };
		IMFMediaType* m_InputMediaType{ nullptr };
		IMFMediaType* m_OutputMediaType{ nullptr };
	};
}