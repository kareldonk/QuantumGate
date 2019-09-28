// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AVExtender.h"

#include <Common\Util.h>
#include <Common\ScopeGuard.h>
#include <Memory\BufferWriter.h>
#include <Memory\BufferReader.h>

using namespace QuantumGate::Implementation;
using namespace QuantumGate::Implementation::Memory;
using namespace std::literals;

namespace QuantumGate::AVExtender
{
	Extender::Extender(HWND hwnd) :
		m_Window(hwnd),
		QuantumGate::Extender(UUID, String(L"QuantumGate Audio/Video Extender"))
	{
		if (!SetStartupCallback(MakeCallback(this, &Extender::OnStartup)) ||
			!SetPostStartupCallback(MakeCallback(this, &Extender::OnPostStartup)) ||
			!SetPreShutdownCallback(MakeCallback(this, &Extender::OnPreShutdown)) ||
			!SetShutdownCallback(MakeCallback(this, &Extender::OnShutdown)) ||
			!SetPeerEventCallback(MakeCallback(this, &Extender::OnPeerEvent)) ||
			!SetPeerMessageCallback(MakeCallback(this, &Extender::OnPeerMessage)))
		{
			LogErr(L"%s: couldn't set one or more extender callbacks", GetName().c_str());
		}
	}

	Extender::~Extender()
	{}

	bool Extender::OnStartup()
	{
		LogDbg(L"%s: starting...", GetName().c_str());

		m_ShutdownEvent.Reset();

		m_Thread = std::thread(Extender::WorkerThreadLoop, this);

		if (m_Window)
		{
			PostMessage(m_Window, static_cast<UINT>(WindowsMessage::ExtenderInit), 0, 0);
		}

		// Return true if initialization was successful, otherwise return false and
		// QuantumGate won't be sending this extender any notifications
		return true;
	}

	void Extender::OnPostStartup()
	{
		LogDbg(L"%s: running...", GetName().c_str());
	}

	void Extender::OnPreShutdown()
	{
		LogDbg(L"%s: will begin shutting down...", GetName().c_str());

		StopAudioSourceReader();
	}

	void Extender::OnShutdown()
	{
		LogDbg(L"%s: shutting down...", GetName().c_str());

		// Set the shutdown event to notify thread that we're shutting down
		m_ShutdownEvent.Set();

		if (m_Thread.joinable())
		{
			// Wait for the thread to shut down
			m_Thread.join();
		}

		m_Peers.WithUniqueLock()->clear();

		if (m_Window)
		{
			PostMessage(m_Window, static_cast<UINT>(WindowsMessage::ExtenderDeinit), 0, 0);
		}
	}

	void Extender::OnPeerEvent(PeerEvent&& event)
	{
		String ev(L"Unknown");

		if (event.GetType() == PeerEventType::Connected)
		{
			ev = L"Connect";

			auto peer = std::make_unique<Peer>(event.GetPeerLUID());

			m_Peers.WithUniqueLock()->insert({ event.GetPeerLUID(), std::move(peer) });
		}
		else if (event.GetType() == PeerEventType::Disconnected)
		{
			ev = L"Disconnect";

			m_Peers.WithUniqueLock()->erase(event.GetPeerLUID());
		}

		LogInfo(L"%s: got peer event: %s, Peer LUID: %llu", GetName().c_str(), ev.c_str(), event.GetPeerLUID());

		if (m_Window != nullptr)
		{
			// Must be deallocated in message handler
			Event* ev = new Event({ event.GetType(), event.GetPeerLUID() });

			// Using PostMessage because the current QuantumGate worker thread should NOT be calling directly to the UI;
			// only the thread that created the Window should do that, to avoid deadlocks
			if (!PostMessage(m_Window, static_cast<UINT>(WindowsMessage::PeerEvent), reinterpret_cast<WPARAM>(ev), 0))
			{
				delete ev;
			}
		}
	}

