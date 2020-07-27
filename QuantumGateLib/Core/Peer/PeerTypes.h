// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Concurrency\RecursiveSharedMutex.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class Peer;

	using Peer_ThS = Concurrency::ThreadSafe<Peer, Concurrency::RecursiveSharedMutex>;
	using PeerSharedPointer = std::shared_ptr<Peer_ThS>;
	using PeerWeakPointer = PeerSharedPointer::weak_type;
}