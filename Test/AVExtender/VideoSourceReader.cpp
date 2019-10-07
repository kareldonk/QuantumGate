// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "VideoSourceReader.h"

#include <Common\Util.h>

namespace QuantumGate::AVExtender
{
	VideoSourceReader::VideoSourceReader() noexcept : SourceReader(CaptureDevice::Type::Video)
	{}

	VideoSourceReader::~VideoSourceReader()
	{}

	STDMETHODIMP VideoSourceReader::QueryInterface(REFIID riid, void** ppvObject)
	{
		static const QITAB qit[] = { QITABENT(VideoSourceReader, IMFSourceReaderCallback), { 0 } };
		return QISearch(this, qit, riid, ppvObject);
	}

	STDMETHODIMP_(ULONG) VideoSourceReader::Release()
	{
		const ULONG count = InterlockedDecrement(&m_RefCount);
		if (count == 0)
		{
			delete this;
		}

		return count;
	}

	STDMETHODIMP_(ULONG) VideoSourceReader::AddRef()
	{
		return InterlockedIncrement(&m_RefCount);
	}

	Result<> VideoSourceReader::OnMediaTypeChanged(IMFMediaType* media_type) noexcept
	{
		auto video_settings = m_VideoFormat.WithUniqueLock();

		// Get width and height
		auto hr = MFGetAttributeSize(media_type, MF_MT_FRAME_SIZE,
									 &video_settings->Width, &video_settings->Height);
		if (SUCCEEDED(hr))
		{
			// Get the stride for this format so we can calculate the number of bytes per pixel
			if (GetDefaultStride(media_type, &video_settings->Stride))
			{
				video_settings->BytesPerPixel = std::abs(video_settings->Stride) / video_settings->Width;

				GUID subtype{ GUID_NULL };

				hr = media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
				if (SUCCEEDED(hr))
				{
					video_settings->Format = CaptureDevices::GetVideoFormat(subtype);

					return AVResultCode::Succeeded;
				}
			}
		}

		return AVResultCode::Failed;
	}

	Result<Size> VideoSourceReader::GetBufferSize(IMFMediaType* media_type) noexcept
	{
		const auto video_format = m_VideoFormat.WithSharedLock();

		return static_cast<Size>(video_format->Width) *
			static_cast<Size>(video_format->Height) *
			static_cast<Size>(video_format->BytesPerPixel);
	}

	// Calculates the default stride based on the format and size of the frames
	bool VideoSourceReader::GetDefaultStride(IMFMediaType* type, LONG* stride) const noexcept
	{
		UINT32 tstride{ 0 };

		// Try to get the default stride from the media type
		auto hr = type->GetUINT32(MF_MT_DEFAULT_STRIDE, &tstride);
		if (SUCCEEDED(hr))
		{
			*stride = tstride;
			return true;
		}
		else
		{
			GUID subtype{ GUID_NULL };
			UINT32 width{ 0 };
			UINT32 height{ 0 };

			// Obtain the subtype
			hr = type->GetGUID(MF_MT_SUBTYPE, &subtype);
			if (SUCCEEDED(hr))
			{
				// Obtain the width and height
				hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height);
				if (SUCCEEDED(hr))
				{
					// Calculate the stride based on the subtype and width
					hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, stride);
					if (SUCCEEDED(hr))
					{
						// Set the attribute so it can be read
						hr = type->SetUINT32(MF_MT_DEFAULT_STRIDE, *stride);
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
}