// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "AudioResampler.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	AudioResampler::AudioResampler() noexcept
	{}

	AudioResampler::~AudioResampler()
	{
		Close();
	}

	bool AudioResampler::Create(const AudioFormat& in_settings, const AudioFormat& out_settings) noexcept
	{
		assert(!IsOpen());

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
						if (SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL)) &&
							SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL)) &&
							SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL)))
						{
							auto result = CaptureDevices::CreateMediaSample(in_settings.AvgBytesPerSecond);
							if (result.Succeeded())
							{
								sg.Deactivate();

								m_InputSample = result.GetValue();

								m_InputFormat = in_settings;
								m_OutputFormat = out_settings;

								m_Open = true;

								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}

	void AudioResampler::Close() noexcept
	{
		m_Open = false;

		SafeRelease(&m_IMFTransform);
		SafeRelease(&m_WMResamplerProps);
		SafeRelease(&m_InputMediaType);
		SafeRelease(&m_OutputMediaType);
		SafeRelease(&m_InputSample);

		m_InputFormat = {};
		m_OutputFormat = {};
	}

	bool AudioResampler::Resample(const UInt64 in_timestamp, const BufferView in_data, IMFSample* out_sample) noexcept
	{
		assert(IsOpen());

		const auto duration = static_cast<LONGLONG>((static_cast<double>(in_data.GetSize()) /
													 static_cast<double>(m_InputFormat.SamplesPerSecond)) * 10000000.0);

		if (CaptureDevices::CopyToMediaSample(in_timestamp, duration, in_data, m_InputSample))
		{
			return Resample(m_InputSample, out_sample);
		}

		return false;
	}

	bool AudioResampler::Resample(IMFSample* in_sample, IMFSample* out_sample) noexcept
	{
		assert(IsOpen());

		// Transform the input sample
		auto hr = m_IMFTransform->ProcessInput(0, in_sample, 0);
		if (SUCCEEDED(hr))
		{
			IMFMediaBuffer* out_buffer{ nullptr };
			hr = out_sample->GetBufferByIndex(0, &out_buffer);
			if (SUCCEEDED(hr))
			{
				// Release when we exit
				const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&out_buffer); });

				hr = out_buffer->SetCurrentLength(0);
				if (SUCCEEDED(hr))
				{
					MFT_OUTPUT_DATA_BUFFER output{ 0 };
					output.dwStreamID = 0;
					output.dwStatus = 0;
					output.pEvents = nullptr;
					output.pSample = out_sample;

					DWORD status{ 0 };

					// Get the transformed output sample back
					hr = m_IMFTransform->ProcessOutput(0, 1, &output, &status);
					if (SUCCEEDED(hr))
					{
						return true;
					}
				}
			}
		}

		return false;
	}
}