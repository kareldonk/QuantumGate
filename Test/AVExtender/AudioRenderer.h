// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"
#include "AudioResampler.h"

#include <mmdeviceapi.h>
#include <audioclient.h>

namespace QuantumGate::AVExtender
{
	class AudioRenderer final
	{
	public:
		AudioRenderer() noexcept;
		AudioRenderer(const AudioRenderer&) = delete;
		AudioRenderer(AudioRenderer&&) = default;
		~AudioRenderer();
		AudioRenderer& operator=(const AudioRenderer&) = delete;
		AudioRenderer& operator=(AudioRenderer&&) = default;

		[[nodiscard]] bool Create(const AudioFormat& input_audio_settings) noexcept;
		void Close() noexcept;
		[[nodiscard]] inline bool IsOpen() const noexcept { return m_Open; }

		[[nodiscard]] bool Play() noexcept;
		[[nodiscard]] bool Render(const UInt64 in_timestamp, const BufferView sample_data) noexcept;

		[[nodiscard]] inline const AudioFormat& GetOutputFormat() const noexcept { return m_OutputFormat; }

	private:
		[[nodiscard]] bool GetSupportedMixFormat(const AudioFormat& audio_settings, WAVEFORMATEXTENSIBLE& wfmt) noexcept;

	private:
		inline static const CLSID CLSID_MMDeviceEnumerator{ __uuidof(MMDeviceEnumerator) };
		inline static const IID IID_IMMDeviceEnumerator{ __uuidof(IMMDeviceEnumerator) };
		inline static const IID IID_IAudioClient{ __uuidof(IAudioClient) };
		inline static const IID IID_IAudioRenderClient{ __uuidof(IAudioRenderClient) };

	private:
		bool m_Open{ false };

		AudioResampler m_AudioResampler;

		AudioFormat m_OutputFormat;
		IMFSample* m_OutputSample{ nullptr };
		IMFMediaBuffer* m_OutputBuffer{ nullptr };

		REFERENCE_TIME m_BufferDuration{ 0 };
		UINT32 m_BufferFrameCount{ 0 };
		IMMDeviceEnumerator* m_Enumerator{ nullptr };
		IMMDevice* m_Device{ nullptr };
		IAudioClient* m_AudioClient{ nullptr };
		IAudioRenderClient* m_RenderClient{ nullptr };
	};

	using AudioRenderer_ThS = Concurrency::ThreadSafe<AudioRenderer, std::shared_mutex>;
}