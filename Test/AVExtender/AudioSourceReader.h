// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SourceReader.h"

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	class AudioSourceReader : public SourceReader
	{
	public:
		AudioSourceReader() noexcept;
		AudioSourceReader(const AudioSourceReader&) = delete;
		AudioSourceReader(AudioSourceReader&&) = delete;
		virtual ~AudioSourceReader();
		AudioSourceReader& operator=(const AudioSourceReader&) = delete;
		AudioSourceReader& operator=(AudioSourceReader&&) = delete;

		void GetSample(Buffer& buffer) noexcept;
		[[nodiscard]] inline AudioSettings GetSampleSettings() noexcept { return *m_OutputAudioSettings.WithSharedLock(); }

		// Methods from IUnknown 
		STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
		STDMETHODIMP_(ULONG) AddRef() override;
		STDMETHODIMP_(ULONG) Release() override;

	protected:
		[[nodiscard]] Result<> OnMediaTypeChanged(IMFMediaType* media_type) noexcept override;
		[[nodiscard]] Result<Size> GetBufferSize(IMFMediaType* media_type) noexcept override;

	private:
		long m_RefCount{ 1 };
		AudioSettings_ThS m_OutputAudioSettings;
	};
}

