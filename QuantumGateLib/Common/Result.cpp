// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Result.h"
#include "Common\Util.h"

namespace QuantumGate::Implementation
{
	class ResultCodeErrorCategory final : public std::error_category
	{
	public:
		const char* name() const noexcept override { return "QuantumGate"; }

		std::string message(int code) const override
		{
			switch (static_cast<ResultCode>(code))
			{
				case ResultCode::Succeeded:
					return "Operation succeeded.";
				case ResultCode::Failed:
					return "Operation failed.";
				case ResultCode::FailedRetry:
					return "Operation failed. Retry possible.";
				case ResultCode::NotRunning:
					return "Operation failed. Object was not in the running state.";
				case ResultCode::InvalidArgument:
					return "Operation failed. An argument was invalid.";
				case ResultCode::NotAllowed:
					return "Operation failed. Not allowed by security configuration.";
				case ResultCode::TimedOut:
					return "Operation timed out.";
				case ResultCode::Aborted:
					return "Operation was aborted.";
				case ResultCode::OutOfMemory:
					return "Operation failed. There was not enough memory available.";
				case ResultCode::FailedTCPListenerManagerStartup:
					return "Operation failed. TCP listenermanager startup failed.";
				case ResultCode::FailedPeerManagerStartup:
					return "Operation failed. Peermanager startup failed.";
				case ResultCode::FailedRelayManagerStartup:
					return "Operation failed. Relaymanager startup failed.";
				case ResultCode::FailedExtenderManagerStartup:
					return "Operation failed. Extendermanager startup failed.";
				case ResultCode::FailedKeyGenerationManagerStartup:
					return "Operation failed. Keygenerationmanager startup failed.";
				case ResultCode::FailedUDPConnectionManagerStartup:
					return "Operation failed. UDP connectionmanager startup failed.";
				case ResultCode::FailedUDPListenerManagerStartup:
					return "Operation failed. UDP listenermanager startup failed.";
				case ResultCode::NoPeersForRelay:
					return "Operation failed. There were no connected peers to relay with.";
				case ResultCode::PeerNotFound:
					return "Operation failed. The peer wasn't found.";
				case ResultCode::PeerNotReady:
					return "Operation failed. The peer wasn't ready.";
				case ResultCode::PeerNoExtender:
					return "Operation failed. The peer doesn't have the extender active or installed.";
				case ResultCode::PeerAlreadyExists:
					return "Operation failed. The peer already exists.";
				case ResultCode::PeerSendBufferFull:
					return "Operation failed. The peer send buffer is full.";
				case ResultCode::PeerSuspended:
					return "Operation failed. The peer was suspended.";
				case ResultCode::AddressInvalid:
					return "Operation failed. The address wasn't recognized and may be invalid.";
				case ResultCode::AddressMaskInvalid:
					return "Operation failed. The address mask wasn't recognized and may be invalid.";
				case ResultCode::AddressNotFound:
					return "Operation failed. The address wasn't found.";
				case ResultCode::ExtenderNotFound:
					return "Operation failed. The extender was't found.";
				case ResultCode::ExtenderAlreadyPresent:
					return "Operation failed. The extender is already present.";
				case ResultCode::ExtenderObjectDifferent:
					return "Operation failed. The extender object is different.";
				case ResultCode::ExtenderAlreadyRemoved:
					return "Operation failed. The extender was already removed.";
				case ResultCode::ExtenderTooMany:
					return "Operation failed. The maximum number of extenders has been reached.";
				case ResultCode::ExtenderModuleAlreadyPresent:
					return "Operation failed. The extender module was already present.";
				case ResultCode::ExtenderModuleLoadFailure:
					return "Operation failed. The extender module failed to load.";
				case ResultCode::ExtenderModuleNotFound:
					return "Operation failed. The extender module wasn't found.";
				case ResultCode::ExtenderHasNoLocalInstance:
					return "Operation failed. The extender has no Local instance.";
				default:
					// Shouldn't get here
					assert(false);
					break;
			}

			return {};
		}
	};

	Export const std::error_category& GetResultCodeErrorCategory() noexcept
	{
		static ResultCodeErrorCategory ErrorCategory;
		return ErrorCategory;
	}
	
	Export std::ostream& operator<<(std::ostream& os, const ResultCode code)
	{
		os << Implementation::Util::ToStringA(Implementation::Util::FormatString(L"%d", static_cast<int>(code)));
		return os;
	}

	Export std::wostream& operator<<(std::wostream& os, const ResultCode code)
	{
		os << Implementation::Util::FormatString(L"%d", static_cast<int>(code));
		return os;
	}

	Export std::error_code make_error_code(const ResultCode code) noexcept
	{
		return std::error_code(static_cast<int>(code), GetResultCodeErrorCategory());
	}
}
