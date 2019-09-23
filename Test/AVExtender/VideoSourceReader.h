// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SourceReader.h"

namespace QuantumGate::AVExtender
{
	class VideoSourceReader : public SourceReader
	{
	public:
		VideoSourceReader() noexcept;
		VideoSourceReader(const VideoSourceReader&) = delete;
		VideoSourceReader(VideoSourceReader&&) = delete;
		virtual ~VideoSourceReader();
		VideoSourceReader& operator=(const VideoSourceReader&) = delete;
		VideoSourceReader& operator=(VideoSourceReader&&) = delete;

		[[nodiscard]] inline VideoFormat GetSampleFormat() noexcept { return *m_VideoFormat.WithSharedLock(); }

		// Methods from IUnknown 
		STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
		STDMETHODIMP_(ULONG) AddRef() override;
		STDMETHODIMP_(ULONG) Release() override;

	protected:
		[[nodiscard]] Result<> OnMediaTypeChanged(IMFMediaType* media_type) noexcept override;
		[[nodiscard]] Result<Size> GetBufferSize(IMFMediaType* media_type) noexcept override;

		[[nodiscard]] bool GetDefaultStride(IMFMediaType* type, LONG* stride) const noexcept;

	private:
		long m_RefCount{ 1 };
		VideoFormat_ThS m_VideoFormat;
	};
}