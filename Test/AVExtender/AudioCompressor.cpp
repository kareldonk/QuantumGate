// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AudioCompressor.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	AudioCompressor::AudioCompressor(const Type type) noexcept :
		Compressor(type, CLSID_AACMFTEncoder, CLSID_CMSAACDecMFT)
	{}

	AudioCompressor::~AudioCompressor()
	{}

	void AudioCompressor::OnClose() noexcept
	{
		m_InputFormat = {};
		m_OutputFormat = {};
	}

	UInt64 AudioCompressor::GetDuration(const Size sample_size) noexcept
	{
		return static_cast<LONGLONG>((static_cast<double>(sample_size) /
									  static_cast<double>(m_InputFormat.SamplesPerSecond)) * 10'000'000.0);
	}

	AudioFormat AudioCompressor::GetEncoderInputFormat() noexcept
	{
		AudioFormat informat;
		informat.NumChannels = 2;
		informat.BitsPerSample = 16;
		informat.SamplesPerSecond = 44100;
		informat.BlockAlignment = 4;
		informat.AvgBytesPerSecond = informat.SamplesPerSecond * informat.BlockAlignment;

		return informat;
	}

	AudioFormat AudioCompressor::GetDecoderOutputFormat() noexcept
	{
		AudioFormat outformat;
		outformat.NumChannels = 2;
		outformat.BitsPerSample = 16;
		outformat.SamplesPerSecond = 44100;

		return outformat;
	}

	bool AudioCompressor::OnCreateMediaTypes(IMFMediaType* input_type, IMFMediaType* output_type) noexcept
	{
		auto intype = input_type;
		auto informat = &m_InputFormat;
		auto outtype = output_type;
		auto outformat = &m_OutputFormat;

		if (GetType() == Type::Decoder)
		{
			intype = output_type;
			informat = &m_OutputFormat;
			outtype = input_type;
			outformat = &m_InputFormat;
		}

		*informat = GetEncoderInputFormat();
		*outformat = GetDecoderOutputFormat();

		if (SUCCEEDED(intype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) &&
			SUCCEEDED(intype->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) &&
			SUCCEEDED(intype->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, informat->BitsPerSample)) &&
			SUCCEEDED(intype->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, informat->SamplesPerSecond)) &&
			SUCCEEDED(intype->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, informat->NumChannels)) &&
			SUCCEEDED(intype->SetUINT32(MF_MT_INTERLACE_MODE, 2)))
		{
			if (SUCCEEDED(outtype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) &&
				SUCCEEDED(outtype->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC)) &&
				SUCCEEDED(outtype->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, outformat->BitsPerSample)) &&
				SUCCEEDED(outtype->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, outformat->SamplesPerSecond)) &&
				SUCCEEDED(outtype->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, outformat->NumChannels)))
			{
				if (GetType() == Type::Encoder)
				{
					// MF_MT_AUDIO_AVG_BYTES_PER_SECOND supported: 12000, 16000, 20000, 24000
					// Docs: https://docs.microsoft.com/en-us/windows/win32/medfound/aac-encoder
					if (SUCCEEDED(outtype->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000)))
					{
						return true;
					}
				}
				else if (GetType() == Type::Decoder)
				{
					AACInfo aacinfo;
					aacinfo.wPayloadType = 0;
					aacinfo.wAudioProfileLevelIndication = 0;

					if (SUCCEEDED(outtype->SetBlob(MF_MT_USER_DATA, reinterpret_cast<UINT8*>(&aacinfo), sizeof(AACInfo))))
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	bool AudioCompressor::OnSetMediaTypes(IMFTransform* transform, IMFMediaType* input_type, IMFMediaType* output_type) noexcept
	{
		auto hr = transform->SetInputType(0, input_type, 0);
		if (SUCCEEDED(hr))
		{
			hr = transform->SetOutputType(0, output_type, 0);
			if (SUCCEEDED(hr))
			{
				return true;
			}
		}

		return false;
	}
}