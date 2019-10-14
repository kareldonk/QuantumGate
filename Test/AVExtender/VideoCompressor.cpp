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

	VideoCompressor::VideoCompressor(const Type type) noexcept :
		Compressor(type, CLSID_CMSH264EncoderMFT, CLSID_CMSH264DecoderMFT)
	{}

	VideoCompressor::~VideoCompressor()
	{}

	void VideoCompressor::SetFormat(const UInt16 width, const UInt16 height, const GUID& video_format) noexcept
	{
		m_Width = width;
		m_Height = height;
		m_VideoFormat = video_format;
	}

	void VideoCompressor::OnClose() noexcept
	{
		SafeRelease(&m_ICodecAPI);
	}

	UInt64 VideoCompressor::GetDuration(const Size sample_size) noexcept
	{
		return static_cast<LONGLONG>((1.0 / static_cast<double>(m_FrameRate)) * 10'000'000.0);
	}

	bool VideoCompressor::OnCreateMediaTypes(IMFMediaType* input_type, IMFMediaType* output_type) noexcept
	{
		auto intype = input_type;
		auto outtype = output_type;

		if (GetType() == Type::Decoder)
		{
			intype = output_type;
			outtype = input_type;
		}

		if (SUCCEEDED(intype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
			SUCCEEDED(intype->SetGUID(MF_MT_SUBTYPE, m_VideoFormat)) &&
			SUCCEEDED(MFSetAttributeSize(intype, MF_MT_FRAME_SIZE, static_cast<UINT32>(m_Width), static_cast<UINT32>(m_Height))) &&
			SUCCEEDED(MFSetAttributeRatio(intype, MF_MT_FRAME_RATE, m_FrameRate, 1)) &&
			SUCCEEDED(MFSetAttributeRatio(intype, MF_MT_PIXEL_ASPECT_RATIO, 1, 1)) &&
			SUCCEEDED(intype->SetUINT32(MF_MT_INTERLACE_MODE, 2)))
		{
			if (SUCCEEDED(outtype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
				SUCCEEDED(outtype->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264)) &&
				SUCCEEDED(outtype->SetUINT32(MF_MT_AVG_BITRATE, 240000)) &&
				SUCCEEDED(MFSetAttributeSize(outtype, MF_MT_FRAME_SIZE, static_cast<UINT32>(m_Width), static_cast<UINT32>(m_Height))) &&
				SUCCEEDED(MFSetAttributeRatio(outtype, MF_MT_FRAME_RATE, m_FrameRate, 1)) &&
				SUCCEEDED(MFSetAttributeRatio(outtype, MF_MT_PIXEL_ASPECT_RATIO, 1, 1)) &&
				SUCCEEDED(outtype->SetUINT32(MF_MT_INTERLACE_MODE, 2)) &&
				SUCCEEDED(outtype->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE)))
			{
				return true;
			}
		}

		return false;
	}

	bool VideoCompressor::OnSetMediaTypes(IMFTransform* transform, IMFMediaType* input_type, IMFMediaType* output_type) noexcept
	{
		auto hr = transform->QueryInterface(&m_ICodecAPI);
		if (SUCCEEDED(hr))
		{
			switch (GetType())
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
							hr = transform->SetOutputType(0, output_type, 0);
							if (SUCCEEDED(hr))
							{
								hr = transform->SetInputType(0, input_type, 0);
							}
						}
					}
					break;
				case Type::Decoder:
					// Need to set input type first and then the output type
					hr = transform->SetInputType(0, input_type, 0);
					if (SUCCEEDED(hr))
					{
						hr = transform->SetOutputType(0, output_type, 0);
					}
					break;
				default:
					assert(false);
					break;
			}

			if (SUCCEEDED(hr))
			{
				return true;
			}
		}

		return false;
	}
}