// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"
#include "CaptureDevice.h"

#include <Wmcodecdsp.h>

namespace QuantumGate::AVExtender
{
	class AudioResampler
	{
	public:
		AudioResampler() noexcept;
		AudioResampler(const AudioResampler&) = delete;
		AudioResampler(AudioResampler&&) = default;
		~AudioResampler();
		AudioResampler& operator=(const AudioResampler&) = delete;
		AudioResampler& operator=(AudioResampler&&) = default;

		[[nodiscard]] bool Create(const AudioSettings& in_settings, const AudioSettings& out_settings) noexcept;
		void Close() noexcept;
		[[nodiscard]] inline bool IsOpen() const noexcept { return m_IMFTransform != nullptr; }

	private:
		inline static const CLSID CLSID_CResamplerMediaObject{ __uuidof(CResamplerMediaObject) };

	private:
		IWMResamplerProps* m_WMResamplerProps{ nullptr };
		IMFTransform* m_IMFTransform{ nullptr };
		IMFMediaType* m_InputMediaType{ nullptr };
		IMFMediaType* m_OutputMediaType{ nullptr };
		MFT_INPUT_STREAM_INFO m_InputStreamInfo{ 0 };
		MFT_OUTPUT_STREAM_INFO m_OutputStreamInfo{ 0 };
	};
}