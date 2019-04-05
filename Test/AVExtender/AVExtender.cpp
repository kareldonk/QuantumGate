// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AVExtender.h"

namespace QuantumGate::AVExtender
{
	Extender::Extender(HWND hwnd) :
		QuantumGate::Extender(UUID, String(L"QuantumGate Audio/Video Extender"))
	{
		m_Window = hwnd;

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
		LogDbg(GetName() + L": starting...");

		if (m_Window)
		{
			PostMessage(m_Window, static_cast<UINT>(WindowsMessage::ExtenderInit), 0, 0);
		}

		// Return true if initialization was successful, otherwise return false and
		// QuantumGate won't be sending this extender any notifications
		return true;
	}

	void Extender::OnPostStartup()
	{
		LogDbg(GetName() + L": running...");
	}

	void Extender::OnPreShutdown()
	{
		LogDbg(GetName() + L": will begin shutting down...");
	}

	void Extender::OnShutdown()
	{
		LogDbg(GetName() + L": shutting down...");

		if (m_Window)
		{
			PostMessage(m_Window, static_cast<UINT>(WindowsMessage::ExtenderDeinit), 0, 0);
		}
	}

	void Extender::OnPeerEvent(PeerEvent&& event)
	{
		String ev(L"Unknown");

		if (event.GetType() == PeerEventType::Connected)
		{
			ev = L"Connect";

			auto peer = std::make_unique<Peer>();
			peer->ID = event.GetPeerLUID();

			m_Peers.WithUniqueLock()->insert({ event.GetPeerLUID(), std::move(peer) });
		}
		else if (event.GetType() == PeerEventType::Disconnected)
		{
			ev = L"Disconnect";

			m_Peers.WithUniqueLock()->erase(event.GetPeerLUID());
		}

		LogInfo(GetName() + L": got peer event: %s, Peer LUID: %llu", ev.c_str(), event.GetPeerLUID());

		if (m_Window != nullptr)
		{
			// Must be deallocated in message handler
			Event* ev = new Event({ event.GetType(), event.GetPeerLUID() });

			// Using PostMessage because the current QuantumGate worker thread should NOT be calling directly to the UI;
			// only the thread that created the Window should do that, to avoid deadlocks
			if (!PostMessage(m_Window, static_cast<UINT>(WindowsMessage::PeerEvent), reinterpret_cast<WPARAM>(ev), 0))
			{
				delete ev;
			}
		}
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