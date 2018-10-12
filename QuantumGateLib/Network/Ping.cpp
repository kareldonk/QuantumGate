// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Ping.h"
#include "..\Common\Random.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Network
{
	const bool Ping::Process() noexcept
	{
		ICMP::EchoMessageHeader icmp_hdr;
		icmp_hdr.Type = static_cast<UInt8>(ICMP::MessageType::Echo);
		icmp_hdr.Code = 0;
		icmp_hdr.Identifier = 369;
		icmp_hdr.SequenceNumber = 1;
		icmp_hdr.Checksum = 0;

		const auto icmp_data = Random::GetPseudoRandomBytes(32);

		Buffer icmp_msg(reinterpret_cast<Byte*>(&icmp_hdr), sizeof(icmp_hdr));
		icmp_msg += icmp_data;

		const auto checksum = ICMP::CalculateChecksum(icmp_msg);

		reinterpret_cast<ICMP::EchoMessageHeader*>(icmp_msg.GetBytes())->Checksum = checksum;

		LogDbg(L"Sending %llu bytes of data", icmp_msg.GetSize());

		m_Socket = Socket(IPAddressFamily::IPv4, Socket::Type::RAW, Socket::Protocol::ICMP);
		if (m_Socket.SendTo(IPEndpoint(m_IPAddress, 0), icmp_msg) &&
			icmp_msg.IsEmpty())
		{
			while (m_Socket.UpdateIOStatus(5000ms))
			{
				if (m_Socket.GetIOStatus().CanRead())
				{
					IPEndpoint endpoint;
					Buffer data;

					if (m_Socket.ReceiveFrom(endpoint, data))
					{
						LogDbg(L"Received %llu bytes of data", data.GetSize());
						break;
					}
				}
				else if (m_Socket.GetIOStatus().HasException())
				{
					LogErr(L"Exception while pinging IP address %s (%s)",
						   m_IPAddress.GetString().c_str(), GetSysErrorString(m_Socket.GetIOStatus().GetErrorCode()).c_str());
				}
				else if (m_Socket.GetIOStatus().CanWrite())
				{
					LogErr(L"Write");
				}
				else
				{
					LogErr(L"Ping timeout");
				}
			}
		}
		else
		{
			// Failed to send
		}

		return false;
	}

	const bool Ping::GetResult() noexcept
	{
		return false;
	}
}