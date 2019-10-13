// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "VideoResizer.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	VideoResizer::VideoResizer() noexcept
	{}

	VideoResizer::~VideoResizer()
	{
		Close();
	}

	bool VideoResizer::Create(const VideoFormat& in_video_format, const Size out_width, const Size out_height) noexcept
	{
		assert(!IsOpen());

		// Close if failed
		auto sg = MakeScopeGuard([&]() noexcept { Close(); });

		auto hr = CoCreateInstance(CLSID_CResizerDMO, nullptr, CLSCTX_ALL,
								   IID_IMFTransform, (void**)&m_IMFTransform);
		if (SUCCEEDED(hr))
		{
			if (SUCCEEDED(MFCreateMediaType(&m_InputMediaType)) &&
				SUCCEEDED(m_InputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
				SUCCEEDED(m_InputMediaType->SetGUID(MF_MT_SUBTYPE, CaptureDevices::GetMFVideoFormat(in_video_format.Format))) &&
				SUCCEEDED(MFSetAttributeSize(m_InputMediaType, MF_MT_FRAME_SIZE, in_video_format.Width, in_video_format.Height)))
			{
				if (SUCCEEDED(MFCreateMediaType(&m_OutputMediaType)) &&
					SUCCEEDED(m_OutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
					SUCCEEDED(m_OutputMediaType->SetGUID(MF_MT_SUBTYPE, CaptureDevices::GetMFVideoFormat(in_video_format.Format))) &&
					SUCCEEDED(MFSetAttributeSize(m_OutputMediaType, MF_MT_FRAME_SIZE, static_cast<UINT32>(out_width), static_cast<UINT32>(out_height))))
				{
					if (SUCCEEDED(m_IMFTransform->SetInputType(0, m_InputMediaType, 0)) &&
						SUCCEEDED(m_IMFTransform->SetOutputType(0, m_OutputMediaType, 0)))
					{
						m_OutputFormat = in_video_format;
						m_OutputFormat.Width = static_cast<UINT32>(out_width);
						m_OutputFormat.Height = static_cast<UINT32>(out_height);

						auto result = CaptureDevices::CreateMediaSample(CaptureDevices::GetImageSize(m_OutputFormat));
						if (result.Succeeded())
						{
							m_OutputSample = result.GetValue();

							sg.Deactivate();

							m_Open = true;

							return true;
						}
					}
				}
			}
		}

		return false;
	}

	void VideoResizer::Close() noexcept
	{
		m_Open = false;

		m_OutputFormat = {};

		SafeRelease(&m_IMFTransform);
		SafeRelease(&m_InputMediaType);
		SafeRelease(&m_OutputMediaType);
		SafeRelease(&m_OutputSample);
	}

	bool VideoResizer::Resize(IMFSample* in_sample, IMFSample* out_sample) noexcept
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

				DWORD maxlen{ 0 };

				hr = out_buffer->GetMaxLength(&maxlen);
				if (SUCCEEDED(hr))
				{
					// Output buffer should be large enough to hold output frame
					assert(maxlen >= CaptureDevices::GetImageSize(m_OutputFormat));

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
		}

		return false;
	}

	IMFSample* VideoResizer::Resize(IMFSample* in_sample) noexcept
	{
		if (Resize(in_sample, m_OutputSample))
		{
			return m_OutputSample;
		}

		return nullptr;
	}
}