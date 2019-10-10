// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CaptureDevice.h"

#include <Concurrency\ThreadSafe.h>
#include <Common\Dispatcher.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	class SourceReader : public IMFSourceReaderCallback
	{
	public:
		using SampleEventDispatcher = Dispatcher<void(const UInt64, IMFSample*)>;
		using SampleEventCallback = QuantumGate::Callback<void(const UInt64, IMFSample*)>;

		struct SourceReaderData
		{
			IMFMediaSource* Source{ nullptr };
			IMFSourceReader* SourceReader{ nullptr };
			GUID Format{ GUID_NULL };
			SampleEventDispatcher Dispatcher;

			void Release() noexcept
			{
				SafeRelease(&SourceReader);

				if (Source)
				{
					Source->Shutdown();
				}
				SafeRelease(&Source);

				Format = GUID_NULL;

				Dispatcher.Clear();
			}
		};

		using SourceReaderData_ThS = Concurrency::ThreadSafe<SourceReaderData, std::shared_mutex>;

		SourceReader(const CaptureDevice::Type type) noexcept;
		SourceReader(const SourceReader&) = delete;
		SourceReader(SourceReader&&) = delete;
		virtual ~SourceReader();
		SourceReader& operator=(const SourceReader&) = delete;
		SourceReader& operator=(SourceReader&&) = delete;

		[[nodiscard]] Result<CaptureDeviceVector> EnumCaptureDevices() const noexcept;

		[[nodiscard]] Result<> Open(const WChar* device, const std::vector<GUID>& supported_formats, SampleEventCallback&& event_callback) noexcept;
		[[nodiscard]] bool IsOpen() const noexcept;
		void Close() noexcept;
		[[nodiscard]] bool BeginRead() noexcept;

		SampleEventDispatcher::FunctionHandle AddSampleEventCallback(SampleEventCallback&& function) noexcept;
		void RemoveSampleEventCallback(SampleEventDispatcher::FunctionHandle& func_handle) noexcept;

		// Methods from IMFSourceReaderCallback 
		STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
								  LONGLONG llTimestamp, IMFSample* pSample) override;
		STDMETHODIMP OnEvent(DWORD dwStreamIndex, IMFMediaEvent* pEvent) override { return S_OK; }
		STDMETHODIMP OnFlush(DWORD dwStreamIndex) override { return S_OK; }

	protected:
		SourceReaderData_ThS& GetSourceReaderData() noexcept { return m_SourceReaderData; }

		[[nodiscard]] virtual IMFSample* TransformSample(IMFSample* pSample) noexcept;

		[[nodiscard]] virtual bool OnOpen() noexcept;
		virtual void OnClose() noexcept;
		[[nodiscard]] virtual Result<> OnMediaTypeChanged(IMFMediaType* media_type) noexcept;

	private:
		[[nodiscard]] Result<> CreateSourceReader(SourceReaderData& source_reader_data,
												  const std::vector<GUID>& supported_formats) noexcept;
		[[nodiscard]] Result<> CreateReaderBuffer(SourceReaderData& source_reader_data, IMFMediaType* media_type) noexcept;

		[[nodiscard]] Result<std::pair<IMFMediaType*, GUID>> GetSupportedMediaType(IMFSourceReader* source_reader,
																				   const std::vector<GUID>& supported_formats) noexcept;

	private:
		const CaptureDevice::Type m_Type{ CaptureDevice::Type::Unknown };
		const GUID m_CaptureGUID{ GUID_NULL };
		const DWORD m_StreamIndex{ 0 };
		SourceReaderData_ThS m_SourceReaderData;
	};
}