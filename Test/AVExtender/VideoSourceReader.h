// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SourceReader.h"
#include "VideoResampler.h"
#include "VideoResizer.h"

namespace QuantumGate::AVExtender
{
	class VideoSourceReader final : public SourceReader
	{
		struct VideoTransform
		{
			VideoResampler InVideoResampler;
			VideoResizer VideoResizer;
			VideoResampler OutVideoResampler;

			IMFSample* m_OutputSample1{ nullptr };
			IMFSample* m_OutputSample2{ nullptr };
		};

		using VideoTransform_ThS = Concurrency::ThreadSafe<VideoTransform, std::shared_mutex>;

		struct VideoFormatData
		{
			Size TransformWidth{ 0 };
			Size TransformHeight{ 0 };
			VideoFormat ReaderFormat;
		};

		using VideoFormatData_ThS = Concurrency::ThreadSafe<VideoFormatData, std::shared_mutex>;

	public:
		VideoSourceReader() noexcept;
		VideoSourceReader(const VideoSourceReader&) = delete;
		VideoSourceReader(VideoSourceReader&&) = delete;
		virtual ~VideoSourceReader();
		VideoSourceReader& operator=(const VideoSourceReader&) = delete;
		VideoSourceReader& operator=(VideoSourceReader&&) = delete;

		[[nodiscard]] bool SetSampleSize(const Size width, const Size height) noexcept;

		[[nodiscard]] VideoFormat GetSampleFormat() const noexcept;

		// Methods from IUnknown 
		STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
		STDMETHODIMP_(ULONG) AddRef() override;
		STDMETHODIMP_(ULONG) Release() override;

	protected:
		[[nodiscard]] bool OnOpen() noexcept override;
		void OnClose() noexcept override;
		[[nodiscard]] Result<> OnMediaTypeChanged(IMFMediaType* media_type) noexcept override;

		[[nodiscard]] IMFSample* TransformSample(IMFSample* pSample) noexcept override;

		[[nodiscard]] bool GetDefaultStride(IMFMediaType* type, LONG* stride) const noexcept;

	private:
		[[nodiscard]] bool CreateVideoTransform() noexcept;
		void CloseVideoTransform() noexcept;

	private:
		long m_RefCount{ 1 };
		std::atomic_bool m_Transform{ false };
		VideoFormatData_ThS m_VideoFormatData;
		VideoTransform_ThS m_VideoTransform;
	};
}