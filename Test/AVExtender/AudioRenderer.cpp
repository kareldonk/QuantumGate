// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AudioRenderer.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	AudioRenderer::AudioRenderer() noexcept
	{
		DiscardReturnValue(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	}

	AudioRenderer::~AudioRenderer()
	{
		Close();

		CoUninitialize();
	}

	bool AudioRenderer::Create(const AudioSettings& input_audio_settings) noexcept
	{
		// Close if failed
		auto sg = MakeScopeGuard([&]() noexcept { Close(); });

		auto hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
								   IID_IMMDeviceEnumerator, (void**)&m_Enumerator);
		if (SUCCEEDED(hr))
		{
			hr = m_Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_Device);

			if (SUCCEEDED(hr))
			{
				hr = m_Device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&m_AudioClient);

				if (SUCCEEDED(hr))
				{
					WAVEFORMATEX wfmt{ 0 };

					if (GetSupportedMixFormat(input_audio_settings, wfmt))
					{
						m_OutputAudioSettings.NumChannels = wfmt.nChannels;
						m_OutputAudioSettings.SamplesPerSecond = wfmt.nSamplesPerSec;
						m_OutputAudioSettings.BlockAlignment = wfmt.nBlockAlign;
						m_OutputAudioSettings.BitsPerSample = wfmt.wBitsPerSample;
						m_OutputAudioSettings.AvgBytesPerSecond = wfmt.nAvgBytesPerSec;

						hr = m_AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10'000'000, 0, &wfmt, nullptr);
						if (SUCCEEDED(hr))
						{
							// Get the actual size of the allocated buffer
							hr = m_AudioClient->GetBufferSize(&m_BufferFrameCount);
							if (SUCCEEDED(hr))
							{
								// Calculate the actual duration of the allocated buffer
								m_BufferDuration = 10'000'000 * m_BufferFrameCount / wfmt.nSamplesPerSec;

								hr = m_AudioClient->GetService(IID_IAudioRenderClient, (void**)&m_RenderClient);
								if (SUCCEEDED(hr))
								{
									if (m_AudioSampler.Create(input_audio_settings, m_OutputAudioSettings))
									{
										sg.Deactivate();

										return true;
									}
								}
							}
						}
					}
				}
			}
		}

		return false;
	}

	void AudioRenderer::Close() noexcept
	{
		m_AudioSampler.Close();

		SafeRelease(&m_Enumerator);
		SafeRelease(&m_Device);
		SafeRelease(&m_AudioClient);
		SafeRelease(&m_RenderClient);
	}

	bool AudioRenderer::Play() noexcept
	{
		const auto hr = m_AudioClient->Start();
		if (SUCCEEDED(hr))
		{
			return true;
		}

		return false;
	}

	bool AudioRenderer::Render(const Byte* src_data, const Size src_len) noexcept
	{
		UINT32 src_frames = static_cast<UINT32>(src_len / m_OutputAudioSettings.BlockAlignment);

		UINT32 padding{ 0 };
		
		// See how much buffer space is available
		auto hr = m_AudioClient->GetCurrentPadding(&padding);
		if (SUCCEEDED(hr))
		{
			const auto available = m_BufferFrameCount - padding;
			if (available < src_frames)
			{
				src_frames = available;
			}

			BYTE* data{ nullptr };

			// Grab all the available space in the shared buffer
			hr = m_RenderClient->GetBuffer(src_frames, &data);
			if (SUCCEEDED(hr))
			{
				auto len = src_frames * m_OutputAudioSettings.BlockAlignment;

				memcpy(data, src_data, len);

				hr = m_RenderClient->ReleaseBuffer(src_frames, 0);
				if (SUCCEEDED(hr))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool AudioRenderer::GetSupportedMixFormat(const AudioSettings& audio_settings, WAVEFORMATEX& wfmt) noexcept
	{
		WAVEFORMATEX iwfmt{ 0 };
		iwfmt.wFormatTag = WAVE_FORMAT_PCM;
		iwfmt.nChannels = audio_settings.NumChannels;
		iwfmt.nBlockAlign = audio_settings.BlockAlignment;
		iwfmt.wBitsPerSample = audio_settings.BitsPerSample;
		iwfmt.nSamplesPerSec = audio_settings.SamplesPerSecond;
		iwfmt.nAvgBytesPerSec = audio_settings.AvgBytesPerSecond;
		iwfmt.cbSize = 0;

		WAVEFORMATEX* owfmt{ nullptr };

		// Release when we exit this scope
		const auto sg = MakeScopeGuard([&]() noexcept
		{
			if (owfmt != nullptr)
			{
				CoTaskMemFree(owfmt);
				owfmt = nullptr;
			}
		});

		// First check if the requested format is supported
		auto hr = m_AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &iwfmt, &owfmt);
		if (hr == S_FALSE)
		{
			// Get default mix format
			hr = m_AudioClient->GetMixFormat(&owfmt);
			if (FAILED(hr))
			{
				return false;
			}
			else wfmt = *owfmt;
		}
		else wfmt = iwfmt;

		return true;
	}
}