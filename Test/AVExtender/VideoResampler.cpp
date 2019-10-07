// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "VideoResampler.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	VideoResampler::VideoResampler() noexcept
	{
		DiscardReturnValue(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	}

	VideoResampler::~VideoResampler()
	{
		Close();

		CoUninitialize();
	}

	bool VideoResampler::Create(const Size width, const Size height,
								const GUID in_video_format, const GUID out_video_format) noexcept
	{
		// Close if failed
		auto sg = MakeScopeGuard([&]() noexcept { Close(); });

		auto hr = CoCreateInstance(CLSID_CColorConvertDMO, nullptr, CLSCTX_ALL,
								   IID_IMFTransform, (void**)&m_IMFTransform);
		if (SUCCEEDED(hr))
		{
			hr = m_IMFTransform->QueryInterface(&m_IMediaObject);
			if (SUCCEEDED(hr))
			{
				auto itype = GetMediaType(width, height, in_video_format);
				auto otype = GetMediaType(width, height, out_video_format);

				// Set input type
				hr = m_IMediaObject->SetInputType(0, &itype.DMOMediaType, 0);
				if (SUCCEEDED(hr))
				{
					m_InputFormat = itype.GetVideoFormat();

					// Set output type
					hr = m_IMediaObject->SetOutputType(0, &otype.DMOMediaType, 0);
					if (SUCCEEDED(hr))
					{
						m_OutputFormat = otype.GetVideoFormat();

						sg.Deactivate();

						m_Open = true;

						return true;
					}
				}
			}
		}

		return false;
	}

	VideoResampler::DMOData VideoResampler::GetMediaType(const Size width, const Size height, const GUID type) const noexcept
	{
		if (type == MFVideoFormat_RGB24)
		{
			return DMOData(width, height, 24, BI_RGB, MFVideoFormat_RGB24, MEDIASUBTYPE_RGB24);
		}
		else if (type == MFVideoFormat_YV12)
		{
			return DMOData(width, height, 12, MAKEFOURCC('Y', 'V', '1', '2'), MFVideoFormat_YV12, MEDIASUBTYPE_YV12);
		}
		else if (type == MFVideoFormat_NV12)
		{
			return DMOData(width, height, 12, MAKEFOURCC('N', 'V', '1', '2'), MFVideoFormat_NV12, MEDIASUBTYPE_NV12);
		}
		else if (type == MFVideoFormat_RGB32)
		{
			return DMOData(width, height, 32, BI_RGB, MFVideoFormat_RGB32, MEDIASUBTYPE_RGB32);
		}
		else assert(false);

		return {};
	}

	void VideoResampler::Close() noexcept
	{
		m_Open = false;

		SafeRelease(&m_IMFTransform);
		SafeRelease(&m_IMediaObject);
	}

	bool VideoResampler::Resample(IMFSample* in_sample, IMFSample* out_sample) noexcept
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