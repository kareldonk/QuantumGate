// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"
#include <Concurrency\ThreadSafe.h>

#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>

#pragma comment(lib,"mf.lib")
#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	struct VideoCaptureDevice
	{
		VideoCaptureDevice() noexcept {}
		VideoCaptureDevice(const VideoCaptureDevice&) = delete;
		
		VideoCaptureDevice(VideoCaptureDevice&& other) noexcept :
			DeviceNameString(other.DeviceNameString),
			DeviceNameStringLength(other.DeviceNameStringLength),
			SymbolicLink(other.SymbolicLink),
			SymbolicLinkLength(other.SymbolicLinkLength)
		{
			other.DeviceNameString = nullptr;
			other.DeviceNameStringLength = 0;
			other.SymbolicLink = nullptr;
			other.SymbolicLinkLength = 0;
		}

		~VideoCaptureDevice()
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
		}

		VideoCaptureDevice& operator=(const VideoCaptureDevice&) = delete;

		VideoCaptureDevice& operator=(VideoCaptureDevice&& other) noexcept
		{
			DeviceNameString = std::exchange(other.DeviceNameString, nullptr);
			DeviceNameStringLength = std::exchange(other.DeviceNameStringLength, 0);
			SymbolicLink = std::exchange(other.SymbolicLink, nullptr);
			SymbolicLinkLength = std::exchange(other.SymbolicLinkLength, 0);
			return *this;
		}

		WCHAR* DeviceNameString{ nullptr };
		UINT32 DeviceNameStringLength{ 0 };
		WCHAR* SymbolicLink{ nullptr };
		UINT32 SymbolicLinkLength{ 0 };
	};

	using VideoCaptureDeviceVector = std::vector<VideoCaptureDevice>;

	class VideoSourceReader : public IMFSourceReaderCallback
	{
		struct SourceReaderData
		{
			IMFMediaSource* Source{ nullptr };
			IMFSourceReader* SourceReader{ nullptr };
			GUID VideoFormat{ GUID_NULL };
			UInt Width{ 0 };
			UInt Height{ 0 };
			UInt BytesPerPixel{ 0 };
			Long Stride{ 0 };
			QuantumGate::Buffer RawData;

			void Release() noexcept
			{
				SafeRelease(&SourceReader);

				if (Source)
				{
					Source->Shutdown();
				}
				SafeRelease(&Source);

				VideoFormat = GUID_NULL;
				Width = 0;
				Height = 0;
				BytesPerPixel = 0;
				Stride = 0;
				RawData.Clear();
				RawData.FreeUnused();
			}
		};

		using SourceReaderData_ThS = Concurrency::ThreadSafe<SourceReaderData, std::shared_mutex>;

	public:
		VideoSourceReader() noexcept;
		VideoSourceReader(const VideoSourceReader&) = delete;
		VideoSourceReader(VideoSourceReader&&) = delete;
		virtual ~VideoSourceReader();
		VideoSourceReader& operator=(const VideoSourceReader&) = delete;
		VideoSourceReader& operator=(VideoSourceReader&&) = delete;

		[[nodiscard]] Result<VideoCaptureDeviceVector> EnumCaptureDevices() const noexcept;
		[[nodiscard]] Result<> Open(const VideoCaptureDevice& device) noexcept;
		[[nodiscard]] bool IsOpen() noexcept;
		void Close() noexcept;

		void GetSample(BGRAPixel* buffer) noexcept;
		[[nodiscard]] std::pair<UInt, UInt> GetSampleDimensions() noexcept;

		// Methods from IUnknown 
		STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
		STDMETHODIMP_(ULONG) AddRef() override;
		STDMETHODIMP_(ULONG) Release() override;

		// Methods from IMFSourceReaderCallback 
		STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
								  LONGLONG llTimestamp, IMFSample* pSample) override;
		STDMETHODIMP OnEvent(DWORD dwStreamIndex, IMFMediaEvent* pEvent) override { return S_OK; }
		STDMETHODIMP OnFlush(DWORD dwStreamIndex) override { return S_OK; }

	private:
		[[nodiscard]] Result<std::pair<UINT32, IMFActivate**>> GetVideoCaptureDevices() const noexcept;
		[[nodiscard]] bool GetVideoCaptureDeviceInfo(IMFActivate* device,
													 VideoCaptureDevice& device_info) const noexcept;

		[[nodiscard]] Result<> CreateSourceReader(SourceReaderData& source_reader_data) noexcept;

		[[nodiscard]] Result<std::pair<IMFMediaType*, GUID>> GetSupportedMediaType(IMFSourceReader* source_reader) noexcept;
		[[nodiscard]] Result<> SetMediaType(SourceReaderData& source_reader_data,
											IMFMediaType* media_type, const GUID& subtype) noexcept;

		[[nodiscard]] bool GetDefaultStride(IMFMediaType* type, LONG* stride) const noexcept;

	private:
		long m_RefCount{ 1 };
		SourceReaderData_ThS m_SourceReader;
	};
}