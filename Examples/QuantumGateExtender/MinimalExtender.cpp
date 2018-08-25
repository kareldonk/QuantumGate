// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "MinimalExtender.h"

#include <iostream>

MinimalExtender::MinimalExtender() :
	QuantumGate::Extender(QuantumGate::ExtenderUUID(L"2ddd4019-e6d1-09a5-2ec7-9c51af0304cb"),
						  QuantumGate::String(L"QuantumGate Minimal Extender"))
{
	// Add the callback functions for this extender; this can also be done
	// in another function instead of the constructor, as long as you set the callbacks
	// before adding the extender to the local instance
	if (!SetStartupCallback(QuantumGate::MakeCallback(this, &MinimalExtender::OnStartup)) ||
		!SetPostStartupCallback(QuantumGate::MakeCallback(this, &MinimalExtender::OnPostStartup)) ||
		!SetPreShutdownCallback(QuantumGate::MakeCallback(this, &MinimalExtender::OnPreShutdown)) ||
		!SetShutdownCallback(QuantumGate::MakeCallback(this, &MinimalExtender::OnShutdown)) ||
		!SetPeerEventCallback(QuantumGate::MakeCallback(this, &MinimalExtender::OnPeerEvent)) ||
		!SetPeerMessageCallback(QuantumGate::MakeCallback(this, &MinimalExtender::OnPeerMessage)))
	{
		throw std::exception("Failed to set one or more extender callbacks");
	}
}

MinimalExtender::~MinimalExtender()
{}

const bool MinimalExtender::OnStartup()
{
	// This function gets called by the QuantumGate instance to notify
	// an extender to initialize and startup

	std::wcout << L"MinimalExtender::OnStartup() called...\r\n";

	// Return true if initialization was succesful, otherwise return false and
	// QuantumGate won't be sending this extender any notifications
	return true;
}

void MinimalExtender::OnPostStartup()
{
	// This function gets called by the QuantumGate instance to notify
	// an extender of the fact that the startup procedure for this extender has
	// been completed successfully and the extender can now interact with the instance

	std::wcout << L"MinimalExtender::OnPostStartup() called...\r\n";
}

void MinimalExtender::OnPreShutdown()
{
	// This callback function gets called by the QuantumGate instance to notify
	// an extender that the shut down procedure has been initiated for this extender.
	// The extender should stop all activity and prepare for deinitialization before
	// returning from this function.

	std::wcout << L"MinimalExtender::OnPreShutdown() called...\r\n";
}

void MinimalExtender::OnShutdown()
{
	// This callback function gets called by the QuantumGate instance to notify an
	// extender that it has been shut down completely and should now deinitialize and
	// free resources

	std::wcout << L"MinimalExtender::OnShutdown() called...\r\n";
}

void MinimalExtender::OnPeerEvent(QuantumGate::PeerEvent&& event)
{
	// This callback function gets called by the QuantumGate instance to notify an
	// extender of a peer event

	std::wstring ev(L"Unknown");

	if (event.GetType() == QuantumGate::PeerEventType::Connected) ev = L"Connect";
	else if (event.GetType() == QuantumGate::PeerEventType::Disconnected) ev = L"Disconnect";

	std::wcout << L"MinimalExtender::OnPeerEvent() got peer event '" << ev <<
		L"' for peer LUID " << event.GetPeerLUID() << L"\r\n";

	// Send a simple hello message to the peer
	if (event.GetType() == QuantumGate::PeerEventType::Connected)
	{
		const wchar_t msg[]{ L"Hello peer, welcome!" };
		QuantumGate::Buffer msg_buf(reinterpret_cast<const QuantumGate::Byte*>(&msg), sizeof(msg) * sizeof(wchar_t));

		const auto result = SendMessageTo(event.GetPeerLUID(), std::move(msg_buf));
		if (result.Succeeded())
		{
			std::wcout << L"MinimalExtender sent hello to peer LUID " << event.GetPeerLUID() << L"\r\n";
		}
		else
		{
			std::wcout << L"MinimalExtender failed to send hello to peer LUID " <<
				event.GetPeerLUID() << L" (" << result << L")\r\n";
		}
	}
}

const std::pair<bool, bool> MinimalExtender::OnPeerMessage(QuantumGate::PeerEvent&& event)
{
	// This callback function gets called by the QuantumGate instance to notify an
	// extender of a peer message event

	std::wcout << L"MinimalExtender::OnPeerMessage() called...\r\n";

	auto handled = false; // Should be true if message was recognized, otherwise false
	auto success = false; // Should be true if message was handled successfully, otherwise false

	if (event.GetMessageData())
	{
		std::wcout << L"MinimalExtender received message from peer LUID " << event.GetPeerLUID() <<
			L": " << reinterpret_cast<const wchar_t*>(event.GetMessageData()->GetBytes()) << L"\r\n";

		handled = true;
		success = true;
	}

	return std::make_pair(handled, success);
}
