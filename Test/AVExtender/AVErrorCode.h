// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <cassert>
#include <system_error>

namespace QuantumGate::AVExtender
{
	enum class AVResultCode
	{
		Succeeded = 0,
		Failed = 1,
		FailedCreateVideoDeviceSource = 2,
		FailedOutOfMemory = 3,
		FailedNoSupportedVideoMediaType = 4,
		FailedGetVideoCaptureDevices = 5
	};

	class AVErrorCategory final : public std::error_category
	{
	public:
		const char* name() const noexcept override { return "AV Extender"; }

		std::string message(int code) const override
		{
			switch (static_cast<AVResultCode>(code))
			{
				case AVResultCode::Succeeded:
					return "Operation succeeded.";
				case AVResultCode::Failed:
					return "Operation failed.";
				case AVResultCode::FailedCreateVideoDeviceSource:
					return "Operation failed. Couldn't create a video device source.";
				case AVResultCode::FailedOutOfMemory:
					return "Operation failed. Couldn't allocate memory.";
				case AVResultCode::FailedNoSupportedVideoMediaType:
					return "Operation failed. Couldn't find a supported video media type.";
				case AVResultCode::FailedGetVideoCaptureDevices:
					return "Operation failed. Couldn't get video capture devices.";
				default:
					// Shouldn't get here
					assert(false);
					break;
			}

			return {};
		}
	};

	// The following function overload is needed for the enum conversion 
	// in one of the constructors for class std::error_code
	std::error_code make_error_code(const AVResultCode code) noexcept;
}

namespace std
{
	// Needed to make the std::error_code class work with the AVResultCode enum
	template<>
	struct is_error_code_enum<QuantumGate::AVExtender::AVResultCode> : std::true_type {};
}