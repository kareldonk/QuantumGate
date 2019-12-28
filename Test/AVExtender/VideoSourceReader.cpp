// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "VideoSourceReader.h"

#include <Common\Util.h>

namespace QuantumGate::AVExtender
{
	VideoSourceReader::VideoSourceReader() noexcept : SourceReader(CaptureDevice::Type::Video)
	{}

	VideoSourceReader::~VideoSourceReader()
	{
		CloseVideoTransform();
	}

	void VideoSourceReader::SetPreferredSize(const UInt16 width, const UInt16 height) noexcept
	{
		m_PreferredWidth = width;
		m_PreferredHeight = height;
	}

	bool VideoSourceReader::SetSampleSize(const UInt16 width, const UInt16 height) noexcept
	{
		bool was_open{ false };

		if (IsOpen())
		{
			was_open = true;

			CloseVideoTransform();
		}

		{
			auto format_data = m_VideoFormatData.WithUniqueLock();
			format_data->TransformWidth = width;
			format_data->TransformHeight = height;
		}

		if (was_open)
		{
			if (!CreateVideoTransform())
			{
				return false;
			}
		}

		m_Transform = true;

		return true;
	}

	VideoFormat VideoSourceReader::GetSampleFormat() const noexcept
	{
		auto format_data = m_VideoFormatData.WithSharedLock();

		if (m_Transform)
		{
			VideoFormat fmt;
			fmt = format_data->ReaderFormat;
			fmt.Width = static_cast<UInt32>(format_data->TransformWidth);
			fmt.Height = static_cast<UInt32>(format_data->TransformHeight);

			return fmt;
		}

		return format_data->ReaderFormat;
	}

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

	bool VideoSourceReader::OnOpen() noexcept
	{
		if (m_Transform)
		{
			return CreateVideoTransform();
		}

		return true;
	}

	void VideoSourceReader::OnClose() noexcept
	{
		CloseVideoTransform();

		auto format_data = m_VideoFormatData.WithUniqueLock();
		format_data->TransformWidth = 0;
		format_data->TransformHeight = 0;
		format_data->ReaderFormat = {};

		m_Transform = false;
	}

	Result<> VideoSourceReader::OnMediaTypeChanged(IMFMediaType* media_type) noexcept
	{
		auto format_data = m_VideoFormatData.WithUniqueLock();

		// Get width and height
		auto hr = MFGetAttributeSize(media_type, MF_MT_FRAME_SIZE,
									 &format_data->ReaderFormat.Width, &format_data->ReaderFormat.Height);
		if (SUCCEEDED(hr))
		{
			LONG stride{ 0 };

			// Get the stride for this format so we can calculate the number of bytes per pixel
			if (GetDefaultStride(media_type, &stride))
			{
				format_data->ReaderFormat.BytesPerPixel = std::abs(stride) / format_data->ReaderFormat.Width;

				GUID subtype{ GUID_NULL };

				hr = media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
				if (SUCCEEDED(hr))
				{
					format_data->ReaderFormat.Format = CaptureDevices::GetVideoFormat(subtype);

					return AVResultCode::Succeeded;
				}
			}
		}

		return AVResultCode::Failed;
	}

	IMFSample* VideoSourceReader::TransformSample(IMFSample* pSample) noexcept
	{
		if (!m_Transform) return pSample;

		auto trf = m_VideoTransform.WithUniqueLock();
		if (trf->InVideoResampler.Resample(pSample, trf->m_OutputSample1))
		{
			auto rsample = trf->VideoResizer.Resize(trf->m_OutputSample1);
			if (rsample != nullptr)
			{
				if (trf->OutVideoResampler.Resample(rsample, trf->m_OutputSample2))
				{
					return trf->m_OutputSample2;
				}
			}
		}

		return nullptr;
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

	Result<std::pair<IMFMediaType*, GUID>> VideoSourceReader::GetSupportedMediaType(IMFSourceReader* source_reader,
																					const DWORD stream_index,
																					const std::vector<GUID>& supported_formats) noexcept
	{
		assert(source_reader != nullptr);

		LogDbg(L"Supported video media formats: %s",
			   CaptureDevices::GetSupportedMediaTypes(source_reader, stream_index).c_str());

		struct VideoRes
		{
			DWORD Idx{ 0 };
			UINT32 Width{ 0 };
			UINT32 Height{ 0 };
		};

		// Try to find a suitable output type
		for (const GUID& guid : supported_formats)
		{
			Vector<VideoRes> resolutions;

			for (DWORD i = 0; ; ++i)
			{
				IMFMediaType* media_type{ nullptr };

				auto hr = source_reader->GetNativeMediaType(stream_index, i, &media_type);
				if (SUCCEEDED(hr))
				{
					// Release media type when we exit this scope
					auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&media_type); });

					GUID subtype{ GUID_NULL };

					hr = media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
					if (SUCCEEDED(hr))
					{
						if (subtype == guid)
						{
							UINT32 width{ 0 };
							UINT32 height{ 0 };

							hr = MFGetAttributeSize(media_type, MF_MT_FRAME_SIZE, &width, &height);
							if (SUCCEEDED(hr))
							{
								auto& vidtype = resolutions.emplace_back();
								vidtype.Idx = i;
								vidtype.Width = width;
								vidtype.Height = height;
							}
						}
					}
				}
				else break;
			}

