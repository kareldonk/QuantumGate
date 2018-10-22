// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerAccessControl.h"

namespace QuantumGate::Implementation::Core::Access
{
	PeerAccessControl::PeerAccessControl(const Settings_CThS& settings) noexcept :
		m_Settings(settings)
	{}

	Result<> PeerAccessControl::AddPeer(PeerAccessSettings&& pas) noexcept
	{
		if (!ValidatePeerAccessSettings(pas)) return ResultCode::InvalidArgument;

		try
		{
			const auto it = m_PeerAccessDetails.find(pas.UUID);
			if (it == m_PeerAccessDetails.end())
			{
				PeerAccessDetails pad;
				pad.PublicKey = std::move(pas.PublicKey);
				pad.AccessAllowed = pas.AccessAllowed;

				[[maybe_unused]] const auto[it, success] = m_PeerAccessDetails.insert({ pas.UUID, std::move(pad) });
				if (success) return ResultCode::Succeeded;
			}
			else return ResultCode::PeerAlreadyExists;
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<> PeerAccessControl::UpdatePeer(PeerAccessSettings&& pas) noexcept
	{
		if (!ValidatePeerAccessSettings(pas)) return ResultCode::InvalidArgument;

		try
		{
			const auto it = m_PeerAccessDetails.find(pas.UUID);
			if (it != m_PeerAccessDetails.end())
			{
				it->second.PublicKey = std::move(pas.PublicKey);
				it->second.AccessAllowed = pas.AccessAllowed;

				return ResultCode::Succeeded;
			}
			else return ResultCode::PeerNotFound;
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<> PeerAccessControl::RemovePeer(const PeerUUID& puuid) noexcept
	{
		if (!puuid.IsValid()) return ResultCode::InvalidArgument;

		try
		{
			if (m_PeerAccessDetails.erase(puuid) > 0) return ResultCode::Succeeded;
			else return ResultCode::PeerNotFound;
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	Result<bool> PeerAccessControl::IsAllowed(const PeerUUID& puuid) const noexcept
	{
		assert(puuid.IsValid());

		const auto it = m_PeerAccessDetails.find(puuid);
		if (it != m_PeerAccessDetails.end())
		{
			if (it->second.AccessAllowed)
			{
				// If we require authentication the peer must have
				// a public key set to be allowed
				if (!(m_Settings.GetCache().Local.RequireAuthentication &&
					  it->second.PublicKey.IsEmpty()))
				{
					return true;
				}
			}
		}
		else if (!m_Settings.GetCache().Local.RequireAuthentication &&
				 m_AccessDefaultAllowed == PeerAccessDefault::Allowed)
		{
			// If we don't know the peer, it is still allowed if we don't
			// require authentication and default access is set to allowed
			return true;
		}

		return false;
	}

	const ProtectedBuffer* PeerAccessControl::GetPublicKey(const PeerUUID& puuid) const noexcept
	{
		const auto it = m_PeerAccessDetails.find(puuid);
		if (it != m_PeerAccessDetails.end())
		{
			if (!it->second.PublicKey.IsEmpty()) return &it->second.PublicKey;
		}

		// Returns nullptr if the peer wasn't found or
		// if it didn't have a public key
		return nullptr;
	}

	void PeerAccessControl::Clear() noexcept
	{
		m_PeerAccessDetails.clear();
	}

	Result<Vector<PeerAccessSettings>> PeerAccessControl::GetPeers() const noexcept
	{
		try
		{
			Vector<PeerAccessSettings> peers;

			for (const auto& peer : m_PeerAccessDetails)
			{
				auto& pset = peers.emplace_back();
				pset.UUID = peer.first;
				pset.PublicKey = peer.second.PublicKey;
				pset.AccessAllowed = peer.second.AccessAllowed;
			}

			return std::move(peers);
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	const bool PeerAccessControl::ValidatePeerAccessSettings(const PeerAccessSettings& pas) const noexcept
	{
		if (!pas.UUID.IsValid()) return false;

		if (!pas.PublicKey.IsEmpty())
		{
			if (!pas.UUID.Verify(pas.PublicKey)) return false;
		}

		return true;
	}
}