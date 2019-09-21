// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AudioSourceReader.h"

#include <Common\ScopeGuard.h>
#include <Common\Util.h>

namespace QuantumGate::AVExtender
{
	AudioSourceReader::AudioSourceReader() noexcept
		: SourceReader(CaptureDevice::Type::Audio)
	{}

	AudioSourceReader::~AudioSourceReader()
	{
		Close();
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

	Result<> AudioSourceReader::Open(const CaptureDevice& device) noexcept
	{
		IMFAttributes* attributes{ nullptr };

		// Create an attribute store to specify symbolic link
		auto hr = MFCreateAttributes(&attributes, 2);
		if (SUCCEEDED(hr))
		{
			// Release attributes when we exit this scope
			const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&attributes); });

			// Set source type attribute
			hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
			if (SUCCEEDED(hr))
			{
				// Set endpoint ID attribute
				hr = attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID, device.EndpointID);
				if (SUCCEEDED(hr))
				{
					auto source_reader_data = m_SourceReader.WithUniqueLock();

					hr = MFCreateDeviceSource(attributes, &source_reader_data->Source);
					if (SUCCEEDED(hr))
					{
						auto result = CreateSourceReader(*source_reader_data);
						if (result.Failed())
						{
							source_reader_data->Release();
						}

						return result;
					}
					else return AVResultCode::FailedCreateAudioDeviceSource;
				}
			}
		}

		return AVResultCode::Failed;
	}

	bool AudioSourceReader::IsOpen() noexcept
	{
		auto source_reader_data = m_SourceReader.WithSharedLock();
		return (source_reader_data->SourceReader != nullptr);
	}

	void AudioSourceReader::Close() noexcept
	{
		m_SourceReader.WithUniqueLock()->Release();
	}

	Result<> AudioSourceReader::CreateSourceReader(SourceReaderData& source_reader_data) noexcept
	{
		IMFAttributes* attributes{ nullptr };

		// Allocate attributes
		auto hr = MFCreateAttributes(&attributes, 2);
		if (SUCCEEDED(hr))
		{
			// Release attributes when we exit this scope
			const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&attributes); });

			hr = attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);
			if (SUCCEEDED(hr))
			{
				// Set the callback pointer
				hr = attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);
				if (SUCCEEDED(hr))
				{
					// Create the source reader
					hr = MFCreateSourceReaderFromMediaSource(source_reader_data.Source, attributes,
															 &source_reader_data.SourceReader);
					if (SUCCEEDED(hr))
					{
						auto result = GetSupportedMediaType(source_reader_data.SourceReader);
						if (result.Succeeded())
						{
							auto& media_type = result->first;
							auto& subtype = result->second;

							// Release media type when we exit this scope
							const auto sg2 = MakeScopeGuard([&]() noexcept { SafeRelease(&result->first); });

							if (SetMediaType(source_reader_data, media_type, subtype).Succeeded())
							{
								// Ask for the first sample
								hr = source_reader_data.SourceReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
																				 0, nullptr, nullptr, nullptr, nullptr);

								if (SUCCEEDED(hr)) return AVResultCode::Succeeded;
							}
						}
						else return AVResultCode::FailedNoSupportedVideoMediaType;
					}
				}
			}
		}

		return AVResultCode::Failed;
	}

	Result<std::pair<IMFMediaType*, GUID>> AudioSourceReader::GetSupportedMediaType(IMFSourceReader* source_reader) noexcept
	{
		assert(source_reader != nullptr);

		// Try to find a suitable output type
		for (DWORD i = 0; ; ++i)
		{
			IMFMediaType* media_type{ nullptr };

			auto hr = source_reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
														i, &media_type);
			if (SUCCEEDED(hr))
			{
				// Release media type when we exit this scope
				auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&media_type); });

				GUID subtype{ 0 };

				hr = media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
				if (SUCCEEDED(hr))
				{
					if (subtype == MFAudioFormat_Float)
					{
						// We'll return the media type so
						// the caller should release it
						sg.Deactivate();

						return std::make_pair(media_type, subtype);
					}
				}
			}
			else break;
		}

		return AVResultCode::Failed;
	}

	Result<> AudioSourceReader::SetMediaType(SourceReaderData& source_reader_data,
											 IMFMediaType* media_type, const GUID& subtype) noexcept
	{
		// Set media type
		auto hr = source_reader_data.SourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
																	   nullptr, media_type);
		if (SUCCEEDED(hr))
		{
			source_reader_data.AudioFormat = subtype;

			hr = media_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &source_reader_data.NumChannels);
			if (SUCCEEDED(hr))
			{
				hr = media_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &source_reader_data.SamplesPerSecond);
				if (SUCCEEDED(hr))
				{
					hr = media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &source_reader_data.BitsPerSample);
					if (SUCCEEDED(hr))
					{
						hr = media_type->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &source_reader_data.AvgBytesPerSecond);
						if (SUCCEEDED(hr))
						{
							hr = media_type->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &source_reader_data.BlockAlignment);
							if (SUCCEEDED(hr))
							{
								try
								{
									// Allocate a buffer for the raw sound data
									source_reader_data.RawData.Allocate(static_cast<Size>(source_reader_data.AvgBytesPerSecond));

									return AVResultCode::Succeeded;
								}
								catch (...)
								{
									return AVResultCode::FailedOutOfMemory;
								}
							}
						}
					}
				}
			}
		}

		return AVResultCode::Failed;
	}

	HRESULT AudioSourceReader::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
											LONGLONG llTimestamp, IMFSample* pSample)
	{
		HRESULT hr{ S_OK };

		auto source_reader_data = m_SourceReader.WithUniqueLock();

		if (FAILED(hrStatus)) hr = hrStatus;

		if (SUCCEEDED(hr))
		{
			if (pSample)
			{
				IMFMediaBuffer* media_buffer{ nullptr };

				// Get the audio buffer from the sample
				hr = pSample->GetBufferByIndex(0, &media_buffer);
				if (SUCCEEDED(hr))
				{
					// Release buffer when we exit this scope
					const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&media_buffer); });

					BYTE* data{ nullptr };
					DWORD data_length{ 0 };
					media_buffer->GetCurrentLength(&data_length);

					if (data_length > source_reader_data->RawData.GetSize())
					{
						data_length = static_cast<DWORD>(source_reader_data->RawData.GetSize());
					}

					hr = media_buffer->Lock(&data, nullptr, nullptr);
					if (SUCCEEDED(hr))
					{
						CopyMemory(source_reader_data->RawData.GetBytes(), data, data_length);
						media_buffer->Unlock();
					}
				}
			}
		}

		// Request the next sample
		if (SUCCEEDED(hr))
		{
			hr = source_reader_data->SourceReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
															  0, nullptr, nullptr, nullptr, nullptr);
		}

		return hr;
	}
}