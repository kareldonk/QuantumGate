// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\API\Access.h"
#include "..\..\Common\Containers.h"

namespace QuantumGate::Implementation::Core::Access
{
	using namespace QuantumGate::API::Access;

	class PeerAccessControl final
	{
		struct PeerAccessDetails final
		{
			ProtectedBuffer PublicKey;
			bool AccessAllowed{ false };
		};

		using PeerAccessDetailsMap = Containers::UnorderedMap<PeerUUID, PeerAccessDetails>;

	public:
		PeerAccessControl() = delete;
		PeerAccessControl(const Settings_CThS& settings) noexcept;
		PeerAccessControl(const PeerAccessControl&) = delete;
		PeerAccessControl(PeerAccessControl&&) noexcept = default;
		~PeerAccessControl() = default;
		PeerAccessControl& operator=(const PeerAccessControl&) = delete;
		PeerAccessControl& operator=(PeerAccessControl&&) noexcept = default;

		Result<> AddPeer(PeerSettings&& pas) noexcept;
		Result<> UpdatePeer(PeerSettings&& pas) noexcept;
		Result<> RemovePeer(const PeerUUID& puuid) noexcept;

		Result<bool> IsAllowed(const PeerUUID& puuid) const noexcept;

		const ProtectedBuffer* GetPublicKey(const PeerUUID& puuid) const noexcept;

		void SetAccessDefault(const PeerAccessDefault pad) noexcept { m_AccessDefaultAllowed = pad; }
		[[nodiscard]] PeerAccessDefault GetAccessDefault() const noexcept { return m_AccessDefaultAllowed; }

		void Clear() noexcept;

		Result<Vector<PeerSettings>> GetPeers() const noexcept;

	private:
		bool ValidatePeerAccessSettings(const PeerSettings& pas) const noexcept;

	private:
		const Settings_CThS& m_Settings;

		PeerAccessDetailsMap m_PeerAccessDetails;
		PeerAccessDefault m_AccessDefaultAllowed{ PeerAccessDefault::NotAllowed };
	};

	using PeerAccessControl_ThS = Concurrency::ThreadSafe<PeerAccessControl, std::shared_mutex>;
}