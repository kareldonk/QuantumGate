// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Extender.h"
#include "..\Core\Extender\Extender.h"

namespace QuantumGate::API
{
	Extender::Extender(const ExtenderUUID& uuid, const String& name)
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

	bool Extender::IsRunning() const noexcept
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

	Result<> Extender::DisconnectFrom(Peer& peer) noexcept
	{
		return m_Extender->DisconnectFrom(peer);
	}

	Result<> Extender::DisconnectFrom(Peer& peer, DisconnectCallback&& function) noexcept
	{
		return m_Extender->DisconnectFrom(peer, std::move(function));
	}

	Result<> Extender::SendMessageTo(const PeerLUID pluid, Buffer&& buffer, const bool compress) const noexcept
	{
		return m_Extender->SendMessageTo(pluid, std::move(buffer), compress);
	}

	Result<> Extender::SendMessageTo(const PeerLUID pluid, Buffer&& buffer, const SendParameters& params) const noexcept
	{
		return m_Extender->SendMessageTo(pluid, std::move(buffer), params);
	}

	Result<> Extender::SendMessageTo(Peer& peer, Buffer&& buffer, const bool compress) const noexcept
	{
		return m_Extender->SendMessageTo(peer, std::move(buffer), compress);
	}

	Result<> Extender::SendMessageTo(Peer& peer, Buffer&& buffer, const SendParameters& params) const noexcept
	{
		return m_Extender->SendMessageTo(peer, std::move(buffer), params);
	}

	Size Extender::GetMaximumMessageDataSize() noexcept
	{
		return QuantumGate::Implementation::Core::Extender::Extender::GetMaximumMessageDataSize();
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

	Result<Peer> Extender::GetPeer(const PeerLUID pluid) const noexcept
	{
		return m_Extender->GetPeer(pluid);
	}

	Result<Vector<PeerLUID>> Extender::QueryPeers(const PeerQueryParameters& params) const noexcept
	{
		return m_Extender->QueryPeers(params);
	}

	Result<> Extender::QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept
	{
		return m_Extender->QueryPeers(params, pluids);
	}

	Result<> Extender::SetStartupCallback(StartupCallback&& function) noexcept
	{
		return m_Extender->SetStartupCallback(std::move(function));
	}

	Result<> Extender::SetPostStartupCallback(PostStartupCallback && function) noexcept
	{
		return m_Extender->SetPostStartupCallback(std::move(function));
	}

	Result<> Extender::SetPreShutdownCallback(PreShutdownCallback && function) noexcept
	{
		return m_Extender->SetPreShutdownCallback(std::move(function));
	}

	Result<> Extender::SetShutdownCallback(ShutdownCallback&& function) noexcept
	{
		return m_Extender->SetShutdownCallback(std::move(function));
	}

	Result<> Extender::SetPeerEventCallback(PeerEventCallback&& function) noexcept
	{
		return m_Extender->SetPeerEventCallback(std::move(function));
	}

	Result<> Extender::SetPeerMessageCallback(PeerMessageCallback&& function) noexcept
	{
		return m_Extender->SetPeerMessageCallback(std::move(function));
	}
}