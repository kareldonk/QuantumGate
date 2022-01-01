// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Attacks.h"
#include "Common\Util.h"

using namespace QuantumGate::Implementation;
using namespace std::literals;

Attacks::ThreadData Attacks::m_ConnectGarbageData;
Attacks::ThreadData Attacks::m_ConnectData;
Attacks::ThreadData Attacks::m_ConnectWaitData;

bool Attacks::StartConnectGarbageAttack(const Endpoint& endpoint)
{
	if (!m_ConnectGarbageData.Thread.joinable())
	{
		m_ConnectGarbageData.Stop = false;
		m_ConnectGarbageData.Thread = std::thread(Attacks::ConnectGarbageThreadProc, endpoint);
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

void Attacks::ConnectGarbageThreadProc(const Endpoint& endpoint)
{
	LogWarn(L"ConnectGarbage: attack starting for endpoint %s...", endpoint.GetString().c_str());

	while (!m_ConnectGarbageData.Stop)
	{
		AttackSocket socket(endpoint.GetAddressFamily(), AttackSocket::Type::Stream, endpoint.GetProtocol());

		if (socket.BeginConnect(endpoint))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(Util::GetPseudoRandomNumber(0, 500)));

			while (!m_ConnectGarbageData.Stop)
			{
				if (socket.UpdateIOStatus(0ms))
				{
					if (socket.GetIOStatus().HasException())
					{
						LogErr(L"ConnectGarbage: exception on endpoint %s: %s", endpoint.GetString().c_str(),
							   Util::GetSystemErrorString(socket.GetIOStatus().GetErrorCode()).c_str());
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
					else if (socket.GetIOStatus().CanRead())
					{
						Buffer buffer;
						if (!socket.Receive(buffer))
						{
							break;
						}
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			socket.Close();
		}
		else
		{
			LogErr(L"ConnectGarbage: failed to connect to endpoint %s", endpoint.GetString().c_str());
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	LogWarn(L"ConnectGarbage: stopping attack on endpoint %s...", endpoint.GetString().c_str());
}

bool Attacks::StartConnectAttack(const Endpoint& endpoint)
{
	if (!m_ConnectData.Thread.joinable())
	{
		m_ConnectData.Stop = false;
		m_ConnectData.Thread = std::thread(Attacks::ConnectThreadProc, endpoint);
		return true;
	}

	return false;
}

void Attacks::StopConnectAttack()
{
	if (m_ConnectData.Thread.joinable())
	{
		m_ConnectData.Stop = true;
		m_ConnectData.Thread.join();
	}
}

bool Attacks::IsConnectAttackRunning() noexcept
{
	return m_ConnectData.Thread.joinable();
}

void Attacks::ConnectThreadProc(const Endpoint& endpoint)
{
	LogWarn(L"ConnectAttack: attack starting for endpoint %s...", endpoint.GetString().c_str());

	while (!m_ConnectData.Stop)
	{
		AttackSocket socket(endpoint.GetAddressFamily(), AttackSocket::Type::Stream, endpoint.GetProtocol());

		if (socket.BeginConnect(endpoint))
		{
			while (!m_ConnectData.Stop)
			{
				if (socket.UpdateIOStatus(0ms))
				{
					if (socket.GetIOStatus().HasException())
					{
						LogErr(L"ConnectAttack: exception on endpoint %s: %s", endpoint.GetString().c_str(),
							   Util::GetSystemErrorString(socket.GetIOStatus().GetErrorCode()).c_str());
						break;
					}
					else if (socket.GetIOStatus().IsConnecting() && socket.GetIOStatus().CanWrite())
					{
						// If the socket becomes writable then the connection succeeded
						break;
					}
					else if (socket.GetIOStatus().CanRead())
					{
						Buffer buffer;
						if (!socket.Receive(buffer))
						{
							break;
						}
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			socket.Close();
		}
		else
		{
			LogErr(L"ConnectAttack: failed to connect to endpoint %s", endpoint.GetString().c_str());
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	LogWarn(L"ConnectAttack: stopping attack on endpoint %s...", endpoint.GetString().c_str());
}

bool Attacks::StartConnectWaitAttack(const Endpoint& endpoint)
{
	if (!m_ConnectWaitData.Thread.joinable())
	{
		m_ConnectWaitData.Stop = false;
		m_ConnectWaitData.Thread = std::thread(Attacks::ConnectWaitThreadProc, endpoint);
		return true;
	}

	return false;
}

void Attacks::StopConnectWaitAttack()
{
	if (m_ConnectWaitData.Thread.joinable())
	{
		m_ConnectWaitData.Stop = true;
		m_ConnectWaitData.Thread.join();
	}
}

bool Attacks::IsConnectWaitAttackRunning() noexcept
{
	return m_ConnectWaitData.Thread.joinable();
}

void Attacks::ConnectWaitThreadProc(const Endpoint& endpoint)
{
	LogWarn(L"ConnectWaitAttack: attack starting for endpoint %s...", endpoint.GetString().c_str());

	while (!m_ConnectWaitData.Stop)
	{
		AttackSocket socket(endpoint.GetAddressFamily(), AttackSocket::Type::Stream, endpoint.GetProtocol());

		if (socket.BeginConnect(endpoint))
		{
			while (!m_ConnectWaitData.Stop)
			{
				if (socket.UpdateIOStatus(0ms))
				{
					if (socket.GetIOStatus().HasException())
					{
						LogErr(L"ConnectWaitAttack: exception on endpoint %s: %s", endpoint.GetString().c_str(),
							   Util::GetSystemErrorString(socket.GetIOStatus().GetErrorCode()).c_str());
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
					else if (socket.GetIOStatus().CanRead())
					{
						Buffer buffer;
						if (!socket.Receive(buffer))
						{
							break;
						}
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			socket.Close();
		}
		else
		{
			LogErr(L"ConnectWaitAttack: failed to connect to endpoint %s", endpoint.GetString().c_str());
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	LogWarn(L"ConnectWaitAttack: stopping attack on endpoint %s...", endpoint.GetString().c_str());
}