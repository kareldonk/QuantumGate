// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "RelaySocket.h"
#include "RelayManager.h"

namespace QuantumGate::Implementation::Core::Relay
{
	Socket::Socket() noexcept
	{
		m_ConnectedSteadyTime = Util::GetCurrentSteadyTime();
		m_IOStatus.SetOpen(true);

		// Always set (unused)
		m_ReceiveEvent.GetSubEvent(1).Set();
	}

	Socket::~Socket()
	{
		if (m_IOStatus.IsOpen()) Close();
	}

	bool Socket::BeginAccept(const RelayPort rport, const RelayHop hop,
							 const Endpoint& lendpoint, const Endpoint& pendpoint) noexcept
	{
		assert(m_IOStatus.IsOpen());
		assert(lendpoint.GetType() == pendpoint.GetType());

		switch (lendpoint.GetType())
		{
			case Endpoint::Type::IP:
			{
				const auto& lep = lendpoint.GetIPEndpoint();
				const auto& pep = pendpoint.GetIPEndpoint();

				assert(lep.GetProtocol() == pep.GetProtocol());

				m_LocalEndpoint = IPEndpoint(lep.GetProtocol(), lep.GetIPAddress(), lep.GetPort(), rport, hop);
				m_PeerEndpoint = IPEndpoint(pep.GetProtocol(), pep.GetIPAddress(), pep.GetPort(), rport, hop);
				break;
			}
			case Endpoint::Type::BTH:
			{
				const auto& lep = lendpoint.GetBTHEndpoint();
				const auto& pep = pendpoint.GetBTHEndpoint();

				assert(lep.GetProtocol() == pep.GetProtocol());

				m_LocalEndpoint = BTHEndpoint(lep.GetProtocol(), lep.GetBTHAddress(), lep.GetPort(), lep.GetServiceClassID(), rport, hop);
				m_PeerEndpoint = BTHEndpoint(pep.GetProtocol(), pep.GetBTHAddress(), pep.GetPort(), pep.GetServiceClassID(), rport, hop);
				break;
			}
			default:
			{
				// Shouldn't get here
				assert(false);
				break;
			}
		}

		m_AcceptCallback();

		return true;
	}

	bool Socket::CompleteAccept() noexcept
	{
		assert(m_IOStatus.IsOpen());

		m_IOStatus.SetConnected(true);

		m_ConnectedSteadyTime = Util::GetCurrentSteadyTime();

		return m_ConnectCallback();
	}

	bool Socket::BeginConnect(const Endpoint& endpoint) noexcept
	{
		assert(m_IOStatus.IsOpen());

		m_IOStatus.SetConnecting(true);

		// Local endpoint is set by the Relay manager once
		// a connection has been established
		m_PeerEndpoint = endpoint;

		m_ConnectingCallback();

		return true;
	}

	bool Socket::CompleteConnect() noexcept
	{
		assert(m_IOStatus.IsOpen() && m_IOStatus.IsConnecting());

		m_IOStatus.SetConnecting(false);
		m_IOStatus.SetConnected(true);

		m_ConnectedSteadyTime = Util::GetCurrentSteadyTime();

		return m_ConnectCallback();
	}

	void Socket::SetLocalEndpoint(const Endpoint& endpoint, const RelayPort rport, const RelayHop hop) noexcept
	{
		switch (endpoint.GetType())
		{
			case Endpoint::Type::IP:
			{
				const auto& lep = endpoint.GetIPEndpoint();
				const auto& pep = m_PeerEndpoint.GetIPEndpoint();

				assert(lep.GetProtocol() == pep.GetProtocol());

				m_LocalEndpoint = IPEndpoint(lep.GetProtocol(), lep.GetIPAddress(), lep.GetPort(), rport, hop);
				m_PeerEndpoint = IPEndpoint(pep.GetProtocol(), pep.GetIPAddress(), pep.GetPort(), rport, hop);
				break;
			}
			case Endpoint::Type::BTH:
			{
				const auto& lep = endpoint.GetBTHEndpoint();
				const auto& pep = m_PeerEndpoint.GetBTHEndpoint();

				assert(lep.GetProtocol() == pep.GetProtocol());

				m_LocalEndpoint = BTHEndpoint(lep.GetProtocol(), lep.GetBTHAddress(), lep.GetPort(), lep.GetServiceClassID(), rport, hop);
				m_PeerEndpoint = BTHEndpoint(pep.GetProtocol(), pep.GetBTHAddress(), pep.GetPort(), pep.GetServiceClassID(), rport, hop);
				break;
			}
			default:
			{
				// Shouldn't get here
				assert(false);
				break;
			}
		}
	}

