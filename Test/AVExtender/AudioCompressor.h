// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Compressor.h"

#include <wmcodecdsp.h>

namespace QuantumGate::AVExtender
{
	class AudioCompressor final : public Compressor
	{
		// The portion of the HEAACWAVEINFO structure that
		// appears after the WAVEFORMATEX structure
		struct AACInfo
		{
			WORD wPayloadType{ 0 };
			WORD wAudioProfileLevelIndication{ 0 };
			WORD wStructType{ 0 };
			WORD wReserved1{ 0 };
			DWORD dwReserved2{ 0 };
		};

	public:
		AudioCompressor(const Type type) noexcept;
		AudioCompressor(const AudioCompressor&) = delete;
		AudioCompressor(AudioCompressor&&) = default;
		virtual ~AudioCompressor();
		AudioCompressor& operator=(const AudioCompressor&) = delete;
		AudioCompressor& operator=(AudioCompressor&&) = default;

		[[nodiscard]] static AudioFormat GetEncoderInputFormat() noexcept;
		[[nodiscard]] static AudioFormat GetDecoderOutputFormat() noexcept;

		[[nodiscard]] const AudioFormat& GetInputFormat() const noexcept { return m_InputFormat; }
		[[nodiscard]] const AudioFormat& GetOutputFormat() const noexcept { return m_OutputFormat; }

	private:
		[[nodiscard]] void OnClose() noexcept override;
		[[nodiscard]] UInt64 GetDuration(const Size sample_size) noexcept override;
		[[nodiscard]] bool OnCreateMediaTypes(IMFMediaType* input_type, IMFMediaType* output_type) noexcept override;
		[[nodiscard]] bool OnSetMediaTypes(IMFTransform* transform, IMFMediaType* input_type, IMFMediaType* output_type) noexcept override;

	private:
		inline static const CLSID CLSID_AACMFTEncoder{ __uuidof(AACMFTEncoder) };
		inline static const CLSID CLSID_CMSAACDecMFT{ __uuidof(CMSAACDecMFT) };

	private:
		AudioFormat m_InputFormat;
		AudioFormat m_OutputFormat;
	};
}