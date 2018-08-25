// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Extender.h"
#include "..\Local.h"

namespace QuantumGate::Implementation::Core::Extender
{
	Extender::Extender(const ExtenderUUID& uuid, const String& name) noexcept :
		m_UUID(uuid), m_Name(name)
	{
		assert(uuid.GetType() == UUID::Type::Extender && !name.empty());
	}

	Result<ConnectDetails> Extender::ConnectTo(ConnectParameters&& params) noexcept
	{
		assert(IsRunning());

		return m_Local.load()->ConnectTo(std::move(params));
	}

	Result<std::pair<PeerLUID, bool>> Extender::ConnectTo(ConnectParameters&& params,
												ConnectCallback&& function) noexcept
	{
		assert(IsRunning());

		return m_Local.load()->ConnectTo(std::move(params), std::move(function));
	}

	Result<> Extender::DisconnectFrom(const PeerLUID pluid) noexcept
	{
		assert(IsRunning());

		return m_Local.load()->DisconnectFrom(pluid);
	}

	Result<> Extender::DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept
	{
		assert(IsRunning());

		return m_Local.load()->DisconnectFrom(pluid, std::move(function));
	}

	Result<> Extender::SendMessageTo(const PeerLUID pluid, Buffer&& buffer, const bool compress) const
	{
		assert(IsRunning());

		return m_Local.load()->SendTo(GetUUID(), m_Running, pluid, std::move(buffer), compress);
	}

	Result<PeerDetails> Extender::GetPeerDetails(const PeerLUID pluid) const noexcept
	{
		assert(IsRunning());

		return m_Local.load()->GetPeerDetails(pluid);
	}

	Result<std::vector<PeerLUID>> Extender::QueryPeers(const PeerQueryParameters& settings) const noexcept
	{
		assert(IsRunning());

		return m_Local.load()->QueryPeers(settings);
	}

	void Extender::OnException() noexcept
	{
		m_Exception = true;
		LogErr(L"Unknown exception in extender '%s' (UUID: %s)",
			   GetName().c_str(), GetUUID().GetString().c_str());
	}

	void Extender::OnException(const std::exception& e) noexcept
	{
		m_Exception = true;
		LogErr(L"Exception in extender '%s' (UUID: %s) - %s",
			   GetName().c_str(), GetUUID().GetString().c_str(), Util::ToStringW(e.what()).c_str());
	}

	Result<PeerUUID> Extender::GetLocalUUID() const noexcept
	{
		assert(m_Local.load() != nullptr);

		if (m_Local.load() != nullptr) return m_Local.load()->GetUUID();

		return ResultCode::ExtenderHasNoLocalInstance;
	}

	Result<std::tuple<UInt, UInt, UInt, UInt>> Extender::GetLocalVersion() const noexcept
	{
		assert(m_Local.load() != nullptr);

		if (m_Local.load() != nullptr) return m_Local.load()->GetVersion();

		return ResultCode::ExtenderHasNoLocalInstance;
	}

	Result<std::pair<UInt, UInt>> Extender::GetLocalProtocolVersion() const noexcept
	{
		assert(m_Local.load() != nullptr);

		if (m_Local.load() != nullptr) return m_Local.load()->GetProtocolVersion();

		return ResultCode::ExtenderHasNoLocalInstance;
	}
}