	Result<Size> Socket::Send(const BufferView& buffer, const Size /*max_snd_size*/) noexcept
	{
		assert(m_IOStatus.IsOpen() && m_IOStatus.IsConnected() && m_IOStatus.CanWrite());

		try
		{
			Size sent_size{ 0 };

			const auto available_size = std::invoke([&]() noexcept
			{
				Size size{ 0 };
				if (MaxSendBufferSize > m_SendBuffer.GetSize())
				{
					size = MaxSendBufferSize - m_SendBuffer.GetSize();
				}
				return size;
			});

			if (available_size >= buffer.GetSize())
			{
				m_SendBuffer += buffer;
				sent_size = buffer.GetSize();
			}
			else if (available_size > 0)
			{
				const auto pbuffer = buffer.GetFirst(available_size);
				m_SendBuffer += pbuffer;
				sent_size = pbuffer.GetSize();
			}
			else
			{
				// Send buffer is full, we'll try again later
				LogDbg(L"Relay socket send buffer full/unavailable for endpoint %s", GetPeerName().c_str());
			}

			if (sent_size > 0)
			{
				m_SendEvent.GetSubEvent(0).Set();

				m_BytesSent += sent_size;
			}

			return sent_size;
		}
		catch (const std::exception& e)
		{
			LogErr(L"Relay socket send exception for endpoint %s - %s",
				   GetPeerName().c_str(), Util::ToStringW(e.what()).c_str());

			SetException(WSAENOBUFS);
		}

		return ResultCode::Failed;
	}

	Result<Size> Socket::Receive(Buffer& buffer, const Size /* max_rcv_size */) noexcept
	{
		assert(m_IOStatus.IsOpen() && m_IOStatus.IsConnected() && m_IOStatus.CanRead());

		try
		{
			const auto bytesrcv = m_ReceiveBuffer.GetSize();

			if (bytesrcv == 0)
			{
				if (!m_ClosingRead) return 0;
				
				LogDbg(L"Relay socket connection closed for endpoint %s", GetPeerName().c_str());

				m_ReceiveEvent.GetSubEvent(0).Reset();
			}
			else
			{
				buffer += m_ReceiveBuffer;

				m_ReceiveBuffer.Clear();
				m_ReceiveEvent.GetSubEvent(0).Reset();

				m_BytesReceived += bytesrcv;

				return bytesrcv;
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Relay socket receive exception for endpoint %s - %s",
					GetPeerName().c_str(), Util::ToStringW(e.what()).c_str());

			SetException(WSAENOBUFS);
		}

		return ResultCode::Failed;
	}

	void Socket::Close(const bool linger) noexcept
	{
		assert(m_IOStatus.IsOpen());

		m_CloseCallback();

		m_IOStatus.Reset();
	}

	bool Socket::UpdateIOStatus(const std::chrono::milliseconds& mseconds) noexcept
	{
		assert(m_IOStatus.IsOpen());

		m_ReceiveEvent.GetSubEvent(0).Reset();

		if (!m_IOStatus.IsOpen()) return false;

		const bool write = (m_ConnectWrite && m_SendBuffer.GetSize() < MaxSendBufferSize && !m_IOStatus.IsSuspended());
		m_IOStatus.SetWrite(write);

		const bool read = (!m_ReceiveBuffer.IsEmpty() || m_ClosingRead);
		m_IOStatus.SetRead(read);

		if (read) m_ReceiveEvent.GetSubEvent(0).Set();

		return true;
	}

	SystemTime Socket::GetConnectedTime() const noexcept
	{
		const auto dif = std::chrono::duration_cast<std::chrono::seconds>(Util::GetCurrentSteadyTime() - GetConnectedSteadyTime());
		return (Util::GetCurrentSystemTime() - dif);
	}
}

