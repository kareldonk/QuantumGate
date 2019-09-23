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

	bool AudioRenderer::Create(const AudioFormat& input_audio_settings) noexcept
	{
		// Close if failed
		auto sg = MakeScopeGuard([&]() noexcept { Close(); });

		auto hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
								   IID_IMMDeviceEnumerator, reinterpret_cast<void**>(&m_Enumerator));
		if (SUCCEEDED(hr))
		{
			hr = m_Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_Device);
			if (SUCCEEDED(hr))
			{
				hr = m_Device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&m_AudioClient));
				if (SUCCEEDED(hr))
				{
					WAVEFORMATEXTENSIBLE wfmt{ 0 };

					if (GetSupportedMixFormat(input_audio_settings, wfmt))
					{
						m_OutputFormat.NumChannels = wfmt.Format.nChannels;
						m_OutputFormat.SamplesPerSecond = wfmt.Format.nSamplesPerSec;
						m_OutputFormat.BlockAlignment = wfmt.Format.nBlockAlign;
						m_OutputFormat.BitsPerSample = wfmt.Format.wBitsPerSample;
						m_OutputFormat.AvgBytesPerSecond = wfmt.Format.nAvgBytesPerSec;

						hr = m_AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10'000'000, 0,
													   reinterpret_cast<WAVEFORMATEX*>(&wfmt), nullptr);
						if (SUCCEEDED(hr))
						{
							// Get the actual size of the allocated buffer
							hr = m_AudioClient->GetBufferSize(&m_BufferFrameCount);
							if (SUCCEEDED(hr))
							{
								// Calculate the actual duration of the allocated buffer
								m_BufferDuration = 10'000'000 * m_BufferFrameCount / wfmt.Format.nSamplesPerSec;

								hr = m_AudioClient->GetService(IID_IAudioRenderClient,
															   reinterpret_cast<void**>(&m_RenderClient));
								if (SUCCEEDED(hr))
								{
									auto result = CaptureDevices::CreateMediaSample(m_OutputFormat.AvgBytesPerSecond);
									if (result.Succeeded())
									{
										m_OutputSample = result->first;
										m_OutputBuffer = result->second;

										if (m_AudioResampler.Create(input_audio_settings, m_OutputFormat))
										{
											sg.Deactivate();

											m_Open = true;

											return true;
										}
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
		m_Open = false;

		m_AudioResampler.Close();

		SafeRelease(&m_Enumerator);
		SafeRelease(&m_Device);
		SafeRelease(&m_AudioClient);
		SafeRelease(&m_RenderClient);
		SafeRelease(&m_OutputSample);
		SafeRelease(&m_OutputBuffer);
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

	bool AudioRenderer::Render(const BufferView in_data) noexcept
	{
		// Nothing to render
		if (in_data.GetSize() == 0) return true;

		if (m_AudioResampler.Resample(in_data, m_OutputSample))
		{
			BYTE* outptr{ nullptr };
			DWORD outcurl{ 0 };

			auto hr = m_OutputBuffer->Lock(&outptr, nullptr, &outcurl);
			if (SUCCEEDED(hr))
			{
				UINT32 out_frames = { outcurl / m_OutputFormat.BlockAlignment };
				UINT32 padding{ 0 };

				// See how much buffer space is available
				hr = m_AudioClient->GetCurrentPadding(&padding);
				if (SUCCEEDED(hr))
				{
					const auto available_frames = m_BufferFrameCount - padding;
					if (available_frames < out_frames)
					{
						out_frames = available_frames;
					}

					BYTE* data{ nullptr };

					// Grab all the available space in the shared buffer
					hr = m_RenderClient->GetBuffer(out_frames, &data);
					if (SUCCEEDED(hr))
					{
						const auto len = out_frames * m_OutputFormat.BlockAlignment;

						std::memcpy(data, outptr, len);

						hr = m_RenderClient->ReleaseBuffer(out_frames, 0);
						if (SUCCEEDED(hr))
						{
							hr = m_OutputBuffer->Unlock();
							if (SUCCEEDED(hr))
							{
								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}

	bool AudioRenderer::GetSupportedMixFormat(const AudioFormat& audio_settings, WAVEFORMATEXTENSIBLE& wfmt) noexcept
	{
		WAVEFORMATEXTENSIBLE iwfmt{ 0 };
		iwfmt.Format.wFormatTag = WAVE_FORMAT_PCM;
		iwfmt.Format.nChannels = audio_settings.NumChannels;
		iwfmt.Format.nBlockAlign = audio_settings.BlockAlignment;
		iwfmt.Format.wBitsPerSample = audio_settings.BitsPerSample;
		iwfmt.Format.nSamplesPerSec = audio_settings.SamplesPerSecond;
		iwfmt.Format.nAvgBytesPerSec = audio_settings.AvgBytesPerSecond;
		iwfmt.Format.cbSize = 0;

		WAVEFORMATEXTENSIBLE* owfmt{ nullptr };

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
		auto hr = m_AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
												   reinterpret_cast<WAVEFORMATEX*>(&iwfmt),
												   reinterpret_cast<WAVEFORMATEX**>(&owfmt));
		if (hr == S_FALSE)
		{
			if (owfmt == nullptr)
			{
				// Get default mix format
				hr = m_AudioClient->GetMixFormat(reinterpret_cast<WAVEFORMATEX**>(&owfmt));
				if (FAILED(hr))
				{
					return false;
				}
			}

			wfmt = *owfmt;
		}
		else wfmt = iwfmt;

		return true;
	}
}