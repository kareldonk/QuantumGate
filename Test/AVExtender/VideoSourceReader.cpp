// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "VideoSourceReader.h"

#include <shlwapi.h>

#include <Common\ScopeGuard.h>
#include <Common\Util.h>

namespace QuantumGate::AVExtender
{
	VideoSourceReader::VideoSourceReader() noexcept
	{
		DiscardReturnValue(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	}

	VideoSourceReader::~VideoSourceReader()
	{
		Close();
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

	Result<> VideoSourceReader::Open(const VideoCaptureDevice& device) noexcept
	{
		IMFAttributes* attributes{ nullptr };

		// Create an attribute store to specify symbolic link
		auto hr = MFCreateAttributes(&attributes, 2);
		if (SUCCEEDED(hr))
		{
			// Release attributes when we exit this scope
			const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&attributes); });

			// Set source type attribute
			hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
			if (SUCCEEDED(hr))
			{
				// Set symbolic link attribute
				hr = attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, device.SymbolicLink);
				if (SUCCEEDED(hr))
				{
					auto source_reader_data = m_SourceReader.WithUniqueLock();

					hr = MFCreateDeviceSource(attributes, &source_reader_data->Source);
					if (SUCCEEDED(hr))
					{
						auto result = CreateSourceReader(*source_reader_data);
						if (result.Failed())
						{
							Close();
						}

						return result;
					}
					else return AVResultCode::FailedCreateVideoDeviceSource;
				}
			}
		}

		return AVResultCode::Failed;
	}

	const bool VideoSourceReader::IsOpen() noexcept
	{
		auto source_reader_data = m_SourceReader.WithSharedLock();
		return (source_reader_data->SourceReader != nullptr);
	}

	void VideoSourceReader::Close() noexcept
	{
		m_SourceReader.WithUniqueLock()->Release();
	}

	Result<VideoCaptureDeviceVector> VideoSourceReader::EnumCaptureDevices() const noexcept
	{
		VideoCaptureDeviceVector ret_devices;

		auto result = GetVideoCaptureDevices();
		if (result.Succeeded())
		{
			const auto& device_count = result->first;
			const auto& devices = result->second;

			if (devices)
			{
				// Free memory when we leave this scope
				const auto sg = MakeScopeGuard([&]() noexcept
				{
					for (UINT32 x = 0; x < result->first; ++x)
					{
						SafeRelease(&result->second[x]);
					}

					CoTaskMemFree(result->second);
				});

				if (device_count > 0)
				{
					try
					{
						ret_devices.reserve(device_count);

						for (UINT32 x = 0; x < device_count; ++x)
						{
							auto& device_info = ret_devices.emplace_back();
							if (!GetVideoCaptureDeviceInfo(devices[x], device_info))
							{
								return AVResultCode::Failed;
							}
						}
					}
					catch (...)
					{
						return AVResultCode::FailedOutOfMemory;
					}
				}
			}

			return std::move(ret_devices);
		}
		else return AVResultCode::FailedGetVideoCaptureDevices;

		return AVResultCode::Failed;
	}

	Result<std::pair<UINT32, IMFActivate**>> VideoSourceReader::GetVideoCaptureDevices() const noexcept
	{
		IMFAttributes* attributes{ nullptr };

		// Create an attribute store to specify enumeration parameters
		auto hr = MFCreateAttributes(&attributes, 1);
		if (SUCCEEDED(hr))
		{
			// Release attributes when we exit this scope
			const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&attributes); });

			// Set source type attribute
			hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
			if (SUCCEEDED(hr))
			{
				UINT32 device_count{ 0 };
				IMFActivate** devices{ nullptr };

				// Enumerate the video capture devices
				hr = MFEnumDeviceSources(attributes, &devices, &device_count);
				if (SUCCEEDED(hr))
				{
					return std::make_pair(device_count, devices);
				}
			}
		}

		return AVResultCode::Failed;
	}

	const bool VideoSourceReader::GetVideoCaptureDeviceInfo(IMFActivate* device,
															VideoCaptureDevice& device_info) const noexcept
	{
		assert(device != nullptr);

		// Get the human-friendly name of the device
		auto hr = device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
											 &device_info.DeviceNameString, &device_info.DeviceNameStringLength);
		if (SUCCEEDED(hr))
		{
			// Get symbolic link for the device
			hr = device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
											&device_info.SymbolicLink, &device_info.SymbolicLinkLength);
			if (SUCCEEDED(hr))
			{
				return true;
			}
		}

		return false;
	}

	Result<> VideoSourceReader::CreateSourceReader(SourceReaderData& source_reader_data) noexcept
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
								hr = source_reader_data.SourceReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
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

	Result<std::pair<IMFMediaType*, GUID>> VideoSourceReader::GetSupportedMediaType(IMFSourceReader* source_reader) noexcept
	{
		assert(source_reader != nullptr);

		// Try to find a suitable output type
		for (DWORD i = 0; ; ++i)
		{
			IMFMediaType* media_type{ nullptr };

			auto hr = source_reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
														i, &media_type);
			if (SUCCEEDED(hr))
			{
				// Release media type when we exit this scope
				auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&media_type); });

				GUID subtype{ 0 };

				hr = media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
				if (SUCCEEDED(hr))
				{
					if (subtype == MFVideoFormat_RGB24)
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

	Result<> VideoSourceReader::SetMediaType(SourceReaderData& source_reader_data,
											 IMFMediaType* media_type, const GUID& subtype) noexcept
	{
		// Set media type
		auto hr = source_reader_data.SourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
																	   nullptr, media_type);
		if (SUCCEEDED(hr))
		{
			source_reader_data.VideoFormat = subtype;

			// Get width and height
			hr = MFGetAttributeSize(media_type, MF_MT_FRAME_SIZE,
									&source_reader_data.Width, &source_reader_data.Height);
			if (SUCCEEDED(hr))
			{
				// Get the stride for this format so we can calculate the number of bytes per pixel
				if (GetDefaultStride(media_type, &source_reader_data.Stride))
				{
					source_reader_data.BytesPerPixel = abs(source_reader_data.Stride) / source_reader_data.Width;

					try
					{
						// Allocate a buffer for the raw pixel data
						source_reader_data.RawData.Allocate(static_cast<Size>(source_reader_data.Width) *
															static_cast<Size>(source_reader_data.Height) *
															static_cast<Size>(source_reader_data.BytesPerPixel));

						return AVResultCode::Succeeded;
					}
					catch (...)
					{
						return AVResultCode::FailedOutOfMemory;
					}
				}
			}
		}

		return AVResultCode::Failed;
	}

	// Calculates the default stride based on the format and size of the frames
	const bool VideoSourceReader::GetDefaultStride(IMFMediaType* type, LONG* stride) const noexcept
	{
		LONG tstride{ 0 };

		// Try to get the default stride from the media type
		auto hr = type->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&tstride);
		if (FAILED(hr))
		{
			// Setting this atribute to NULL we can obtain the default stride
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
					hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, &tstride);
					if (SUCCEEDED(hr))
					{
						// Set the attribute so it can be read
						hr = type->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(tstride));
					}
				}
			}
		}

		if (SUCCEEDED(hr))
		{
			*stride = tstride;
			return true;
		}

		return false;
	}

	HRESULT VideoSourceReader::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
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

				// Get the video frame buffer from the sample
				hr = pSample->GetBufferByIndex(0, &media_buffer);
				if (SUCCEEDED(hr))
				{
					// Release buffer when we exit this scope
					const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&media_buffer); });

					// Draw the frame
					BYTE* data{ nullptr };

					DbgInvoke([&]() {
						DWORD data_length{ 0 };
						media_buffer->GetCurrentLength(&data_length);

						// Buffer lengths should match or something is wrong
						assert(data_length == source_reader_data->RawData.GetSize());
					});

					hr = media_buffer->Lock(&data, nullptr, nullptr);
					if (SUCCEEDED(hr))
					{
						CopyMemory(source_reader_data->RawData.GetBytes(), data, source_reader_data->RawData.GetSize());
						media_buffer->Unlock();
					}
				}
			}
		}

		// Request the next frame
		if (SUCCEEDED(hr))
		{
			hr = source_reader_data->SourceReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
															  0, nullptr, nullptr, nullptr, nullptr);
		}

		return hr;
	}

	void VideoSourceReader::GetSample(BGRAPixel* buffer) noexcept
	{
		auto source_reader_data = m_SourceReader.WithSharedLock();

		if (source_reader_data->VideoFormat == MFVideoFormat_RGB24)
		{
			// For some reason MFVideoFormat_RGB24 has a BGR order
			BGR24ToBGRA32(buffer, reinterpret_cast<const BGRPixel*>(source_reader_data->RawData.GetBytes()),
						  source_reader_data->Width, source_reader_data->Height, source_reader_data->Stride);
		}
	}

	std::pair<UInt, UInt> VideoSourceReader::GetSampleDimensions() noexcept
	{
		auto source_reader_data = m_SourceReader.WithSharedLock();

		return std::make_pair(source_reader_data->Width, source_reader_data->Height);
	}
}