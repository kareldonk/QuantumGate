// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"
#include "CaptureDevice.h"

#include <Wmcodecdsp.h>

namespace QuantumGate::AVExtender
{
	class AudioResampler final
	{
	public:
		AudioResampler() noexcept;
		AudioResampler(const AudioResampler&) = delete;
		AudioResampler(AudioResampler&&) = default;
		~AudioResampler();
		AudioResampler& operator=(const AudioResampler&) = delete;
		AudioResampler& operator=(AudioResampler&&) = default;

		[[nodiscard]] bool Create(const AudioFormat& in_settings, const AudioFormat& out_settings) noexcept;
		void Close() noexcept;
		[[nodiscard]] inline bool IsOpen() const noexcept { return m_Open; }

		[[nodiscard]] bool Resample(const UInt64 in_timestamp, const BufferView in_data, IMFSample* out_sample) noexcept;
		[[nodiscard]] bool Resample(IMFSample* in_sample, IMFSample* out_sample) noexcept;

	private:
		inline static const CLSID CLSID_CResamplerMediaObject{ __uuidof(CResamplerMediaObject) };

	private:
		bool m_Open{ false };

		IWMResamplerProps* m_WMResamplerProps{ nullptr };
		IMFTransform* m_IMFTransform{ nullptr };
		IMFMediaType* m_InputMediaType{ nullptr };
		IMFMediaType* m_OutputMediaType{ nullptr };

		AudioFormat m_InputFormat;
		AudioFormat m_OutputFormat;
		IMFSample* m_InputSample{ nullptr };
	};
}