	const std::pair<bool, bool> Extender::OnPeerMessage(PeerEvent&& event)
	{
		assert(event.GetType() == PeerEventType::Message);

		auto handled = false;
		auto success = false;

		auto msgdata = event.GetMessageData();
		if (msgdata != nullptr)
		{
			UInt16 mtype = 0;
			BufferReader rdr(*msgdata, true);

			// Get message type
			if (rdr.Read(mtype))
			{
				const auto type = static_cast<MessageType>(mtype);
				switch (type)
				{
					case MessageType::CallRequest:
					{
						Dbg(L"Received CallRequest message from %llu", event.GetPeerLUID());

						handled = true;

						Buffer buffer(sizeof(CallRequestData));
						if (rdr.Read(buffer))
						{
							IfGetCall(event.GetPeerLUID(), [&](auto& call)
							{
								const CallRequestData* crdata = reinterpret_cast<const CallRequestData*>(buffer.GetBytes());
								AudioFormat fmt;
								fmt.NumChannels = crdata->AudioFormat.NumChannels;
								fmt.SamplesPerSecond = crdata->AudioFormat.SamplesPerSecond;
								fmt.AvgBytesPerSecond = crdata->AudioFormat.AvgBytesPerSecond;
								fmt.BlockAlignment = crdata->AudioFormat.BlockAlignment;
								fmt.BitsPerSample = crdata->AudioFormat.BitsPerSample;

								call.SetPeerSendAudio(crdata->SendAudio == 1);
								call.SetPeerSendVideo(crdata->SendVideo == 1);
								call.SetPeerAudioFormat(fmt);

								if (call.ProcessIncomingCall())
								{
									success = true;

									if (m_Window != nullptr)
									{
										// Must be deallocated in message handler
										CallAccept* ca = new CallAccept(event.GetPeerLUID());

										if (!PostMessage(m_Window, static_cast<UINT>(WindowsMessage::AcceptIncomingCall),
														 reinterpret_cast<WPARAM>(ca), 0))
										{
											delete ca;
										}
									}
								}
								else
								{
									DiscardReturnValue(SendGeneralFailure(event.GetPeerLUID()));
								}
							});
						}

						if (!success)
						{
							LogErr(L"Couldn't process incoming call from peer %llu", event.GetPeerLUID());
						}
						break;
					}
					case MessageType::CallAccept:
					{
						Dbg(L"Received CallAccept message from %llu", event.GetPeerLUID());

						handled = true;

						Buffer buffer(sizeof(CallRequestData));
						if (rdr.Read(buffer))
						{
							IfGetCall(event.GetPeerLUID(), [&](auto& call)
							{
								const CallRequestData* crdata = reinterpret_cast<const CallRequestData*>(buffer.GetBytes());
								AudioFormat fmt;
								fmt.NumChannels = crdata->AudioFormat.NumChannels;
								fmt.SamplesPerSecond = crdata->AudioFormat.SamplesPerSecond;
								fmt.AvgBytesPerSecond = crdata->AudioFormat.AvgBytesPerSecond;
								fmt.BlockAlignment = crdata->AudioFormat.BlockAlignment;
								fmt.BitsPerSample = crdata->AudioFormat.BitsPerSample;

								call.SetPeerSendAudio(crdata->SendAudio == 1);
								call.SetPeerSendVideo(crdata->SendVideo == 1);
								call.SetPeerAudioFormat(fmt);

								if (call.AcceptCall())
								{
									success = true;
								}
								else
								{
									DiscardReturnValue(SendGeneralFailure(event.GetPeerLUID()));
								}
							});
						}

						if (!success)
						{
							LogErr(L"Couldn't accept outgoing call from peer %llu", event.GetPeerLUID());
						}
						break;
					}
					case MessageType::CallAVUpdate:
					{
						Dbg(L"Received CallAVUpdate message from %llu", event.GetPeerLUID());

						handled = true;

						Buffer buffer(sizeof(CallRequestData));
						if (rdr.Read(buffer))
						{
							IfGetCall(event.GetPeerLUID(), [&](auto& call)
							{
								const CallRequestData* crdata = reinterpret_cast<const CallRequestData*>(buffer.GetBytes());
								AudioFormat fmt;
								fmt.NumChannels = crdata->AudioFormat.NumChannels;
								fmt.SamplesPerSecond = crdata->AudioFormat.SamplesPerSecond;
								fmt.AvgBytesPerSecond = crdata->AudioFormat.AvgBytesPerSecond;
								fmt.BlockAlignment = crdata->AudioFormat.BlockAlignment;
								fmt.BitsPerSample = crdata->AudioFormat.BitsPerSample;

								call.SetPeerSendAudio(crdata->SendAudio == 1);
								call.SetPeerSendVideo(crdata->SendVideo == 1);
								call.SetPeerAudioFormat(fmt);

								success = true;
							});
						}
						break;
					}
					case MessageType::CallHangup:
					{
						Dbg(L"Received CallHangup message from %llu", event.GetPeerLUID());

						handled = true;

						IfGetCall(event.GetPeerLUID(), [&](auto& call)
						{
							if (call.StopCall())
							{
								success = true;
							}
						});

						if (!success)
						{
							LogErr(L"Couldn't hangup call from peer %llu", event.GetPeerLUID());
						}
						break;
					}
					case MessageType::GeneralFailure:
					{
						Dbg(L"Received GeneralFailure message from %llu", event.GetPeerLUID());

						handled = true;

						IfGetCall(event.GetPeerLUID(), [&](auto& call)
						{
							if (call.ProcessCallFailure())
							{
								success = true;
							}
						});

						if (!success)
						{
							LogErr(L"Couldn't process call failure from peer %llu", event.GetPeerLUID());
						}
						break;
					}
					case MessageType::AudioSample:
					{
						handled = true;

						UInt64 timestamp{ 0 };
						Buffer buffer;

						if (rdr.Read(timestamp, WithSize(buffer, GetMaximumMessageDataSize())))
						{
							IfGetCall(event.GetPeerLUID(), [&](auto& call)
							{
								call.OnPeerAudioSample(timestamp, buffer);
								success = true;
							});
						}

						break;
					}
					default:
					{
						LogInfo(L"Received unknown msgtype from %llu: %u", event.GetPeerLUID(), type);
						break;
					}
				}
			}
		}

		return std::make_pair(handled, success);
	}

