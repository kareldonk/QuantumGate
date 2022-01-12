// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

// Include the QuantumGate main header with API definitions
#include <QuantumGate.h>

class BluetoothMessengerExtender final : public QuantumGate::Extender
{
public:
	BluetoothMessengerExtender() :
		QuantumGate::Extender(QuantumGate::ExtenderUUID(L"7BDCA67B-47B5-B96E-4E8C-B4B802805247"),
							  QuantumGate::String(L"QuantumGate Bluetooth Messenger Extender"))
	{
		// Add the callback functions that we use for this extender; this can also be done
		// in another function instead of the constructor, as long as you set the callbacks
		// before adding the extender to the local instance
		if (!SetPeerEventCallback(QuantumGate::MakeCallback(this, &BluetoothMessengerExtender::OnPeerEvent)) ||
			!SetPeerMessageCallback(QuantumGate::MakeCallback(this, &BluetoothMessengerExtender::OnPeerMessage)))
		{
			throw std::exception("Failed to set extender callbacks");
		}
	};

	virtual ~BluetoothMessengerExtender() {};

	void SendMessage(const QuantumGate::PeerLUID pluid, const std::wstring& msg, const int num_times) const
	{
		for (int x = 0; x < num_times; ++x)
		{
			QuantumGate::Buffer msg_buf(reinterpret_cast<const QuantumGate::Byte*>(msg.data()),
										msg.size() * sizeof(std::wstring::value_type));

			const auto result = SendMessageTo(pluid, std::move(msg_buf), {});
			if (result.Failed())
			{
				std::wcout << L"Failed to send message to peer LUID " << pluid << L" (" << result << L")\r\n";
				return;
			}
		}

		std::wcout << L"Message sent to peer LUID " << pluid << L"\r\n";
	}

protected:
	void OnPeerEvent(QuantumGate::Extender::PeerEvent&& event)
	{
		// This callback function gets called by the QuantumGate instance to notify an
		// extender of a peer event

		std::wstring ev(L"unknown event");

		switch (event.GetType())
		{
			case QuantumGate::Extender::PeerEvent::Type::Connected:
				ev = L"connected";
				break;
			case QuantumGate::Extender::PeerEvent::Type::Suspended:
				ev = L"suspended";
				break;
			case QuantumGate::Extender::PeerEvent::Type::Resumed:
				ev = L"resumed";
				break;
			case QuantumGate::Extender::PeerEvent::Type::Disconnected:
				ev = L"disconnected";
				break;
			default:
				break;
		}

		std::wcout << L"\x1b[95m";
		std::wcout << L"Peer with LUID " << event.GetPeerLUID() << L" has " << ev << L"\r\n";
		std::wcout << L"\x1b[39m";
	}

	QuantumGate::Extender::PeerEvent::Result OnPeerMessage(QuantumGate::Extender::PeerEvent&& event)
	{
		// This callback function gets called by the QuantumGate instance to notify an
		// extender of a peer message event

		QuantumGate::Extender::PeerEvent::Result result;

		if (const auto msgdata = event.GetMessageData(); msgdata != nullptr)
		{
			std::wstring msg;
			msg.resize(msgdata->GetSize() / sizeof(std::wstring::value_type));

			std::memcpy(msg.data(), msgdata->GetBytes(), msgdata->GetSize());

			std::wcout << L"Received message from peer LUID " << event.GetPeerLUID() << L": ";
			std::wcout << L"\x1b[92m";
			std::wcout << msg << L"\r\n";
			std::wcout << L"\x1b[39m";

			result.Handled = true; // Should be true if message was recognized, otherwise false
			result.Success = true; // Should be true if message was handled successfully, otherwise false
		}

		// If we return false for Handled and Success too often,
		// QuantumGate will disconnect the misbehaving peer eventually
		// as its reputation declines
		return result;
	}
};