			if (!resolutions.empty())
			{
				// Sort based on height from small to big
				std::sort(resolutions.begin(), resolutions.end(),
						  [](const VideoRes& a, const VideoRes& b)
				{
					return a.Height < b.Height;
				});
				
				UINT32 height = resolutions[0].Height;

				for (auto& res : resolutions)
				{
					if (res.Height > m_PreferredHeight)
					{
						break;
					}
					else height = res.Height;
				}

				Vector<VideoRes> resolutions2;
				std::copy_if(resolutions.begin(), resolutions.end(), std::back_inserter(resolutions2),
							 [&](const VideoRes& a)
				{
					return a.Height == height;
				});

				if (!resolutions2.empty())
				{
					// Sort based on width from small to big
					std::sort(resolutions2.begin(), resolutions2.end(),
							  [](const VideoRes& a, const VideoRes& b)
					{
						return a.Width < b.Width;
					});

					DWORD idx = resolutions2[0].Idx;

					for (auto& res : resolutions2)
					{
						if (res.Width > m_PreferredWidth)
						{
							break;
						}
						else idx = res.Idx;
					}

					IMFMediaType* media_type{ nullptr };

					auto hr = source_reader->GetNativeMediaType(stream_index, idx, &media_type);
					if (SUCCEEDED(hr))
					{
						GUID subtype{ GUID_NULL };

						hr = media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
						if (SUCCEEDED(hr))
						{
							// We'll return the media type so
							// the caller should release it
							return std::make_pair(media_type, subtype);
						}
					}
				}
			}
		}

		return AVResultCode::FailedNoSupportedVideoMediaType;
	}

	bool VideoSourceReader::CreateVideoTransform() noexcept
	{
		auto trf = m_VideoTransform.WithUniqueLock();
		auto format_data = m_VideoFormatData.WithSharedLock();

		if (trf->InVideoResampler.Create(format_data->ReaderFormat.Width, format_data->ReaderFormat.Height,
										 CaptureDevices::GetMFVideoFormat(format_data->ReaderFormat.Format), MFVideoFormat_YV12))
		{
			auto result = CaptureDevices::CreateMediaSample(CaptureDevices::GetImageSize(trf->InVideoResampler.GetOutputFormat()));
			if (result.Succeeded())
			{
				trf->m_OutputSample1 = result.GetValue();

				if (trf->VideoResizer.Create(trf->InVideoResampler.GetOutputFormat(),
											 format_data->TransformWidth, format_data->TransformHeight))
				{
					if (trf->OutVideoResampler.Create(format_data->TransformWidth, format_data->TransformHeight, MFVideoFormat_YV12,
													  CaptureDevices::GetMFVideoFormat(format_data->ReaderFormat.Format)))
					{
						auto result2 = CaptureDevices::CreateMediaSample(CaptureDevices::GetImageSize(trf->OutVideoResampler.GetOutputFormat()));
						if (result2.Succeeded())
						{
							trf->m_OutputSample2 = result2.GetValue();

							return true;
						}
					}
				}
			}
		}

		return false;
	}

	void VideoSourceReader::CloseVideoTransform() noexcept
	{
		auto trf = m_VideoTransform.WithUniqueLock();
		trf->InVideoResampler.Close();
		trf->VideoResizer.Close();
		trf->OutVideoResampler.Close();

		SafeRelease(&trf->m_OutputSample1);
		SafeRelease(&trf->m_OutputSample2);
	}
}