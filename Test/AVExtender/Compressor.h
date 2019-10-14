// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common.h"
#include "CaptureDevice.h"

namespace QuantumGate::AVExtender
{
	class Compressor
	{
	public:
		enum class Type { Unknown, Encoder, Decoder };

		Compressor(const Type type, const CLSID encoderid, const CLSID decoderid) noexcept;
		Compressor(const Compressor&) = delete;
		Compressor(Compressor&&) = default;
		virtual ~Compressor();
		Compressor& operator=(const Compressor&) = delete;
		Compressor& operator=(Compressor&&) = default;

		[[nodiscard]] inline Type GetType() const noexcept { return m_Type; }

		[[nodiscard]] bool Create() noexcept;
		void Close() noexcept;
		[[nodiscard]] inline bool IsOpen() const noexcept { return m_Open; }

		[[nodiscard]] bool AddInput(const UInt64 in_timestamp, const BufferView data) noexcept;
		[[nodiscard]] bool AddInput(IMFSample* in_sample) noexcept;

		[[nodiscard]] IMFSample* GetOutput() noexcept;
		[[nodiscard]] bool GetOutput(Buffer& buffer) noexcept;

	protected:
		[[nodiscard]] virtual bool OnCreate() noexcept;
		[[nodiscard]] virtual void OnClose() noexcept;
		[[nodiscard]] virtual bool OnCreateMediaTypes(IMFMediaType* input_type, IMFMediaType* output_type) noexcept;
		[[nodiscard]] virtual bool OnSetMediaTypes(IMFTransform* transform, IMFMediaType* input_type, IMFMediaType* output_type) noexcept;
		[[nodiscard]] virtual UInt64 GetDuration(const Size sample_size) noexcept;

	private:
		const Type m_Type{ Type::Unknown };
		const CLSID m_EncoderID{ CLSID_NULL };
		const CLSID m_DecoderID{ CLSID_NULL };
		bool m_Open{ false };

		IMFTransform* m_IMFTransform{ nullptr };
		IMFMediaType* m_InputMediaType{ nullptr };
		IMFMediaType* m_OutputMediaType{ nullptr };
	};
}