	void Extender::WorkerThreadLoop(Extender* extender)
	{
		LogDbg(L"%s worker thread %u starting", extender->GetName().c_str(), std::this_thread::get_id());

		Util::SetCurrentThreadName(extender->GetName() + L" User Thread");

		// If the shutdown event is set quit the loop
		while (!extender->m_ShutdownEvent.IsSet())
		{
			extender->m_Peers.IfSharedLock([&](auto& peers)
			{
				for (auto it = peers.begin(); it != peers.end() && !extender->m_ShutdownEvent.IsSet(); ++it)
				{
					auto call = it->second->Call.WithUniqueLock();

					// If we've been waiting too long for a call to be
					// accepted cancel it
					if (call->IsCalling())
					{
						if (call->IsWaitExpired())
						{
							LogErr(L"Cancelling expired call %s peer %llu",
								(call->GetType() == CallType::Incoming) ? L"from" : L"to", it->second->ID);

							DiscardReturnValue(call->CancelCall());
						}
					}
					else if (call->IsInCall())
					{
						if (call->GetPeerSendVideo())
						{
							if (!call->HasVideoWindow())
							{
								call->OpenVideoWindow();
							}

							call->UpdateVideoWindow();
						}
						else
						{
							if (call->HasVideoWindow())
							{
								call->CloseVideoWindow();
							}
						}

						if (call->GetPeerSendAudio())
						{
							if (!call->HasAudioRenderer())
							{
								call->OpenAudioRenderer();
							}
						}
						else
						{
							if (call->HasAudioRenderer())
							{
								call->CloseAudioRenderer();
							}
						}
					}
					else if (call->IsDisconnected())
					{
						if (call->HasVideoWindow())
						{
							call->CloseVideoWindow();
						}

						if (call->HasAudioRenderer())
						{
							call->CloseAudioRenderer();
						}
					}
				}
			});

			// Sleep for a while or until we have to shut down
			extender->m_ShutdownEvent.Wait(1ms);
		}

		LogDbg(L"%s worker thread %u exiting", extender->GetName().c_str(), std::this_thread::get_id());
	}

	bool Extender::BeginCall(const PeerLUID pluid, const bool send_video, const bool send_audio) noexcept
	{
		auto success = false;

		IfGetCall(pluid, [&](auto& call)
		{
			if (send_audio)
			{
				StartAudioSourceReader();
			}

			call.SetSendVideo(send_video);
			call.SetSendAudio(send_audio);

			if (call.BeginCall())
			{
				// Cancel call if we leave scope without success
				auto sg = MakeScopeGuard([&]() noexcept { DiscardReturnValue(call.CancelCall()); });

				if (SendCallRequest(pluid, call))
				{
					SLogInfo(SLogFmt(FGBrightCyan) << L"Calling peer " << pluid << SLogFmt(Default));

					sg.Deactivate();
					success = true;
				}
			}
		});

		return success;
	}

	bool Extender::AcceptCall(const PeerLUID pluid) noexcept
	{
		auto success = false;

		IfGetCall(pluid, [&](auto& call)
		{
			if (call.GetSendAudio())
			{
				StartAudioSourceReader();
			}

			if (call.AcceptCall())
			{
				// Cancel call if we leave scope without success
				auto sg = MakeScopeGuard([&]() noexcept { DiscardReturnValue(call.CancelCall()); });

				if (SendCallAccept(pluid, call))
				{
					SLogInfo(SLogFmt(FGBrightCyan) << L"Accepted call from peer " << pluid << SLogFmt(Default));

					sg.Deactivate();
					success = true;
				}
			}
		});

		return success;
	}

