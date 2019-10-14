// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SourceReader.h"
#include "AudioResampler.h"

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	class AudioSourceReader final : public SourceReader
	{
		struct AudioTransform
		{
			AudioResampler InAudioResampler;

			IMFSample* m_OutputSample{ nullptr };
		};

		using AudioTransform_ThS = Concurrency::ThreadSafe<AudioTransform, std::shared_mutex>;

		struct AudioFormatData
		{
			AudioFormat ReaderFormat;
			AudioFormat TransformFormat;
		};

		using AudioFormatData_ThS = Concurrency::ThreadSafe<AudioFormatData, std::shared_mutex>;

	public:
		AudioSourceReader() noexcept;
		AudioSourceReader(const AudioSourceReader&) = delete;
		AudioSourceReader(AudioSourceReader&&) = delete;
		virtual ~AudioSourceReader();
		AudioSourceReader& operator=(const AudioSourceReader&) = delete;
		AudioSourceReader& operator=(AudioSourceReader&&) = delete;

		[[nodiscard]] bool SetSampleFormat(const AudioFormat fmt) noexcept;

		[[nodiscard]] AudioFormat GetSampleFormat() const noexcept;

		// Methods from IUnknown 
		STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
		STDMETHODIMP_(ULONG) AddRef() override;
		STDMETHODIMP_(ULONG) Release() override;

	protected:
		[[nodiscard]] bool OnOpen() noexcept override;
		void OnClose() noexcept override;
		[[nodiscard]] Result<> OnMediaTypeChanged(IMFMediaType* media_type) noexcept override;

		[[nodiscard]] IMFSample* TransformSample(IMFSample* pSample) noexcept override;

	private:
		[[nodiscard]] bool CreateAudioTransform() noexcept;
		void CloseAudioTransform() noexcept;

	private:
		long m_RefCount{ 1 };
		std::atomic_bool m_Transform{ false };
		AudioFormatData_ThS m_AudioFormatData;
		AudioTransform_ThS m_AudioTransform;
	};
}

