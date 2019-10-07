// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "VideoCompressor.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	VideoCompressor::VideoCompressor(const Type type) noexcept : m_Type(type)
	{
		DiscardReturnValue(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	}

	VideoCompressor::~VideoCompressor()
	{
		Close();

		CoUninitialize();
	}

	bool VideoCompressor::Create(const Size width, const Size height, const GUID video_format) noexcept
	{
		// Close if failed
		auto sg = MakeScopeGuard([&]() noexcept { Close(); });

		HRESULT hr{ E_FAIL };

		switch (m_Type)
		{
			case Type::Encoder:
				hr = CoCreateInstance(CLSID_CMSH264EncoderMFT, nullptr, CLSCTX_ALL,
									  IID_IMFTransform, (void**)&m_IMFTransform);
				break;
			case Type::Decoder:
				hr = CoCreateInstance(CLSID_CMSH264DecoderMFT, nullptr, CLSCTX_ALL,
									  IID_IMFTransform, (void**)&m_IMFTransform);
				break;
			default:
				assert(false);
				break;
		}

		if (SUCCEEDED(hr))
		{
			if (CreateMediaTypes(width, height, video_format))
			{
				if (SUCCEEDED(m_IMFTransform->SetInputType(0, m_InputMediaType, 0)) &&
					SUCCEEDED(m_IMFTransform->SetOutputType(0, m_OutputMediaType, 0)))
				{
					DWORD status{ 0 };

					if (SUCCEEDED(m_IMFTransform->GetInputStatus(0, &status)))
					{
						if (status == MFT_INPUT_STATUS_ACCEPT_DATA)
						{
							if (SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL)) &&
								SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL)) &&
								SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL)))
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

	void VideoCompressor::Close() noexcept
	{
		m_Open = false;

		SafeRelease(&m_IMFTransform);
		SafeRelease(&m_InputMediaType);
		SafeRelease(&m_OutputMediaType);
	}

	bool VideoCompressor::Resample(IMFSample* in_sample, IMFSample* out_sample) noexcept
	{
		auto hr = m_IMFTransform->ProcessInput(0, in_sample, 0);
		if (SUCCEEDED(hr))
		{
			DWORD flags{ 0 };

			hr = m_IMFTransform->GetOutputStatus(&flags);
			if (SUCCEEDED(hr))
			{
				if (flags == MFT_OUTPUT_STATUS_SAMPLE_READY)
				{
					MFT_OUTPUT_STREAM_INFO sinfo;

					hr = m_IMFTransform->GetOutputStreamInfo(0, &sinfo);
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
								DWORD maxlen{ 0 };
								hr = out_buffer->GetMaxLength(&maxlen);
								if (SUCCEEDED(hr))
								{
									if (sinfo.cbSize <= maxlen)
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
					}
				}
			}
		}

		return false;
	}

	bool VideoCompressor::CreateMediaTypes(const Size width, const Size height, const GUID video_format) noexcept
	{
		if (SUCCEEDED(MFCreateMediaType(&m_InputMediaType)) &&
			SUCCEEDED(MFCreateMediaType(&m_OutputMediaType)))
		{
			auto intype = m_InputMediaType;
			auto outtype = m_OutputMediaType;

			if (m_Type == Type::Decoder)
			{
				intype = m_OutputMediaType;
				outtype = m_InputMediaType;
			}

			if (SUCCEEDED(intype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
				SUCCEEDED(intype->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV)) &&
				SUCCEEDED(MFSetAttributeSize(intype, MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height))) &&
				SUCCEEDED(MFSetAttributeRatio(intype, MF_MT_FRAME_RATE, 30, 1)) &&
				SUCCEEDED(MFSetAttributeRatio(intype, MF_MT_PIXEL_ASPECT_RATIO, 1, 1)) &&
				SUCCEEDED(intype->SetUINT32(MF_MT_INTERLACE_MODE, 2)))
			{
				if (SUCCEEDED(outtype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
					SUCCEEDED(outtype->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264)) &&
					SUCCEEDED(outtype->SetUINT32(MF_MT_AVG_BITRATE, 240000)) &&
					SUCCEEDED(MFSetAttributeSize(outtype, MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height))) &&
					SUCCEEDED(MFSetAttributeRatio(outtype, MF_MT_FRAME_RATE, 30, 1)) &&
					SUCCEEDED(MFSetAttributeRatio(outtype, MF_MT_PIXEL_ASPECT_RATIO, 1, 1)) &&
					SUCCEEDED(outtype->SetUINT32(MF_MT_INTERLACE_MODE, 2)) &&
					SUCCEEDED(outtype->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE)))
				{
					return true;
				}
			}
		}

		return false;
	}
}