	bool Extender::DeclineCall(const PeerLUID pluid) noexcept
	{
		auto success = false;

		IfGetCall(pluid, [&](auto& call)
		{
			if (call.CancelCall())
			{
				SLogInfo(SLogFmt(FGBrightCyan) << L"Declined call from peer " << pluid << SLogFmt(Default));

				success = true;
			}
		});

		return success;
	}

	bool Extender::HangupCall(const PeerLUID pluid) noexcept
	{
		auto success = false;

		IfGetCall(pluid, [&](auto& call)
		{
			if (call.IsInCall())
			{
				if (call.StopCall())
				{
					if (SendCallHangup(pluid))
					{
						SLogInfo(SLogFmt(FGBrightCyan) << L"Hung up call to peer " << pluid << SLogFmt(Default));

						success = true;
					}
				}
			}
			else if (call.IsCalling())
			{
				if (call.CancelCall())
				{
					SLogInfo(SLogFmt(FGBrightCyan) << L"Cancelled call to peer " << pluid << SLogFmt(Default));

					success = true;
				}
			}
		});

		return success;
	}

	template<typename Func>
	void Extender::IfGetCall(const PeerLUID pluid, Func&& func) noexcept(noexcept(func(std::declval<Call&>())))
	{
		m_Peers.WithSharedLock([&](auto& peers)
		{
			const auto it = peers.find(pluid);
			if (it != peers.end())
			{
				auto call = it->second->Call.WithUniqueLock();
				func(*call);
			}
			else LogErr(L"Peer not found");
		});
	}

