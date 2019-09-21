// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SourceReader.h"
#include <Concurrency\ThreadSafe.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	class AudioSourceReader : public SourceReader
	{
		struct SourceReaderData
		{
			IMFMediaSource* Source{ nullptr };
			IMFSourceReader* SourceReader{ nullptr };
			GUID AudioFormat{ GUID_NULL };
			UINT32 NumChannels{ 0 };
			UINT32 BitsPerSample{ 0 };
			UINT32 SamplesPerSecond{ 0 };
			UINT32 AvgBytesPerSecond{ 0 };
			UINT32 BlockAlignment{ 0 };
			QuantumGate::Buffer RawData;

			void Release() noexcept
			{
				SafeRelease(&SourceReader);

				if (Source)
				{
					Source->Shutdown();
				}
				SafeRelease(&Source);

				AudioFormat = GUID_NULL;
				NumChannels = 0;
				BitsPerSample = 0;
				SamplesPerSecond = 0;
				AvgBytesPerSecond = 0;
				BlockAlignment = 0;
				RawData.Clear();
				RawData.FreeUnused();
			}
		};

		using SourceReaderData_ThS = Concurrency::ThreadSafe<SourceReaderData, std::shared_mutex>;

	public:
		AudioSourceReader() noexcept;
		AudioSourceReader(const AudioSourceReader&) = delete;
		AudioSourceReader(AudioSourceReader&&) = delete;
		virtual ~AudioSourceReader();
		AudioSourceReader& operator=(const AudioSourceReader&) = delete;
		AudioSourceReader& operator=(AudioSourceReader&&) = delete;

		[[nodiscard]] Result<> Open(const CaptureDevice& device) noexcept;
		[[nodiscard]] bool IsOpen() noexcept;
		void Close() noexcept;

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
		[[nodiscard]] Result<> CreateSourceReader(SourceReaderData& source_reader_data) noexcept;

		[[nodiscard]] Result<std::pair<IMFMediaType*, GUID>> GetSupportedMediaType(IMFSourceReader* source_reader) noexcept;
		[[nodiscard]] Result<> SetMediaType(SourceReaderData& source_reader_data,
											IMFMediaType* media_type, const GUID& subtype) noexcept;

	private:
		long m_RefCount{ 1 };
		SourceReaderData_ThS m_SourceReader;
	};
}

