// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AVExtender.h"

namespace QuantumGate::AVExtender
{
	Extender::Extender() noexcept :
		QuantumGate::Extender(UUID, String(L"QuantumGate Audio/Video Extender"))
	{
		if (!SetStartupCallback(MakeCallback(this, &Extender::OnStartup)) ||
			!SetPostStartupCallback(MakeCallback(this, &Extender::OnPostStartup)) ||
			!SetPreShutdownCallback(MakeCallback(this, &Extender::OnPreShutdown)) ||
			!SetShutdownCallback(MakeCallback(this, &Extender::OnShutdown)) ||
			!SetPeerEventCallback(MakeCallback(this, &Extender::OnPeerEvent)) ||
			!SetPeerMessageCallback(MakeCallback(this, &Extender::OnPeerMessage)))
		{
			LogErr(GetName() + L": couldn't set one or more extender callbacks");
		}
	}

	Extender::~Extender()
	{}

	const bool Extender::OnStartup()
	{
		LogDbg(L"Extender '" + GetName() + L"' starting...");

		// Return true if initialization was successful, otherwise return false and
		// QuantumGate won't be sending this extender any notifications
		return true;
	}

	void Extender::OnPostStartup()
	{
		LogDbg(L"Extender '" + GetName() + L"' running...");
	}

	void Extender::OnPreShutdown()
	{
		LogDbg(L"Extender '" + GetName() + L"' will begin shutting down...");
	}

	void Extender::OnShutdown()
	{
		LogDbg(L"Extender '" + GetName() + L"' shutting down...");
	}

	void Extender::OnPeerEvent(PeerEvent&& event)
	{
		String ev(L"Unknown");

		switch (event.GetType())
		{
			case PeerEventType::Connected:
			{
				ev = L"Connect";
				break;
			}
			case PeerEventType::Disconnected:
			{
				ev = L"Disconnect";
				break;
			}
			default:
			{
				assert(false);
			}
		}

		LogInfo(L"Extender '" + GetName() + L"' got peer event: %s, Peer LUID: %llu", ev.c_str(), event.GetPeerLUID());
	}

	const std::pair<bool, bool> Extender::OnPeerMessage(PeerEvent&& event)
	{
		assert(event.GetType() == PeerEventType::Message);

		auto handled = false;
		auto success = false;

		auto msgdata = event.GetMessageData();
		if (msgdata != nullptr)
		{
		}

		return std::make_pair(handled, success);
	}
}