	bool Extender::SendSimpleMessage(const PeerLUID pluid, const MessageType type, const BufferView& data)
	{
		const UInt16 msgtype = static_cast<const UInt16>(type);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, data))
		{
			return SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded();
		}
		else LogErr(L"Failed to prepare message for peer %llu", pluid);

		return false;
	}

	bool Extender::SendCallAVSample(const PeerLUID pluid, const MessageType type, const UInt64 timestamp, const BufferView& data)
	{
		const UInt16 msgtype = static_cast<const UInt16>(type);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, timestamp, WithSize(data, GetMaximumMessageDataSize())))
		{
			return SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded();
		}
		else LogErr(L"Failed to prepare message for peer %llu", pluid);

		return false;
	}

	bool Extender::SendCallRequest(const PeerLUID pluid, const Call& call)
	{
		CallRequestData data;
		if (call.GetSendAudio()) data.SendAudio = 1;
		if (call.GetSendVideo()) data.SendVideo = 1;

		m_AudioSource.WithUniqueLock([&](auto& as)
		{
			const auto fmt = as.AudioSourceReader.GetSampleFormat();
			data.AudioFormat.NumChannels = fmt.NumChannels;
			data.AudioFormat.SamplesPerSecond = fmt.SamplesPerSecond;
			data.AudioFormat.AvgBytesPerSecond = fmt.AvgBytesPerSecond;
			data.AudioFormat.BlockAlignment = fmt.BlockAlignment;
			data.AudioFormat.BitsPerSample = fmt.BitsPerSample;
		});

		if (SendSimpleMessage(pluid, MessageType::CallRequest,
							  BufferView(reinterpret_cast<Byte*>(&data), sizeof(data))))
		{
			return true;
		}
		else LogErr(L"Could not send CallRequest message to peer");

		return false;
	}

	bool Extender::SendCallAccept(const PeerLUID pluid, const Call& call)
	{
		CallRequestData data;
		if (call.GetSendAudio()) data.SendAudio = 1;
		if (call.GetSendVideo()) data.SendVideo = 1;

		m_AudioSource.WithUniqueLock([&](auto& as)
		{
			const auto fmt = as.AudioSourceReader.GetSampleFormat();
			data.AudioFormat.NumChannels = fmt.NumChannels;
			data.AudioFormat.SamplesPerSecond = fmt.SamplesPerSecond;
			data.AudioFormat.AvgBytesPerSecond = fmt.AvgBytesPerSecond;
			data.AudioFormat.BlockAlignment = fmt.BlockAlignment;
			data.AudioFormat.BitsPerSample = fmt.BitsPerSample;
		});

		if (SendSimpleMessage(pluid, MessageType::CallAccept,
							  BufferView(reinterpret_cast<Byte*>(&data), sizeof(data))))
		{
			return true;
		}
		else LogErr(L"Could not send CallAccept message to peer");

		return false;
	}

	bool Extender::SendCallAVUpdate(const PeerLUID pluid, const Call& call)
	{
		CallRequestData data;
		if (call.GetSendAudio()) data.SendAudio = 1;
		if (call.GetSendVideo()) data.SendVideo = 1;

		m_AudioSource.WithUniqueLock([&](auto& as)
		{
			const auto fmt = as.AudioSourceReader.GetSampleFormat();
			data.AudioFormat.NumChannels = fmt.NumChannels;
			data.AudioFormat.SamplesPerSecond = fmt.SamplesPerSecond;
			data.AudioFormat.AvgBytesPerSecond = fmt.AvgBytesPerSecond;
			data.AudioFormat.BlockAlignment = fmt.BlockAlignment;
			data.AudioFormat.BitsPerSample = fmt.BitsPerSample;
		});

		if (SendSimpleMessage(pluid, MessageType::CallAVUpdate,
							  BufferView(reinterpret_cast<Byte*>(&data), sizeof(data))))
		{
			return true;
		}
		else LogErr(L"Could not send CallAVUpdate message to peer");

		return false;
	}

	bool Extender::SendCallHangup(const PeerLUID pluid)
	{
		if (SendSimpleMessage(pluid, MessageType::CallHangup))
		{
			return true;
		}
		else LogErr(L"Could not send CallHangup message to peer");

		return false;
	}

	bool Extender::SendGeneralFailure(const PeerLUID pluid)
	{
		if (SendSimpleMessage(pluid, MessageType::GeneralFailure))
		{
			return true;
		}
		else LogErr(L"Could not send GeneralFailure message to peer");

		return false;
	}

	void Extender::StartAudioSourceReader() noexcept
	{
		auto audio_source = m_AudioSource.WithUniqueLock();
		if (audio_source->AudioSourceReader.IsOpen()) return;

		if (!audio_source->AudioEndpointID.empty())
		{
			const auto result = audio_source->AudioSourceReader.Open(audio_source->AudioEndpointID.c_str(),
																	 QuantumGate::MakeCallback(this, &Extender::OnAudioSample));
			if (result.Failed())
			{
				LogErr(L"Failed to start audio source reader; peers will not receive audio (%s)",
					   result.GetErrorString().c_str());
			}
		}
		else
		{
			LogErr(L"No audio device endpoint ID set; peers will not receive audio");
		}
	}

	void Extender::StopAudioSourceReader() noexcept
	{
		auto audio_source = m_AudioSource.WithUniqueLock();
		if (!audio_source->AudioSourceReader.IsOpen()) return;

		audio_source->AudioSourceReader.Close();
	}

	void Extender::OnAudioSample(const UInt64 timestamp, IMFSample* sample)
	{
		m_Peers.WithSharedLock([&](auto& peers)
		{
			for (auto it = peers.begin(); it != peers.end(); ++it)
			{
				const Peer& peer = *it->second;

				auto call = peer.Call.WithSharedLock();
				if (call->IsInCall() && call->GetSendAudio())
				{
					IMFMediaBuffer* media_buffer{ nullptr };

					// Get the buffer from the sample
					auto hr = sample->GetBufferByIndex(0, &media_buffer);
					if (SUCCEEDED(hr))
					{
						// Release buffer when we exit this scope
						const auto sg = MakeScopeGuard([&]() noexcept { SafeRelease(&media_buffer); });

						BYTE* data{ nullptr };
						DWORD data_len{ 0 };

						hr = media_buffer->Lock(&data, nullptr, &data_len);
						if (SUCCEEDED(hr))
						{
							DiscardReturnValue(SendCallAVSample(peer.ID, MessageType::AudioSample, timestamp,
																BufferView(reinterpret_cast<Byte*>(data), data_len)));

							media_buffer->Unlock();
						}
					}
				}
			}
		});
	}

	void Extender::UpdateSendAudioVideo(const PeerLUID pluid, const bool send_video, const bool send_audio)
	{
		m_Peers.WithSharedLock([&](auto& peers)
		{
			const auto peer = peers.find(pluid);
			if (peer != peers.end())
			{
				peer->second->Call.WithUniqueLock([&](auto& call)
				{
					call.SetSendVideo(send_video);
					call.SetSendAudio(send_audio);

					if (call.IsInCall())
					{
						if (send_audio)
						{
							StartAudioSourceReader();
						}

						DiscardReturnValue(SendCallAVUpdate(pluid, call));
					}
				});
			}
		});
	}
}
