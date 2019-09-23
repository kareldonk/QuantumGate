// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"

#include <Common\ScopeGuard.h>

#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	struct CaptureDevice
	{
		enum Type
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

		[[nodiscard]] static Result<std::pair<IMFSample*, IMFMediaBuffer*>> CreateMediaSample(const Size size) noexcept
		{
			IMFSample* sample{ nullptr };
			IMFMediaBuffer* buffer{ nullptr };

			auto hr = MFCreateSample(&sample);
			if (SUCCEEDED(hr))
			{
				hr = MFCreateMemoryBuffer(static_cast<DWORD>(size), &buffer);
				if (SUCCEEDED(hr))
				{
					hr = sample->AddBuffer(buffer);
					if (SUCCEEDED(hr))
					{
						return std::make_pair(sample, buffer);
					}
				}
			}

			return AVResultCode::Failed;
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