// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AudioResampler.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	AudioResampler::AudioResampler() noexcept
	{
		DiscardReturnValue(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	}

	AudioResampler::~AudioResampler()
	{
		Close();

		CoUninitialize();
	}

	bool AudioResampler::Create(const AudioSettings& in_settings, const AudioSettings& out_settings) noexcept
	{
		// Close if failed
		auto sg = MakeScopeGuard([&]() noexcept { Close(); });

		const auto hr = CoCreateInstance(CLSID_CResamplerMediaObject, nullptr, CLSCTX_ALL,
										 IID_IMFTransform, (void**)&m_IMFTransform);
		if (SUCCEEDED(hr))
		{
			if (SUCCEEDED(m_IMFTransform->QueryInterface(&m_WMResamplerProps)) &&
				SUCCEEDED(m_WMResamplerProps->SetHalfFilterLength(30)))
			{
				// Set input type
				if (SUCCEEDED(MFCreateMediaType(&m_InputMediaType)) &&
					SUCCEEDED(m_InputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) &&
					SUCCEEDED(m_InputMediaType->SetGUID(MF_MT_SUBTYPE, in_settings.BitsPerSample == 32 ? MFAudioFormat_Float : MFAudioFormat_PCM)) &&
					SUCCEEDED(m_InputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, in_settings.BitsPerSample)) &&
					SUCCEEDED(m_InputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, in_settings.NumChannels)) &&
					SUCCEEDED(m_InputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, in_settings.SamplesPerSecond)) &&
					SUCCEEDED(m_InputMediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, in_settings.BlockAlignment)) &&
					SUCCEEDED(m_InputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, in_settings.AvgBytesPerSecond)) &&
					SUCCEEDED(m_InputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE)) &&
					SUCCEEDED(m_IMFTransform->SetInputType(0, m_InputMediaType, 0)))
				{
					// Set output type
					if (SUCCEEDED(MFCreateMediaType(&m_OutputMediaType)) &&
						SUCCEEDED(m_OutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) &&
						SUCCEEDED(m_OutputMediaType->SetGUID(MF_MT_SUBTYPE, out_settings.BitsPerSample == 32 ? MFAudioFormat_Float : MFAudioFormat_PCM)) &&
						SUCCEEDED(m_OutputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, out_settings.BitsPerSample)) &&
						SUCCEEDED(m_OutputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, out_settings.NumChannels)) &&
						SUCCEEDED(m_OutputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, out_settings.SamplesPerSecond)) &&
						SUCCEEDED(m_OutputMediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, out_settings.BlockAlignment)) &&
						SUCCEEDED(m_OutputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, out_settings.AvgBytesPerSecond)) &&
						SUCCEEDED(m_IMFTransform->SetOutputType(0, m_OutputMediaType, 0)))
					{
						if (SUCCEEDED(m_IMFTransform->GetInputStreamInfo(0, &m_InputStreamInfo)) &&
							SUCCEEDED(m_IMFTransform->GetOutputStreamInfo(0, &m_OutputStreamInfo)) &&
							SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL)) &&
							SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL)) &&
							SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL)))
						{
							sg.Deactivate();

							return true;
						}
					}
				}
			}
		}

		return false;
	}

	void AudioResampler::Close() noexcept
	{
		SafeRelease(&m_IMFTransform);
		SafeRelease(&m_WMResamplerProps);
		SafeRelease(&m_InputMediaType);
		SafeRelease(&m_OutputMediaType);
	}
}