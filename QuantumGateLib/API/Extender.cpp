// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Extender.h"
#include "..\Core\Extender\Extender.h"

namespace QuantumGate::API
{
	Extender::Extender(const ExtenderUUID& uuid, const String& name) noexcept
	{
		m_Extender = std::make_shared<QuantumGate::Implementation::Core::Extender::Extender>(uuid, name);
	}

	const ExtenderUUID& Extender::GetUUID() const noexcept
	{
		return m_Extender->GetUUID();
	}

	const String& Extender::GetName() const noexcept
	{
		return m_Extender->GetName();
	}

	const bool Extender::IsRunning() const noexcept
	{
		return m_Extender->IsRunning();
	}

	Result<ConnectDetails> Extender::ConnectTo(ConnectParameters&& params) noexcept
	{
		return m_Extender->ConnectTo(std::move(params));
	}

	Result<std::pair<PeerLUID, bool>> Extender::ConnectTo(ConnectParameters&& params,
														  ConnectCallback&& function) noexcept
	{
		return m_Extender->ConnectTo(std::move(params), std::move(function));
	}

	Result<> Extender::DisconnectFrom(const PeerLUID pluid) noexcept
	{
		return m_Extender->DisconnectFrom(pluid);
	}

	Result<> Extender::DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept
	{
		return m_Extender->DisconnectFrom(pluid, std::move(function));
	}

	Result<> Extender::SendMessageTo(const PeerLUID pluid, Buffer&& buffer, const bool compress) const
	{
		return m_Extender->SendMessageTo(pluid, std::move(buffer), compress);
	}

	const Size Extender::GetMaximumMessageDataSize() const noexcept
	{
		return m_Extender->GetMaximumMessageDataSize();
	}

	Result<std::tuple<UInt, UInt, UInt, UInt>> Extender::GetLocalVersion() const noexcept
	{
		return m_Extender->GetLocalVersion();
	}

	Result<std::pair<UInt, UInt>> Extender::GetLocalProtocolVersion() const noexcept
	{
		return m_Extender->GetLocalProtocolVersion();
	}

	Result<PeerUUID> Extender::GetLocalUUID() const noexcept
	{
		return m_Extender->GetLocalUUID();
	}

	Result<PeerDetails> Extender::GetPeerDetails(const PeerLUID pluid) const noexcept
	{
		return m_Extender->GetPeerDetails(pluid);
	}

	Result<std::vector<PeerLUID>> Extender::QueryPeers(const PeerQueryParameters& settings) const noexcept
	{
		return m_Extender->QueryPeers(settings);
	}

	Result<> Extender::SetStartupCallback(ExtenderStartupCallback&& function) noexcept
	{
		return m_Extender->SetStartupCallback(std::move(function));
	}

	Result<> Extender::SetPostStartupCallback(ExtenderPostStartupCallback && function) noexcept
	{
		return m_Extender->SetPostStartupCallback(std::move(function));
	}

	Result<> Extender::SetPreShutdownCallback(ExtenderPreShutdownCallback && function) noexcept
	{
		return m_Extender->SetPreShutdownCallback(std::move(function));
	}

	Result<> Extender::SetShutdownCallback(ExtenderShutdownCallback&& function) noexcept
	{
		return m_Extender->SetShutdownCallback(std::move(function));
	}

	Result<> Extender::SetPeerEventCallback(ExtenderPeerEventCallback&& function) noexcept
	{
		return m_Extender->SetPeerEventCallback(std::move(function));
	}

	Result<> Extender::SetPeerMessageCallback(ExtenderPeerMessageCallback&& function) noexcept
	{
		return m_Extender->SetPeerMessageCallback(std::move(function));
	}
}