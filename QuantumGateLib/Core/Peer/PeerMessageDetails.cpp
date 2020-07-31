// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerMessageDetails.h"
#include "Peer.h"

namespace QuantumGate::Implementation::Core::Peer
{
	MessageDetails::MessageRate::MessageRate(Peer& peer, const MessageType type, const Size size) noexcept :
		m_MessageType(type), m_MessageSize(size), m_PeerPointer(peer.GetWeakPointer())
	{
		peer.GetReceiveQueues().AddMessageRate(m_MessageType, m_MessageSize);
	}

	MessageDetails::MessageRate::MessageRate(MessageRate&& other) noexcept :
		m_MessageType(other.m_MessageType), m_MessageSize(other.m_MessageSize), m_PeerPointer(std::move(other.m_PeerPointer))
	{
		other.m_MessageType = MessageType::Unknown;
		other.m_MessageSize = 0;
	}

	MessageDetails::MessageRate::~MessageRate()
	{
		auto peer_ths = m_PeerPointer.lock();
		if (peer_ths)
		{
			peer_ths->WithUniqueLock()->GetReceiveQueues().SubtractMessageRate(m_MessageType, m_MessageSize);
		}
	}

	MessageDetails::MessageRate& MessageDetails::MessageRate::operator=(MessageRate&& other) noexcept
	{
		// Check for same object
		if (this == &other) return *this;

		m_MessageType = std::exchange(other.m_MessageType, MessageType::Unknown);
		m_MessageSize = std::exchange(other.m_MessageSize, 0);
		m_PeerPointer = std::move(other.m_PeerPointer);

		return *this;
	}

	void MessageDetails::MessageRate::AddToMessageSize(const Size size)
	{
		auto peer_ths = m_PeerPointer.lock();
		if (peer_ths)
		{
			m_MessageSize += size;

			peer_ths->WithUniqueLock()->GetReceiveQueues().AddMessageRate(m_MessageType, size);
		}
	}

	MessageDetails::MessageDetails(Peer& peer, const MessageType type, const ExtenderUUID& extuuid, Buffer&& msgdata) :
		m_MessageType(type), m_ExtenderUUID(extuuid), m_MessageData(std::move(msgdata)),
		m_MessageRate(peer, m_MessageType, m_MessageData.GetSize())
	{}

	MessageDetails::~MessageDetails()
	{}

	bool MessageDetails::AddToMessageData(const Buffer& data) noexcept
	{
		try
		{
			m_MessageData += data;

			m_MessageRate.AddToMessageSize(data.GetSize());

			return true;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Failed to add data to message due to exception - %s", Util::ToStringW(e.what()).c_str());
		}
		catch (...) {}

		return false;
	}
}