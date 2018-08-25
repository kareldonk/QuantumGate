// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Attacks.h"
#include "Common\Util.h"

using namespace QuantumGate::Implementation;
using namespace std::literals;

Attacks::ConnectGarbageData Attacks::m_ConnectGarbageData;

const bool Attacks::StartConnectGarbageAttack(const CString& ip, const UInt16 port)
{
	if (!m_ConnectGarbageData.Thread.joinable())
	{
		m_ConnectGarbageData.Stop = false;
		m_ConnectGarbageData.Thread = std::thread(Attacks::ConnectGarbageThreadProc, ip, port);
		return true;
	}

	return false;
}

void Attacks::StopConnectGarbageAttack()
{
	if (m_ConnectGarbageData.Thread.joinable())
	{
		m_ConnectGarbageData.Stop = true;
		m_ConnectGarbageData.Thread.join();
	}
}

bool Attacks::IsConnectGarbageAttackRunning() noexcept
{
	return m_ConnectGarbageData.Thread.joinable();
}

void Attacks::ConnectGarbageThreadProc(const CString ip, const UInt16 port)
{
	while (!m_ConnectGarbageData.Stop)
	{
		IPEndpoint endpoint(IPAddress((LPCWSTR)ip), port);
		AttackSocket socket(endpoint.GetIPAddress().GetFamily(), SOCK_STREAM, IPPROTO_TCP);
		
		if (socket.BeginConnect(endpoint))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(Util::GetPseudoRandomNumber(0, 500)));

			while (!m_ConnectGarbageData.Stop)
			{
				if (socket.UpdateIOStatus(0ms))
				{
					if (socket.GetIOStatus().HasException())
					{
						break;
					}
					else if (socket.GetIOStatus().IsConnecting() && socket.GetIOStatus().CanWrite())
					{
						// If the socket becomes writable then the connection succeeded;
						// complete the connection attempt
						if (!socket.CompleteConnect())
						{
							break;
						}
					}
					else if (socket.GetIOStatus().CanWrite())
					{
						Buffer buffer(Util::GetPseudoRandomBytes(static_cast<Size>(Util::GetPseudoRandomNumber(0, 4096))));
						if (!socket.Send(buffer))
						{
							break;
						}
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			socket.Close();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}