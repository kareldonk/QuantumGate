// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

// Include the QuantumGate main header with API definitions
#include <QuantumGate.h>

class MinimalExtender final : public QuantumGate::Extender
{
public:
	MinimalExtender();
	virtual ~MinimalExtender();

protected:
	bool OnStartup();
	void OnPostStartup();
	void OnPreShutdown();
	void OnShutdown();
	void OnPeerEvent(QuantumGate::PeerEvent&& event);
	const std::pair<bool, bool> OnPeerMessage(QuantumGate::PeerEvent&& event);
};

