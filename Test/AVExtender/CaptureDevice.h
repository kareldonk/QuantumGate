// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <mferror.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	struct CaptureDevice final
	{
		enum class Type
		{
			Unknown, Video, Audio
		};

		CaptureDevice(const Type type) noexcept :
			DeviceType(type)
		{}

		CaptureDevice(const CaptureDevice&) = delete;

		CaptureDevice(CaptureDevice&& other) noexcept :
			DeviceType(other.DeviceType),
			DeviceNameString(other.DeviceNameString),
			DeviceNameStringLength(other.DeviceNameStringLength),
			SymbolicLink(other.SymbolicLink),
			SymbolicLinkLength(other.SymbolicLinkLength),
			EndpointID(other.EndpointID),
			EndpointIDLength(other.EndpointIDLength)
		{
			other.DeviceType = Type::Unknown;
			other.DeviceNameString = nullptr;
			other.DeviceNameStringLength = 0;
			other.SymbolicLink = nullptr;
			other.SymbolicLinkLength = 0;
			other.EndpointID = nullptr;
			other.EndpointIDLength = 0;
		}

		~CaptureDevice()
		{
			if (DeviceNameString)
			{
				CoTaskMemFree(DeviceNameString);
				DeviceNameString = nullptr;
				DeviceNameStringLength = 0;
			}

			if (SymbolicLink)
			{
				CoTaskMemFree(SymbolicLink);
				SymbolicLink = nullptr;
				SymbolicLinkLength = 0;
			}

			if (EndpointID)
			{
				CoTaskMemFree(EndpointID);
				EndpointID = nullptr;
				EndpointIDLength = 0;
			}
		}

		CaptureDevice& operator=(const CaptureDevice&) = delete;

		CaptureDevice& operator=(CaptureDevice&& other) noexcept
		{
			DeviceType = std::exchange(other.DeviceType, Type::Unknown);
			DeviceNameString = std::exchange(other.DeviceNameString, nullptr);
			DeviceNameStringLength = std::exchange(other.DeviceNameStringLength, 0);
			SymbolicLink = std::exchange(other.SymbolicLink, nullptr);
			SymbolicLinkLength = std::exchange(other.SymbolicLinkLength, 0);
			EndpointID = std::exchange(other.EndpointID, nullptr);
			EndpointIDLength = std::exchange(other.EndpointIDLength, 0);
			return *this;
		}

		Type DeviceType{ Type::Unknown };
		WCHAR* DeviceNameString{ nullptr };
		UINT32 DeviceNameStringLength{ 0 };
		WCHAR* SymbolicLink{ nullptr };
		UINT32 SymbolicLinkLength{ 0 };
		WCHAR* EndpointID{ nullptr };
		UINT32 EndpointIDLength{ 0 };
	};

	using CaptureDeviceVector = std::vector<CaptureDevice>;

	struct CaptureDevices
	{
	public:
		[[nodiscard]] static bool Startup() noexcept
		{
			const auto hr = MFStartup(MF_VERSION);
			if (SUCCEEDED(hr)) return true;

			return false;
		}

		[[nodiscard]] static bool Shutdown() noexcept
		{
			const auto hr = MFShutdown();
			if (SUCCEEDED(hr)) return true;

			return false;
		}

		[[nodiscard]] static Result<CaptureDeviceVector> Enum(const CaptureDevice::Type type) noexcept
		{
			assert(type != CaptureDevice::Type::Unknown);

			CaptureDeviceVector ret_devices;

			auto result = CaptureDevices::GetCaptureDevices(type);
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
								auto& device_info = ret_devices.emplace_back(type);
								if (!CaptureDevices::GetCaptureDeviceInfo(devices[x], device_info))
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
			else
			{
				if (type == CaptureDevice::Type::Audio) return AVResultCode::FailedGetAudioCaptureDevices;
				else if (type == CaptureDevice::Type::Video) return AVResultCode::FailedGetVideoCaptureDevices;
			}

			return AVResultCode::Failed;
		}

		[[nodiscard]] static String GetSupportedMediaTypes(IMFSourceReader* source_reader,
														   const DWORD stream_index) noexcept
		{
			assert(source_reader != nullptr);

			String types;

			for (DWORD i = 0; ; ++i)
			{
				IMFMediaType* media_type{ nullptr };

				auto hr = source_reader->GetNativeMediaType(stream_index, i, &media_type);
				if (SUCCEEDED(hr))
				{
					// Release media type when we exit this scope
					auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&media_type); });

					GUID majortype{ GUID_NULL };
					GUID subtype{ GUID_NULL };

					hr = media_type->GetGUID(MF_MT_MAJOR_TYPE, &majortype);
					if (SUCCEEDED(hr))
					{
						hr = media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
						if (SUCCEEDED(hr))
						{
							try
							{
								if (!types.empty()) types += L", ";

								if (majortype == MFMediaType_Video)
								{
									UINT32 width{ 0 };
									UINT32 height{ 0 };

									MFGetAttributeSize(media_type, MF_MT_FRAME_SIZE, &width, &height);

									types += Util::FormatString(L"%s (%u x %u)", GetMediaTypeName(subtype), width, height);
								}
								else if (majortype == MFMediaType_Audio)
								{
									UINT32 channels{ 0 };
									UINT32 samples{ 0 };
									UINT32 bits{ 0 };

									media_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
									media_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samples);
									media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);

									types += Util::FormatString(L"%s (%u channels, %u Hz, %u bits)",
																GetMediaTypeName(subtype), channels, samples, bits);
								}
							}
							catch (...) {}
						}
					}
				}
				else break;
			}

			return types;
		}

		[[nodiscard]] static const WChar* GetMediaTypeName(const GUID type) noexcept
		{
			if (type == MFVideoFormat_IYUV) return L"MFVideoFormat_IYUV";
			else if (type == MFVideoFormat_NV12) return L"MFVideoFormat_NV12";
			else if (type == MFVideoFormat_YUY2) return L"MFVideoFormat_YUY2";
			else if (type == MFVideoFormat_YV12) return L"MFVideoFormat_YV12";
			else if (type == MFVideoFormat_UYVY) return L"MFVideoFormat_UYVY";
			else if (type == MFVideoFormat_AYUV) return L"MFVideoFormat_AYUV";
			else if (type == MFVideoFormat_I420) return L"MFVideoFormat_I420";
			else if (type == MFVideoFormat_AI44) return L"MFVideoFormat_AI44";
			else if (type == MFVideoFormat_NV11) return L"MFVideoFormat_NV11";
			else if (type == MFVideoFormat_Y41P) return L"MFVideoFormat_Y41P";
			else if (type == MFVideoFormat_Y41T) return L"MFVideoFormat_Y41T";
			else if (type == MFVideoFormat_Y42T) return L"MFVideoFormat_Y42T";
			else if (type == MFVideoFormat_YVU9) return L"MFVideoFormat_YVU9";
			else if (type == MFVideoFormat_YVYU) return L"MFVideoFormat_YVYU";
			else if (type == MFVideoFormat_RGB32) return L"MFVideoFormat_RGB32";
			else if (type == MFVideoFormat_RGB24) return L"MFVideoFormat_RGB24";
			else if (type == MFVideoFormat_RGB8) return L"MFVideoFormat_RGB8";
			else if (type == MFAudioFormat_Float) return L"MFAudioFormat_Float";
			else if (type == MFAudioFormat_PCM) return L"MFAudioFormat_PCM";

			return L"Unknown";
		}

		[[nodiscard]] static Result<IMFSample*> CreateMediaSample(const Size size) noexcept
		{
			IMFSample* sample{ nullptr };
			IMFMediaBuffer* buffer{ nullptr };

			auto hr = MFCreateSample(&sample);
			if (SUCCEEDED(hr))
			{
				// Free upon failure
				auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&sample); });

				hr = MFCreateMemoryBuffer(static_cast<DWORD>(size), &buffer);
				if (SUCCEEDED(hr))
				{
					// Free when we return
					const auto sgs = MakeScopeGuard([&]() noexcept { SafeRelease(&buffer); });

					hr = sample->AddBuffer(buffer);
					if (SUCCEEDED(hr))
					{
						sg.Deactivate();

						return sample;
					}
				}
			}

			return AVResultCode::Failed;
		}

		[[nodiscard]] static bool CopyToMediaSample(const UInt64 in_timestamp, const UInt64 in_duration,
													const BufferView in_data, IMFSample* out_sample) noexcept
		{
			IMFMediaBuffer* media_buffer{ nullptr };

			// Get the buffer from the sample
			auto hr = out_sample->GetBufferByIndex(0, &media_buffer);
			if (SUCCEEDED(hr))
			{
				// Release buffer when we exit this scope
				const auto sg2 = MakeScopeGuard([&]() noexcept { SafeRelease(&media_buffer); });

				BYTE* out_data{ nullptr };
				DWORD max_out_data_len{ 0 };

				hr = media_buffer->Lock(&out_data, &max_out_data_len, nullptr);
				if (SUCCEEDED(hr))
				{
					assert(in_data.GetSize() <= max_out_data_len);

					DWORD cpylen = static_cast<DWORD>(in_data.GetSize());
					if (cpylen > max_out_data_len) cpylen = max_out_data_len;

					std::memcpy(out_data, in_data.GetBytes(), cpylen);

					if (SUCCEEDED(media_buffer->Unlock()))
					{
						hr = media_buffer->SetCurrentLength(cpylen);
						if (SUCCEEDED(hr))
						{
							if (SUCCEEDED(out_sample->SetSampleTime(in_timestamp)) &&
								SUCCEEDED(out_sample->SetSampleDuration(in_duration)))
							{
								return true;
							}
						}
					}
				}
			}

			return false;
		}

		[[nodiscard]] static bool CopyFromMediaSample(IMFSample* in_sample, Buffer& out_buffer) noexcept
		{
			IMFMediaBuffer* media_buffer{ nullptr };

			// Get the buffer from the sample
			auto hr = in_sample->GetBufferByIndex(0, &media_buffer);
			if (SUCCEEDED(hr))
			{
				// Release buffer when we exit this scope
				const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&media_buffer); });

				BYTE* in_data{ nullptr };
				DWORD in_data_len{ 0 };

				hr = media_buffer->Lock(&in_data, nullptr, &in_data_len);
				if (SUCCEEDED(hr))
				{
					// Unlock when we exit this scope
					auto sg2 = MakeScopeGuard([&]() noexcept { media_buffer->Unlock(); });

					try
					{
						out_buffer.Resize(in_data_len);

						std::memcpy(out_buffer.GetBytes(), in_data, in_data_len);

						return true;
					}
					catch (...) {}
				}
			}

			return false;
		}

		[[nodiscard]] static GUID GetMFVideoFormat(const VideoFormat::PixelFormat fmt) noexcept
		{
			switch (fmt)
			{
				case VideoFormat::PixelFormat::RGB24:
					return MFVideoFormat_RGB24;
				case VideoFormat::PixelFormat::RGB32:
					return MFVideoFormat_RGB32;
				case VideoFormat::PixelFormat::NV12:
					return MFVideoFormat_NV12;
				case VideoFormat::PixelFormat::YV12:
					return MFVideoFormat_YV12;
				case VideoFormat::PixelFormat::I420:
					return MFVideoFormat_I420;
				default:
					assert(false);
					break;
			}

			return GUID_NULL;
		}

		[[nodiscard]] static VideoFormat::PixelFormat GetVideoFormat(const GUID subtype) noexcept
		{
			if (subtype == MFVideoFormat_RGB24)
			{
				return VideoFormat::PixelFormat::RGB24;
			}
			else if (subtype == MFVideoFormat_RGB32)
			{
				return VideoFormat::PixelFormat::RGB32;
			}
			else if (subtype == MFVideoFormat_NV12)
			{
				return VideoFormat::PixelFormat::NV12;
			}
			else if (subtype == MFVideoFormat_YV12)
			{
				return VideoFormat::PixelFormat::YV12;
			}
			else if (subtype == MFVideoFormat_I420)
			{
				return VideoFormat::PixelFormat::I420;
			}

			return VideoFormat::PixelFormat::Unknown;
		}

		[[nodiscard]] static Size GetImageSize(const VideoFormat& fmt) noexcept
		{
			UINT32 size{ 0 };

			if (SUCCEEDED(MFCalculateImageSize(GetMFVideoFormat(fmt.Format), fmt.Width, fmt.Height, &size)))
			{
				return size;
			}
			else assert(false);

			return 0;
		}

		[[nodiscard]] static Size GetImageSize(const GUID fmt, const Size width, const Size height) noexcept
		{
			UINT32 size{ 0 };

			if (SUCCEEDED(MFCalculateImageSize(fmt, static_cast<UINT32>(width), static_cast<UINT32>(height), &size)))
			{
				return size;
			}
			else assert(false);

			return 0;
		}

	private:
		[[nodiscard]] static Result<std::pair<UINT32, IMFActivate**>> GetCaptureDevices(const CaptureDevice::Type type) noexcept
		{
			assert(type != CaptureDevice::Type::Unknown);

			IMFAttributes* attributes{ nullptr };

			// Create an attribute store to specify enumeration parameters
			auto hr = MFCreateAttributes(&attributes, 1);
			if (SUCCEEDED(hr))
			{
				// Release attributes when we exit this scope
				const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&attributes); });

				auto source_guid = MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID;
				if (type == CaptureDevice::Type::Video)
				{
					source_guid = MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID;
				}

				// Set source type attribute
				hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, source_guid);
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

		[[nodiscard]] static bool GetCaptureDeviceInfo(IMFActivate* device, CaptureDevice& device_info) noexcept
		{
			assert(device != nullptr);
			assert(device_info.DeviceType != CaptureDevice::Type::Unknown);

			// Get the human-friendly name of the device
			auto hr = device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
												 &device_info.DeviceNameString, &device_info.DeviceNameStringLength);
			if (SUCCEEDED(hr))
			{
				if (device_info.DeviceType == CaptureDevice::Type::Video)
				{
					// Get symbolic link for the device
					hr = device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
													&device_info.SymbolicLink, &device_info.SymbolicLinkLength);
					if (SUCCEEDED(hr))
					{
						return true;
					}
				}
				else if (device_info.DeviceType == CaptureDevice::Type::Audio)
				{
					// Get symbolic link for the device
					hr = device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID,
													&device_info.EndpointID, &device_info.EndpointIDLength);
					if (SUCCEEDED(hr))
					{
						return true;
					}
				}
			}

			return false;
		}
	};
}