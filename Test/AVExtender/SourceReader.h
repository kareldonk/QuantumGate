// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "CaptureDevice.h"
#include <Concurrency\ThreadSafe.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate::Implementation;

	class SourceReader : public IMFSourceReaderCallback
	{
		struct SourceReaderData
		{
			IMFMediaSource* Source{ nullptr };
			IMFSourceReader* SourceReader{ nullptr };
			GUID Format{ GUID_NULL };
			QuantumGate::Buffer RawData;

			void Release() noexcept
			{
				SafeRelease(&SourceReader);

				if (Source)
				{
					Source->Shutdown();
				}
				SafeRelease(&Source);

				Format = GUID_NULL;
				RawData.Clear();
				RawData.FreeUnused();
			}
		};

		using SourceReaderData_ThS = Concurrency::ThreadSafe<SourceReaderData, std::shared_mutex>;

	public:
		SourceReader(const CaptureDevice::Type type) noexcept;
		SourceReader(const SourceReader&) = delete;
		SourceReader(SourceReader&&) = delete;
		virtual ~SourceReader();
		SourceReader& operator=(const SourceReader&) = delete;
		SourceReader& operator=(SourceReader&&) = delete;

		[[nodiscard]] Result<CaptureDeviceVector> EnumCaptureDevices() const noexcept;

	private:
		CaptureDevice::Type m_Type{ CaptureDevice::Type::Unknown };
		SourceReaderData_ThS m_SourceReader;
	};
}