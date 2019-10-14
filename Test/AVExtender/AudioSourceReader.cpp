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

	bool AudioSourceReader::SetSampleFormat(const AudioFormat fmt) noexcept
	{
		bool was_open{ false };

		if (IsOpen())
		{
			was_open = true;

			CloseAudioTransform();
		}

		{
			auto format_data = m_AudioFormatData.WithUniqueLock();
			format_data->TransformFormat = fmt;
		}

		if (was_open)
		{
			if (!CreateAudioTransform())
			{
				return false;
			}
		}

		m_Transform = true;

		return true;
	}

	AudioFormat AudioSourceReader::GetSampleFormat() const noexcept
	{
		if (m_Transform) return m_AudioFormatData.WithSharedLock()->TransformFormat;
		else return m_AudioFormatData.WithSharedLock()->ReaderFormat;
	}

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

	bool AudioSourceReader::OnOpen() noexcept
	{
		if (m_Transform)
		{
			return CreateAudioTransform();
		}

		return true;
	}

	void AudioSourceReader::OnClose() noexcept
	{
		CloseAudioTransform();

		auto format_data = m_AudioFormatData.WithUniqueLock();
		format_data->ReaderFormat = {};
		format_data->TransformFormat = {};

		m_Transform = false;
	}

	Result<> AudioSourceReader::OnMediaTypeChanged(IMFMediaType* media_type) noexcept
	{
		auto format_data = m_AudioFormatData.WithUniqueLock();

		auto hr = media_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &format_data->ReaderFormat.NumChannels);
		if (SUCCEEDED(hr))
		{
			hr = media_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &format_data->ReaderFormat.SamplesPerSecond);
			if (SUCCEEDED(hr))
			{
				hr = media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &format_data->ReaderFormat.BitsPerSample);
				if (SUCCEEDED(hr))
				{
					hr = media_type->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &format_data->ReaderFormat.AvgBytesPerSecond);
					if (SUCCEEDED(hr))
					{
						hr = media_type->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &format_data->ReaderFormat.BlockAlignment);
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

	IMFSample* AudioSourceReader::TransformSample(IMFSample* pSample) noexcept
	{
		if (!m_Transform) return pSample;

		auto trf = m_AudioTransform.WithUniqueLock();
		if (trf->InAudioResampler.Resample(pSample, trf->m_OutputSample))
		{
			return trf->m_OutputSample;
		}

		return nullptr;
	}

	bool AudioSourceReader::CreateAudioTransform() noexcept
	{
		auto trf = m_AudioTransform.WithUniqueLock();
		auto format_data = m_AudioFormatData.WithSharedLock();

		if (trf->InAudioResampler.Create(format_data->ReaderFormat, format_data->TransformFormat))
		{
			auto result = CaptureDevices::CreateMediaSample(format_data->TransformFormat.AvgBytesPerSecond);
			if (result.Succeeded())
			{
				trf->m_OutputSample = result.GetValue();

				return true;
			}
		}

		return false;
	}

	void AudioSourceReader::CloseAudioTransform() noexcept
	{
		auto trf = m_AudioTransform.WithUniqueLock();
		trf->InAudioResampler.Close();

		SafeRelease(&trf->m_OutputSample);
	}
}