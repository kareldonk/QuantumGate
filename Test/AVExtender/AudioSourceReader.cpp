// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AudioSourceReader.h"

#include <Common\ScopeGuard.h>
#include <Common\Util.h>

namespace QuantumGate::AVExtender
{
	AudioSourceReader::AudioSourceReader() noexcept : SourceReader(CaptureDevice::Type::Audio)
	{}

	AudioSourceReader::~AudioSourceReader()
	{}

	STDMETHODIMP AudioSourceReader::QueryInterface(REFIID riid, void** ppvObject)
	{
		static const QITAB qit[] = { QITABENT(AudioSourceReader, IMFSourceReaderCallback), { 0 } };
		return QISearch(this, qit, riid, ppvObject);
	}

	STDMETHODIMP_(ULONG) AudioSourceReader::Release()
	{
		const ULONG count = InterlockedDecrement(&m_RefCount);
		if (count == 0)
		{
			delete this;
		}

		return count;
	}

	STDMETHODIMP_(ULONG) AudioSourceReader::AddRef()
	{
		return InterlockedIncrement(&m_RefCount);
	}

	Result<> AudioSourceReader::OnMediaTypeChanged(IMFMediaType* media_type) noexcept
	{
		auto audio_settings = m_AudioFormat.WithUniqueLock();

		auto hr = media_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &audio_settings->NumChannels);
		if (SUCCEEDED(hr))
		{
			hr = media_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &audio_settings->SamplesPerSecond);
			if (SUCCEEDED(hr))
			{
				hr = media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &audio_settings->BitsPerSample);
				if (SUCCEEDED(hr))
				{
					hr = media_type->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &audio_settings->AvgBytesPerSecond);
					if (SUCCEEDED(hr))
					{
						hr = media_type->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &audio_settings->BlockAlignment);
						if (SUCCEEDED(hr))
						{
							return AVResultCode::Succeeded;
						}
					}
				}
			}
		}

		return AVResultCode::Failed;
	}

	Result<Size> AudioSourceReader::GetBufferSize(IMFMediaType* media_type) noexcept
	{
		const auto audio_format = m_AudioFormat.WithSharedLock();

		return static_cast<Size>(audio_format->AvgBytesPerSecond);
	}
}