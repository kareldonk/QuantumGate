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

		m_Peers.WithSharedLock([&](auto& peers)
		{
			for (auto it = peers.begin(); it != peers.end(); ++it)
			{
				const Peer& peer = *it->second;

				auto call = peer.Call->WithUniqueLock();
				if (!call->IsDisconnected())
				{
					DiscardReturnValue(call->StopCall());
				}
			}
		});

		StopAudioSourceReader();
		StopVideoSourceReader();
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
			peer->Call = std::make_shared<Call_ThS>(*this, m_AVSource, event.GetPeerLUID());

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
			UInt16 mtype{ 0 };
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

						Buffer buffer(sizeof(CallAVFormatData));
						if (rdr.Read(buffer))
						{
							IfGetCall(event.GetPeerLUID(), [&](auto& call)
							{
								if (call.IsDisconnected())
								{
									SLogInfo(SLogFmt(FGBrightCyan) << L"Incoming call from peer " << event.GetPeerLUID() << SLogFmt(Default));

									const CallAVFormatData* fmtdata = reinterpret_cast<const CallAVFormatData*>(buffer.GetBytes());

									if (call.SetPeerAVFormat(fmtdata))
									{
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
									}

									if (!success)
									{
										DiscardReturnValue(SendGeneralFailure(event.GetPeerLUID()));
									}
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

						Buffer buffer(sizeof(CallAVFormatData));
						if (rdr.Read(buffer))
						{
							auto call_ths = GetCall(event.GetPeerLUID());
							if (call_ths != nullptr)
							{
								call_ths->WithUniqueLock([&](auto& call)
								{
									if (call.IsCalling())
									{
										SLogInfo(SLogFmt(FGBrightCyan) << L"Peer " << event.GetPeerLUID() << L" accepted call" << SLogFmt(Default));

										const CallAVFormatData* fmtdata = reinterpret_cast<const CallAVFormatData*>(buffer.GetBytes());

										if (call.SetPeerAVFormat(fmtdata))
										{
											if (call.AcceptCall())
											{
												success = true;
											}
										}

										if (!success)
										{
											DiscardReturnValue(SendGeneralFailure(event.GetPeerLUID()));
										}
									}
								});
							}
						}

						if (!success)
						{
							LogErr(L"Couldn't accept outgoing call from peer %llu", event.GetPeerLUID());
						}
						break;
					}
					case MessageType::CallHangup:
					{
						Dbg(L"Received CallHangup message from %llu", event.GetPeerLUID());

						handled = true;

						IfGetCall(event.GetPeerLUID(), [&](auto& call)
						{
							SLogInfo(SLogFmt(FGBrightCyan) << L"Peer " << event.GetPeerLUID() << L" hung up" << SLogFmt(Default));

							if (call.IsInCall())
							{
								if (call.StopCall())
								{
									success = true;
								}
							}
						});

						if (!success)
						{
							LogErr(L"Couldn't hangup call from peer %llu", event.GetPeerLUID());
						}
						break;
					}
					case MessageType::CallDecline:
					{
						Dbg(L"Received CallDecline message from %llu", event.GetPeerLUID());

						handled = true;

						IfGetCall(event.GetPeerLUID(), [&](auto& call)
						{
							SLogInfo(SLogFmt(FGBrightCyan) << L"Peer " << event.GetPeerLUID() << L" declined call" << SLogFmt(Default));

							if (call.IsCalling())
							{
								if (call.StopCall())
								{
									success = true;
								}
							}
						});

						if (!success)
						{
							LogErr(L"Couldn't process call decline from peer %llu", event.GetPeerLUID());
						}
						break;
					}
					case MessageType::GeneralFailure:
					{
						Dbg(L"Received GeneralFailure message from %llu", event.GetPeerLUID());

						handled = true;

						IfGetCall(event.GetPeerLUID(), [&](auto& call)
						{
							SLogInfo(SLogFmt(FGBrightCyan) << L"Call with Peer " << event.GetPeerLUID() << SLogFmt(FGBrightRed)
									 << L" failed" << SLogFmt(Default));

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
					case MessageType::CallAVUpdate:
					{
						Dbg(L"Received CallAVUpdate message from %llu", event.GetPeerLUID());

						handled = true;

						Buffer buffer(sizeof(CallAVFormatData));
						if (rdr.Read(buffer))
						{
							IfGetCall(event.GetPeerLUID(), [&](auto& call)
							{
								const CallAVFormatData* fmtdata = reinterpret_cast<const CallAVFormatData*>(buffer.GetBytes());

								success = call.SetPeerAVFormat(fmtdata);
							});
						}
						break;
					}
					case MessageType::AudioSample:
					{
						handled = true;

						MediaSample_ThS* sample_ths{ nullptr };

						IfGetCall(event.GetPeerLUID(), [&](auto& call)
						{
							sample_ths = &call.GetAudioOutSample();
						});

						if (sample_ths != nullptr)
						{
							sample_ths->WithUniqueLock([&](auto& s)
							{
								UInt64 timestamp{ 0 };

								if (rdr.Read(timestamp, WithSize(s.SampleBuffer, GetMaximumMessageDataSize())))
								{
									s.New = true;
									s.TimeStamp = timestamp;

									success = true;
								}
							});
						}
						break;
					}
					case MessageType::VideoSample:
					{
						handled = true;

						MediaSample_ThS* sample_ths{ nullptr };

						IfGetCall(event.GetPeerLUID(), [&](auto& call)
						{
							sample_ths = &call.GetVideoOutSample();
						});

						if (sample_ths != nullptr)
						{
							sample_ths->WithUniqueLock([&](auto& s)
							{
								UInt64 timestamp{ 0 };

								if (rdr.Read(timestamp, WithSize(s.SampleBuffer, GetMaximumMessageDataSize())))
								{
									s.New = true;
									s.TimeStamp = timestamp;

									success = true;
								}
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
					auto call = it->second->Call->WithUniqueLock();

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

		auto call_ths = GetCall(pluid);
		if (call_ths != nullptr)
		{
			call_ths->WithUniqueLock([&](auto& call)
			{
				call.SetSendVideo(send_video);
				call.SetSendAudio(send_audio);

				success = call.BeginCall();
			});

			if (success)
			{
				if (send_audio) StartAudioSourceReader();
				if (send_video) StartVideoSourceReader();

				// Cancel call if we leave scope without success
				auto sg = MakeScopeGuard([&]() noexcept
				{
					DiscardReturnValue(call_ths->WithUniqueLock()->CancelCall());
				});

				if (SendCallRequest(pluid, send_audio, send_video))
				{
					SLogInfo(SLogFmt(FGBrightCyan) << L"Calling peer " << pluid << SLogFmt(Default));

					sg.Deactivate();
				}
				else success = false;
			}
		}

		return success;
	}

	bool Extender::AcceptCall(const PeerLUID pluid) noexcept
	{
		auto success = false;

		auto call_ths = GetCall(pluid);
		if (call_ths != nullptr)
		{
			auto send_audio = false;
			auto send_video = false;

			call_ths->WithUniqueLock([&](auto& call)
			{
				// Should be in a call
				if (call.IsCalling())
				{
					send_audio = call.GetSendAudio();
					send_video = call.GetSendVideo();

					success = call.AcceptCall();
				}
			});

			if (success)
			{
				if (send_audio) StartAudioSourceReader();
				if (send_video) StartVideoSourceReader();

				// Cancel call if we leave scope without success
				auto sg = MakeScopeGuard([&]() noexcept
				{
					DiscardReturnValue(call_ths->WithUniqueLock()->CancelCall());
				});

				if (SendCallAccept(pluid, send_audio, send_video))
				{
					SLogInfo(SLogFmt(FGBrightCyan) << L"Accepted call from peer " << pluid << SLogFmt(Default));

					sg.Deactivate();
				}
				else success = false;
			}

			if (!success)
			{
				// Try to let the peer know we couldn't accept the call
				DiscardReturnValue(SendGeneralFailure(pluid));
			}
		}

		return success;
	}

	bool Extender::DeclineCall(const PeerLUID pluid) noexcept
	{
		auto success = false;

		auto call_ths = GetCall(pluid);
		if (call_ths != nullptr)
		{
			call_ths->WithUniqueLock([&](auto& call)
			{
				// Should be in a call
				if (call.IsCalling())
				{
					success = call.CancelCall();
				}
			});

			if (success)
			{
				if (SendCallDecline(pluid))
				{
					SLogInfo(SLogFmt(FGBrightCyan) << L"Declined call from peer " << pluid << SLogFmt(Default));
				}
				else success = false;
			}
		}

		return success;
	}

	bool Extender::HangupCall(const PeerLUID pluid) noexcept
	{
		auto call_ths = GetCall(pluid);
		if (call_ths != nullptr)
		{
			return HangupCall(call_ths);
		}

		return false;
	}

	bool Extender::HangupCall(std::shared_ptr<Call_ThS>& call_ths) noexcept
	{
		auto success = false;
		auto ishangup = true;
		PeerLUID pluid{ 0 };

		call_ths->WithUniqueLock([&](auto& call)
		{
			pluid = call.GetPeerLUID();

			if (call.IsInCall())
			{
				success = call.StopCall();
			}
			else if (call.IsCalling())
			{
				ishangup = false;
				success = call.CancelCall();
			}
		});

		if (success)
		{
			if (ishangup)
			{
				if (SendCallHangup(pluid))
				{
					SLogInfo(SLogFmt(FGBrightCyan) << L"Hung up call to peer " << pluid << SLogFmt(Default));
				}
				else success = false;
			}
			else
			{
				SLogInfo(SLogFmt(FGBrightCyan) << L"Cancelled call to peer " << pluid << SLogFmt(Default));
			}
		}

		return success;
	}

	void Extender::HangupAllCalls() noexcept
	{
		m_Peers.WithSharedLock([&](auto& peers)
		{
			for (auto it = peers.begin(); it != peers.end(); ++it)
			{
				DiscardReturnValue(HangupCall(it->second->Call));
			}
		});
	}

	template<typename Func>
	void Extender::IfGetCall(const PeerLUID pluid, Func&& func) noexcept(noexcept(func(std::declval<Call&>())))
	{
		m_Peers.WithSharedLock([&](auto& peers)
		{
			if (const auto it = peers.find(pluid); it != peers.end())
			{
				auto call = it->second->Call->WithUniqueLock();
				func(*call);
			}
			else LogErr(L"Peer not found");
		});
	}

	std::shared_ptr<Call_ThS> Extender::GetCall(const PeerLUID pluid) const noexcept
	{
		std::shared_ptr<Call_ThS> call;

		m_Peers.WithSharedLock([&](auto& peers)
		{
			if (const auto it = peers.find(pluid); it != peers.end())
			{
				call = it->second->Call;
			}
		});

		return call;
	}

	bool Extender::SendSimpleMessage(const PeerLUID pluid, const MessageType type, const BufferView data)
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

	bool Extender::SendCallAVSample(const PeerLUID pluid, const MessageType type, const UInt64 timestamp, const BufferView data)
	{
		const UInt16 msgtype = static_cast<const UInt16>(type);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, timestamp, WithSize(data, GetMaximumMessageDataSize())))
		{
			SendParameters params;

			switch (type)
			{
				case MessageType::AudioSample:
					params.Priority = SendParameters::PriorityOption::Expedited;
					break;
				case MessageType::VideoSample:
					// default
					break;
				default:
					assert(false);
					break;
			}

			return SendMessageTo(pluid, writer.MoveWrittenBytes(), params).Succeeded();
		}
		else LogErr(L"Failed to prepare message for peer %llu", pluid);

		return false;
	}

	CallAVFormatData Extender::GetCallAVFormatData(const bool send_audio, const bool send_video)
	{
		CallAVFormatData data;
		if (send_audio) data.SendAudio = 1;
		if (send_video) data.SendVideo = 1;

		m_AVSource.WithSharedLock([&](auto& avsource)
		{
			const auto afmt = avsource.AudioSourceReader.GetSampleFormat();
			data.AudioFormat.NumChannels = afmt.NumChannels;
			data.AudioFormat.SamplesPerSecond = afmt.SamplesPerSecond;
			data.AudioFormat.AvgBytesPerSecond = afmt.AvgBytesPerSecond;
			data.AudioFormat.BlockAlignment = afmt.BlockAlignment;
			data.AudioFormat.BitsPerSample = afmt.BitsPerSample;

			const auto vfmt = avsource.VideoSourceReader.GetSampleFormat();
			data.VideoFormat.Format = vfmt.Format;
			data.VideoFormat.BytesPerPixel = vfmt.BytesPerPixel;
			data.VideoFormat.Stride = vfmt.Stride;
			data.VideoFormat.Width = vfmt.Width;
			data.VideoFormat.Height = vfmt.Height;
		});

		return data;
	}

	bool Extender::SendCallRequest(const PeerLUID pluid, const bool send_audio, const bool send_video)
	{
		const CallAVFormatData data = GetCallAVFormatData(send_audio, send_video);

		if (SendSimpleMessage(pluid, MessageType::CallRequest,
							  BufferView(reinterpret_cast<const Byte*>(&data), sizeof(CallAVFormatData))))
		{
			return true;
		}
		else LogErr(L"Could not send CallRequest message to peer");

		return false;
	}

	bool Extender::SendCallAccept(const PeerLUID pluid, const bool send_audio, const bool send_video)
	{
		const CallAVFormatData data = GetCallAVFormatData(send_audio, send_video);

		if (SendSimpleMessage(pluid, MessageType::CallAccept,
							  BufferView(reinterpret_cast<const Byte*>(&data), sizeof(CallAVFormatData))))
		{
			return true;
		}
		else LogErr(L"Could not send CallAccept message to peer");

		return false;
	}

	bool Extender::SendCallAVUpdate(const PeerLUID pluid, const bool send_audio, const bool send_video)
	{
		const CallAVFormatData data = GetCallAVFormatData(send_audio, send_video);

		if (SendSimpleMessage(pluid, MessageType::CallAVUpdate,
							  BufferView(reinterpret_cast<const Byte*>(&data), sizeof(CallAVFormatData))))
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

	bool Extender::SendCallDecline(const PeerLUID pluid)
	{
		if (SendSimpleMessage(pluid, MessageType::CallDecline))
		{
			return true;
		}
		else LogErr(L"Could not send CallDecline message to peer");

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
		auto avsource = m_AVSource.WithUniqueLock();
		StartAudioSourceReader(*avsource);
	}

	void Extender::StartAudioSourceReader(AVSource& avsource) noexcept
	{
		if (avsource.AudioSourceReader.IsOpen()) return;

		if (!avsource.AudioEndpointID.empty())
		{
			const auto result = avsource.AudioSourceReader.Open(avsource.AudioEndpointID.c_str(),
																{ MFAudioFormat_Float }, nullptr);
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
		auto avsource = m_AVSource.WithUniqueLock();
		StopAudioSourceReader(*avsource);
	}

	void Extender::StopAudioSourceReader(AVSource& avsource) noexcept
	{
		if (!avsource.AudioSourceReader.IsOpen()) return;

		avsource.AudioSourceReader.Close();
	}

	void Extender::StartVideoSourceReader() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();
		StartVideoSourceReader(*avsource);
	}

	void Extender::StartVideoSourceReader(AVSource& avsource) noexcept
	{
		if (avsource.VideoSourceReader.IsOpen()) return;

		if (!avsource.VideoSymbolicLink.empty())
		{
			const auto result = avsource.VideoSourceReader.Open(avsource.VideoSymbolicLink.c_str(),
																{ MFVideoFormat_RGB24 }, nullptr);
			if (result.Failed())
			{
				LogErr(L"Failed to start video source reader; peers will not receive video (%s)",
					   result.GetErrorString().c_str());
			}
		}
		else
		{
			LogErr(L"No video device symbolic link set; peers will not receive video");
		}
	}

	void Extender::StopVideoSourceReader() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();
		StopVideoSourceReader(*avsource);
	}

	void Extender::StopVideoSourceReader(AVSource& avsource) noexcept
	{
		if (!avsource.VideoSourceReader.IsOpen()) return;

		avsource.VideoSourceReader.Close();
	}

	void Extender::UpdateSendAudioVideo(const PeerLUID pluid, const bool send_video, const bool send_audio)
	{
		std::shared_ptr<Call_ThS> call_ths;

		m_Peers.WithSharedLock([&](auto& peers)
		{
			const auto peer = peers.find(pluid);
			if (peer != peers.end())
			{
				call_ths = peer->second->Call;
			}
		});

		if (call_ths != nullptr)
		{
			if (send_audio) StartAudioSourceReader();

			if (send_video) StartVideoSourceReader();

			if (call_ths->WithSharedLock()->IsInCall())
			{
				DiscardReturnValue(SendCallAVUpdate(pluid, send_audio, send_video));
			}

			call_ths->WithUniqueLock([&](auto& call)
			{
				call.SetSendVideo(send_video);
				call.SetSendAudio(send_audio);
			});
		}
	}

	void Extender::SetAudioEndpointID(const WCHAR* id)
	{
		m_AVSource.WithUniqueLock([&](auto& avsource)
		{
			const bool was_open = avsource.AudioSourceReader.IsOpen();

			StopAudioSourceReader(avsource);

			avsource.AudioEndpointID = id;

			if (was_open) StartAudioSourceReader(avsource);
		});

		m_Peers.WithSharedLock([&](auto& peers)
		{
			for (auto it = peers.begin(); it != peers.end(); ++it)
			{
				const Peer& peer = *it->second;

				auto call = peer.Call->WithUniqueLock();
				if (call->IsInCall())
				{
					call->OnAVSourceChange();

					if (call->GetSendAudio())
					{
						DiscardReturnValue(SendCallAVUpdate(peer.ID, call->GetSendAudio(), call->GetSendVideo()));
					}
				}
			}
		});
	}

	void Extender::SetVideoSymbolicLink(const WCHAR* id)
	{
		m_AVSource.WithUniqueLock([&](auto& avsource)
		{
			const bool was_open = avsource.VideoSourceReader.IsOpen();

			StopVideoSourceReader(avsource);

			avsource.VideoSymbolicLink = id;

			if (was_open) StartVideoSourceReader(avsource);
		});

		m_Peers.WithSharedLock([&](auto& peers)
		{
			for (auto it = peers.begin(); it != peers.end(); ++it)
			{
				const Peer& peer = *it->second;

				auto call = peer.Call->WithUniqueLock();
				if (call->IsInCall())
				{
					call->OnAVSourceChange();

					if (call->GetSendVideo())
					{
						DiscardReturnValue(SendCallAVUpdate(peer.ID, call->GetSendAudio(), call->GetSendVideo()));
					}
				}
			}
		});
	}
}
