// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"
#include "AudioResampler.h"

#include <mmdeviceapi.h>
#include <audioclient.h>

namespace QuantumGate::AVExtender
{
	class AudioRenderer
	{
	public:
		AudioRenderer() noexcept;
		AudioRenderer(const AudioRenderer&) = delete;
		AudioRenderer(AudioRenderer&&) = default;
		~AudioRenderer();
		AudioRenderer& operator=(const AudioRenderer&) = delete;
		AudioRenderer& operator=(AudioRenderer&&) = default;

		[[nodiscard]] bool Create(const AudioSettings& input_audio_settings) noexcept;
		void Close() noexcept;
		[[nodiscard]] inline bool IsOpen() const noexcept { return m_RenderClient != nullptr; }
		
		[[nodiscard]] bool Play() noexcept;
		[[nodiscard]] bool Render(const Byte* src_data, const Size src_len) noexcept;
		[[nodiscard]] inline const AudioSettings& GetOutputAudioSettings() const noexcept { return m_OutputAudioSettings; }

	private:
		[[nodiscard]] bool GetSupportedMixFormat(const AudioSettings& audio_settings, WAVEFORMATEX& wfmt) noexcept;

	private:
		inline static const CLSID CLSID_MMDeviceEnumerator{ __uuidof(MMDeviceEnumerator) };
		inline static const IID IID_IMMDeviceEnumerator{ __uuidof(IMMDeviceEnumerator) };
		inline static const IID IID_IAudioClient{ __uuidof(IAudioClient) };
		inline static const IID IID_IAudioRenderClient{ __uuidof(IAudioRenderClient) };

	private:
		AudioResampler m_AudioSampler;
		AudioSettings m_OutputAudioSettings;
		REFERENCE_TIME m_BufferDuration{ 0 };
		UINT32 m_BufferFrameCount{ 0 };
		IMMDeviceEnumerator* m_Enumerator{ nullptr };
		IMMDevice* m_Device{ nullptr };
		IAudioClient* m_AudioClient{ nullptr };
		IAudioRenderClient* m_RenderClient{ nullptr };
	};
}