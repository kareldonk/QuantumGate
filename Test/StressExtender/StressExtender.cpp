// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "StressExtender.h"

#include <cassert>

#include "Console.h"
#include "Common\Util.h"
#include "Memory\BufferWriter.h"
#include "Memory\BufferReader.h"

namespace StressExtender
{
	using namespace QuantumGate::Implementation;
	using namespace QuantumGate::Implementation::Memory;

	Extender::Extender() :
		QuantumGate::Extender(ExtenderUUID(L"2ddd4019-e6d1-09a5-2ec7-9c51af0304cb"),
							  String(L"QuantumGate Stress Extender"))
	{
		LogWarn(L"Constructor called for QGStress Extender");

		if (!SetStartupCallback(MakeCallback(this, &Extender::OnStartup)) ||
			!SetPostStartupCallback(MakeCallback(this, &Extender::OnPostStartup)) ||
			!SetPreShutdownCallback(MakeCallback(this, &Extender::OnPreShutdown)) ||
			!SetShutdownCallback(MakeCallback(this, &Extender::OnShutdown)) ||
			!SetPeerEventCallback(MakeCallback(this, &Extender::OnPeerEvent)) ||
			!SetPeerMessageCallback(MakeCallback(this, &Extender::OnPeerMessage)))
		{
			throw std::exception("Failed to set one or more extender callbacks");
		}
	}

	Extender::~Extender()
	{
		LogWarn(L"Destructor called for QGStress Extender");
	}

	bool Extender::OnStartup()
	{
		LogDbg(L"Extender '%s' starting...", GetName().c_str());

		if (m_ExceptionTest.Startup) throw(std::exception("Test Startup exception"));

		// Return true if initialization was successful, otherwise return false and
		// QuantumGate won't be sending this extender any notifications
		return true;
	}

	void Extender::OnPostStartup()
	{
		LogDbg(L"Extender '%s' running...", GetName().c_str());

		if (m_ExceptionTest.PostStartup) throw(std::exception("Test PostStartup exception"));
	}

	void Extender::OnPreShutdown()
	{
		if (m_ExceptionTest.PreShutdown) throw(std::exception("Test PreShutdown exception"));
	}

	void Extender::OnShutdown()
	{
		LogDbg(L"Extender '%s' shutting down...", GetName().c_str());

		if (m_ExceptionTest.Shutdown) throw(std::exception("Test Shutdown exception"));

		// Deinitialization and cleanup here
	}

	void Extender::OnPeerEvent(PeerEvent&& event)
	{
		String ev(L"Unknown");

		if (event.GetType() == PeerEvent::Type::Connected) ev = L"Connect";
		else if (event.GetType() == PeerEvent::Type::Disconnected) ev = L"Disconnect";

		LogInfo(L"Extender '%s' got peer event: %s, Peer LUID: %llu", GetName().c_str(), ev.c_str(), event.GetPeerLUID());

		if (m_ExceptionTest.PeerEvent) throw(std::exception("Test PeerEvent exception"));
	}

	QuantumGate::Extender::PeerEvent::Result Extender::OnPeerMessage(PeerEvent&& event)
	{
		if (m_ExceptionTest.PeerMessage) throw(std::exception("Test PeerMessage exception"));

		PeerEvent::Result result;

		if (event.GetType() == PeerEvent::Type::Message)
		{
			auto msgdata = event.GetMessageData();
			if (msgdata != nullptr)
			{
				UInt16 mtype = 0;
				BufferReader rdr(*msgdata, true);

				// Get message type first
				if (rdr.Read(mtype))
				{
					const auto type = static_cast<MessageType>(mtype);
					if (type == MessageType::MessageString)
					{
						result.Handled = true;

						String str;
						str.resize((msgdata->GetSize() - sizeof(UInt16)) / sizeof(String::value_type));

						if (rdr.Read(str))
						{
							SLogInfo(L"Message from " << event.GetPeerLUID() << L": " <<
									 SLogFmt(FGBrightCyan) << str << SLogFmt(Default));

							result.Success = true;
						}
					}
					else if (type == MessageType::BenchmarkStart)
					{
						result.Handled = true;

						if (m_IsPeerBenchmarking)
						{
							LogErr(L"There's already a peer benchmark running");
						}
						else
						{
							m_IsPeerBenchmarking = true;
							m_PeerBenchmarkStart = std::chrono::high_resolution_clock::now();
							result.Success = true;
						}
					}
					else if (type == MessageType::BenchmarkEnd)
					{
						result.Handled = true;

						if (!m_IsPeerBenchmarking)
						{
							LogErr(L"There was no peer benchmark running");
						}
						else
						{
							m_IsPeerBenchmarking = false;
							auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_PeerBenchmarkStart);

							LogSys(L"Peer %s benchmark result: %dms", GetName().c_str(), ms.count());
							result.Success = true;
						}
					}
					else LogErr(L"Received unknown msgtype from %llu: %u", event.GetPeerLUID(), type);
				}
			}
		}
		else
		{
			LogErr(L"Unknown peer event from %llu: %u", event.GetPeerLUID(), event.GetType());
		}

		return result;
	}

	bool Extender::SendMessage(const PeerLUID pluid, const String& msg, const SendParameters::PriorityOption priority,
							   const std::chrono::milliseconds delay) const
	{
		const UInt16 msgtype = static_cast<UInt16>(MessageType::MessageString);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, msg))
		{
			SendParameters params;
			params.Compress = m_UseCompression;
			params.Priority = priority;
			params.Delay = delay;

			if (const auto result = SendMessageTo(pluid, writer.MoveWrittenBytes(), params); result.Failed())
			{
				LogErr(L"Could not send message to peer (%s)", result.GetErrorString().c_str());
				return false;
			}
		}

		return true;
	}

	bool Extender::SendBenchmarkStart(const PeerLUID pluid) noexcept
	{
		if (m_IsLocalBenchmarking)
		{
			LogErr(L"There's already a benchmark running");
			return false;
		}

		constexpr UInt16 msgtype = static_cast<UInt16>(MessageType::BenchmarkStart);

		BufferWriter writer(true);
		if (writer.Write(msgtype))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
			{
				m_IsLocalBenchmarking = true;
				m_LocalBenchmarkStart = std::chrono::high_resolution_clock::now();

				return true;
			}

			LogErr(L"Could not send benchmark start message to peer");
		}

		return false;
	}

	bool Extender::SendBenchmarkEnd(const PeerLUID pluid) noexcept
	{
		if (!m_IsLocalBenchmarking)
		{
			LogErr(L"There is no benchmark running");
			return false;
		}
		else
		{
			m_IsLocalBenchmarking = false;
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_LocalBenchmarkStart);

			LogSys(L"Local %s benchmark result: %dms", GetName().c_str(), ms.count());
		}

		const UInt16 msgtype = static_cast<UInt16>(MessageType::BenchmarkEnd);

		BufferWriter writer(true);
		if (writer.Write(msgtype))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded()) return true;

			LogErr(L"Could not send benchmark end message to peer");
		}

		return false;
	}

	bool Extender::BenchmarkSendMessage(const PeerLUID pluid)
	{
		LogSys(L"%s starting messaging benchmark", GetName().c_str());

		if (!SendBenchmarkStart(pluid)) return false;

		for (auto x = 0u; x < 100'000u; ++x)
		{
			if (!SendMessage(pluid, L"Hello world",
							 QuantumGate::SendParameters::PriorityOption::Normal, std::chrono::milliseconds(0)))
			{
				return false;
			}
		}

		if (!SendBenchmarkEnd(pluid)) return false;

		return true;
	}
}