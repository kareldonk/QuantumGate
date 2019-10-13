// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "VideoCompressor.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

#include <codecapi.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	VideoCompressor::VideoCompressor(const Type type) noexcept : m_Type(type)
	{}

	VideoCompressor::~VideoCompressor()
	{
		Close();
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
			hr = m_IMFTransform->QueryInterface(&m_ICodecAPI);
			if (SUCCEEDED(hr))
			{
				if (CreateMediaTypes(width, height, video_format))
				{
					switch (m_Type)
					{
						case Type::Encoder:
							VARIANT var;
							var.vt = VT_BOOL;
							var.boolVal = VARIANT_TRUE;

							hr = m_ICodecAPI->SetValue(&CODECAPI_AVLowLatencyMode, &var);
							if (SUCCEEDED(hr))
							{
								var.vt = VT_UI4;
								var.ulVal = 0;

								hr = m_ICodecAPI->SetValue(&CODECAPI_AVEncCommonQualityVsSpeed, &var);
								if (SUCCEEDED(hr))
								{
									// Need to set output type first and then the input type
									hr = m_IMFTransform->SetOutputType(0, m_OutputMediaType, 0);
									if (SUCCEEDED(hr))
									{
										hr = m_IMFTransform->SetInputType(0, m_InputMediaType, 0);
									}
								}
							}
							break;
						case Type::Decoder:
							// Need to set input type first and then the output type
							hr = m_IMFTransform->SetInputType(0, m_InputMediaType, 0);
							if (SUCCEEDED(hr))
							{
								hr = m_IMFTransform->SetOutputType(0, m_OutputMediaType, 0);
							}
							break;
						default:
							assert(false);
							break;
					}

					if (SUCCEEDED(hr))
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

		return false;
	}

	void VideoCompressor::Close() noexcept
	{
		m_Open = false;

		SafeRelease(&m_IMFTransform);
		SafeRelease(&m_ICodecAPI);
		SafeRelease(&m_InputMediaType);
		SafeRelease(&m_OutputMediaType);
	}

	bool VideoCompressor::AddInput(const UInt64 in_timestamp, const BufferView data) noexcept
	{
		auto result = CaptureDevices::CreateMediaSample(data.GetSize());
		if (result.Succeeded())
		{
			auto in_sample = result.GetValue();

			// Release in case of failure
			const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&in_sample); });

			const auto duration = static_cast<LONGLONG>((1.0 / static_cast<double>(m_FrameRate)) * 10000000.0);

			if (CaptureDevices::CopyToMediaSample(in_timestamp, duration, data, in_sample))
			{
				if (AddInput(in_sample))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool VideoCompressor::AddInput(IMFSample* in_sample) noexcept
	{
		auto hr = m_IMFTransform->ProcessInput(0, in_sample, 0);
		if (SUCCEEDED(hr))
		{
			return true;
		}

		return false;
	}

	IMFSample* VideoCompressor::GetOutput() noexcept
	{
		if (m_Type == Type::Encoder)
		{
			DWORD flags{ 0 };

			auto hr = m_IMFTransform->GetOutputStatus(&flags);
			if (SUCCEEDED(hr))
			{
				if (flags != MFT_OUTPUT_STATUS_SAMPLE_READY)
				{
					return nullptr;
				}
			}
			else return nullptr;
		}

		MFT_OUTPUT_STREAM_INFO osinfo{ 0 };

		auto hr = m_IMFTransform->GetOutputStreamInfo(0, &osinfo);
		if (SUCCEEDED(hr))
		{
			auto result = CaptureDevices::CreateMediaSample(osinfo.cbSize);
			if (result.Succeeded())
			{
				auto out_sample = result.GetValue();

				// Release in case of failure
				auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&out_sample); });

				MFT_OUTPUT_DATA_BUFFER output{ 0 };
				output.dwStreamID = 0;
				output.dwStatus = 0;
				output.pEvents = nullptr;
				output.pSample = out_sample;

				DWORD status{ 0 };

				hr = m_IMFTransform->ProcessOutput(0, 1, &output, &status);
				if (SUCCEEDED(hr))
				{
					sg.Deactivate();

					return out_sample;
				}
			}
		}

		return nullptr;
	}

	bool VideoCompressor::GetOutput(Buffer& buffer) noexcept
	{
		auto out_sample = GetOutput();
		if (out_sample != nullptr)
		{
			// Release when we exit this scope
			auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&out_sample); });

			return CaptureDevices::CopyFromMediaSample(out_sample, buffer);
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
				SUCCEEDED(intype->SetGUID(MF_MT_SUBTYPE, video_format)) &&
				SUCCEEDED(MFSetAttributeSize(intype, MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height))) &&
				SUCCEEDED(MFSetAttributeRatio(intype, MF_MT_FRAME_RATE, m_FrameRate, 1)) &&
				SUCCEEDED(MFSetAttributeRatio(intype, MF_MT_PIXEL_ASPECT_RATIO, 1, 1)) &&
				SUCCEEDED(intype->SetUINT32(MF_MT_INTERLACE_MODE, 2)))
			{
				if (SUCCEEDED(outtype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
					SUCCEEDED(outtype->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264)) &&
					SUCCEEDED(outtype->SetUINT32(MF_MT_AVG_BITRATE, 240000)) &&
					SUCCEEDED(MFSetAttributeSize(outtype, MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height))) &&
					SUCCEEDED(MFSetAttributeRatio(outtype, MF_MT_FRAME_RATE, m_FrameRate, 1)) &&
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