// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Compressor.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	Compressor::Compressor(const Type type, const CLSID encoderid, const CLSID decoderid) noexcept :
		m_Type(type), m_EncoderID(encoderid), m_DecoderID(decoderid)
	{}

	Compressor::~Compressor()
	{
		Close();
	}

	bool Compressor::Create() noexcept
	{
		assert(!IsOpen());

		// Close if failed
		auto sg = MakeScopeGuard([&]() noexcept { Close(); });

		HRESULT hr{ E_FAIL };

		switch (m_Type)
		{
			case Type::Encoder:
				hr = CoCreateInstance(m_EncoderID, nullptr, CLSCTX_ALL,
									  IID_IMFTransform, (void**)&m_IMFTransform);
				break;
			case Type::Decoder:
				hr = CoCreateInstance(m_DecoderID, nullptr, CLSCTX_ALL,
									  IID_IMFTransform, (void**)&m_IMFTransform);
				break;
			default:
				assert(false);
				break;
		}

		if (SUCCEEDED(hr))
		{
			if (SUCCEEDED(MFCreateMediaType(&m_InputMediaType)) &&
				SUCCEEDED(MFCreateMediaType(&m_OutputMediaType)))
			{
				if (OnCreateMediaTypes(m_InputMediaType, m_OutputMediaType))
				{
					if (OnSetMediaTypes(m_IMFTransform, m_InputMediaType, m_OutputMediaType))
					{
						DWORD status{ 0 };

						if (SUCCEEDED(m_IMFTransform->GetInputStatus(0, &status)))
						{
							if (status == MFT_INPUT_STATUS_ACCEPT_DATA)
							{
								if (SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL)) &&
									SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL)) &&
									SUCCEEDED(m_IMFTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL)))
								{
									if (OnCreate())
									{
										sg.Deactivate();

										m_Open = true;

										return true;
									}
								}
							}
						}
					}
				}
			}
		}

		return false;
	}

	void Compressor::Close() noexcept
	{
		m_Open = false;

		SafeRelease(&m_IMFTransform);
		SafeRelease(&m_InputMediaType);
		SafeRelease(&m_OutputMediaType);

		OnClose();
	}

	bool Compressor::AddInput(const UInt64 in_timestamp, const BufferView data) noexcept
	{
		assert(IsOpen());

		auto result = CaptureDevices::CreateMediaSample(data.GetSize());
		if (result.Succeeded())
		{
			auto in_sample = result.GetValue();

			// Release in case of failure
			const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&in_sample); });

			if (CaptureDevices::CopyToMediaSample(in_timestamp, GetDuration(data.GetSize()), data, in_sample))
			{
				if (AddInput(in_sample))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool Compressor::AddInput(IMFSample* in_sample) noexcept
	{
		assert(IsOpen());

		const auto hr = m_IMFTransform->ProcessInput(0, in_sample, 0);
		if (SUCCEEDED(hr))
		{
			return true;
		}
		else if (hr == MF_E_NOTACCEPTING)
		{
			// Need to read samples via GetOutput()
		}

		return false;
	}

	IMFSample* Compressor::GetOutput() noexcept
	{
		assert(IsOpen());

		if (m_Type == Type::Encoder)
		{
			DWORD flags{ 0 };

			const auto hr = m_IMFTransform->GetOutputStatus(&flags);
			if (SUCCEEDED(hr))
			{
				if (flags != MFT_OUTPUT_STATUS_SAMPLE_READY)
				{
					return nullptr;
				}
			}
			else return nullptr;
		}

		MFT_OUTPUT_STREAM_INFO osinfo{ 0 };

		auto hr = m_IMFTransform->GetOutputStreamInfo(0, &osinfo);
		if (SUCCEEDED(hr))
		{
			auto result = CaptureDevices::CreateMediaSample(osinfo.cbSize);
			if (result.Succeeded())
			{
				auto out_sample = result.GetValue();

				// Release in case of failure
				auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&out_sample); });

				MFT_OUTPUT_DATA_BUFFER output{ 0 };
				output.dwStreamID = 0;
				output.dwStatus = 0;
				output.pEvents = nullptr;
				output.pSample = out_sample;

				DWORD status{ 0 };

				hr = m_IMFTransform->ProcessOutput(0, 1, &output, &status);
				if (SUCCEEDED(hr))
				{
					sg.Deactivate();

					return out_sample;
				}
				else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
				{
					// Need to add more samples via AddInput()
				}
			}
		}

		return nullptr;
	}

	bool Compressor::GetOutput(Buffer& buffer) noexcept
	{
		assert(IsOpen());

		auto out_sample = GetOutput();
		if (out_sample != nullptr)
		{
			// Release when we exit this scope
			auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&out_sample); });

			return CaptureDevices::CopyFromMediaSample(out_sample, buffer);
		}

		return false;
	}

	bool Compressor::OnCreate() noexcept
	{
		return true;
	}

	void Compressor::OnClose() noexcept
	{}

	bool Compressor::OnCreateMediaTypes(IMFMediaType* input_type, IMFMediaType* output_type) noexcept
	{
		return false;
	}

	bool Compressor::OnSetMediaTypes(IMFTransform* transform, IMFMediaType* input_type, IMFMediaType* output_type) noexcept
	{
		return false;
	}

	UInt64 Compressor::GetDuration(const Size sample_size) noexcept
	{
		return 0;
	}
}