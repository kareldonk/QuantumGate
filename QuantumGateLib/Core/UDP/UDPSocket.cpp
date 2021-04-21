// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPSocket.h"

namespace QuantumGate::Implementation::Core::UDP
{
	Socket::Socket() noexcept
	{
		m_ConnectedSteadyTime = Util::GetCurrentSteadyTime();
		m_IOStatus.SetOpen(true);
	}

	Socket::~Socket()
	{
		if (m_IOStatus.IsOpen()) Close();
	}

	bool Socket::Accept(const std::shared_ptr<Listener::SendQueue_ThS>& send_queue,
						const IPEndpoint& lendpoint, const IPEndpoint& pendpoint) noexcept
	{
		assert(m_IOStatus.IsOpen());
		assert(lendpoint.GetProtocol() == pendpoint.GetProtocol());

		m_ConnectionData->WithUniqueLock([&](auto& data)
		{
			data.SetConnectRequest();
			data.SetLocalEndpoint(lendpoint);
			data.SetPeerEndpoint(pendpoint);
			data.SetListenerSendQueue(send_queue);
		});

		UpdateSocketInfo();

		m_AcceptCallback();

		m_IOStatus.SetConnected(true);

		return m_ConnectCallback();
	}

	bool Socket::BeginConnect(const IPEndpoint& endpoint) noexcept
	{
		assert(GetIOStatus().IsOpen());

		m_IOStatus.SetConnecting(true);

		m_ConnectionData->WithUniqueLock([&](auto& data)
		{
			data.SetConnectRequest();
			data.SetPeerEndpoint(endpoint);
		});

		UpdateSocketInfo();

		m_ConnectingCallback();

		return true;
	}

	bool Socket::CompleteConnect() noexcept
	{
		assert(m_IOStatus.IsOpen() && m_IOStatus.IsConnecting());

		m_IOStatus.SetConnecting(false);
		m_IOStatus.SetConnected(true);

		UpdateSocketInfo();

		return m_ConnectCallback();
	}

	void Socket::SetException(const Int errorcode) noexcept
	{
		m_ConnectionData->WithUniqueLock()->SetException(errorcode);

		m_IOStatus.SetException(true);
		m_IOStatus.SetErrorCode(errorcode);
	}

	Result<Size> Socket::Send(const BufferView& buffer, const Size /*max_snd_size*/) noexcept
	{
		assert(m_IOStatus.IsOpen() && m_IOStatus.IsConnected() && m_IOStatus.CanWrite());

		try
		{
			Size sent_size{ 0 };

			m_ConnectionData->WithUniqueLock([&](auto& connection_data)
			{
				if (connection_data.GetSendBuffer().GetWriteSize() > 0)
				{
					sent_size = connection_data.GetSendBuffer().Write(buffer);
					
					connection_data.SignalSendEvent();

					m_BytesSent += sent_size;
				}
				else
				{
					// Send buffer is full, we'll try again later
					LogDbg(L"UDP socket send buffer full/unavailable for endpoint %s", GetPeerName().c_str());
				}
			});

			return sent_size;
		}
		catch (const std::exception& e)
		{
			LogErr(L"UDP socket send exception for endpoint %s - %s",
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
			auto connection_data = m_ConnectionData->WithUniqueLock();

			const auto max_rcv_size = connection_data->GetReceiveBuffer().GetReadSize();
			if (max_rcv_size > 0)
			{
				buffer.Resize(buffer.GetSize() + max_rcv_size);

				const auto rcv_size = connection_data->GetReceiveBuffer().
					Read(buffer.GetBytes() + (buffer.GetSize() - max_rcv_size), max_rcv_size);

				assert(max_rcv_size == rcv_size);

				connection_data->SetRead(false);

				m_BytesReceived += rcv_size;

				return rcv_size;
			}
			else
			{
				LogDbg(L"UDP socket connection closed for endpoint %s", GetPeerName().c_str());

				connection_data->ResetReceiveEvent();
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"UDP socket receive exception for endpoint %s - %s",
				   GetPeerName().c_str(), Util::ToStringW(e.what()).c_str());

			SetException(WSAENOBUFS);
		}

		return ResultCode::Failed;
	}

	void Socket::Close(const bool linger) noexcept
	{
		assert(m_IOStatus.IsOpen());

		m_CloseCallback();

		m_ConnectionData->WithUniqueLock()->SetCloseRequest();

		m_IOStatus.Reset();
	}

	bool Socket::UpdateIOStatus(const std::chrono::milliseconds& mseconds) noexcept
	{
		assert(m_IOStatus.IsOpen());
		
		if (!m_IOStatus.IsOpen()) return false;

		auto connection_data = m_ConnectionData->WithUniqueLock();

		connection_data->ResetReceiveEvent();

		m_IOStatus.SetRead(connection_data->CanRead() || connection_data->HasCloseRequest());
		m_IOStatus.SetWrite(connection_data->CanWrite() && !connection_data->IsSuspended());

		if (!m_IOStatus.IsSuspended() && connection_data->IsSuspended())
		{
			m_LastSuspendedSteadyTime = Util::GetCurrentSteadyTime();
			m_IOStatus.SetSuspended(true);
		}
		else if(m_IOStatus.IsSuspended() && !connection_data->IsSuspended())
		{
			m_LastResumedSteadyTime = Util::GetCurrentSteadyTime();
			m_IOStatus.SetSuspended(false);
		}

		if (connection_data->HasException())
		{
			m_IOStatus.SetException(true);
			m_IOStatus.SetErrorCode(connection_data->GetErrorCode());
		}

		return true;
	}

	SystemTime Socket::GetConnectedTime() const noexcept
	{
		const auto dif = std::chrono::duration_cast<std::chrono::seconds>(Util::GetCurrentSteadyTime() -
																		  GetConnectedSteadyTime());
		return (Util::GetCurrentSystemTime() - dif);
	}

	void Socket::UpdateSocketInfo() noexcept
	{
		m_ConnectedSteadyTime = Util::GetCurrentSteadyTime();

		auto connection_data = m_ConnectionData->WithSharedLock();
		m_LocalEndpoint = connection_data->GetLocalEndpoint();
		m_PeerEndpoint = connection_data->GetPeerEndpoint();
	}
}