// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "AVExtender.h"
#include "AudioCompressor.h"

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

	void Extender::SetUseCompression(const bool compression) noexcept
	{
		m_Settings.UpdateValue([&](auto& settings)
		{
			settings.UseCompression = compression;
		});
	}

	void Extender::SetUseAudioCompression(const bool compression) noexcept
	{
		m_Settings.UpdateValue([&](auto& settings)
		{
			settings.UseAudioCompression = compression;
		});
	}

	void Extender::SetUseVideoCompression(const bool compression) noexcept
	{
		m_Settings.UpdateValue([&](auto& settings)
		{
			settings.UseVideoCompression = compression;
		});
	}

	void Extender::SetFillVideoScreen(const bool fill) noexcept
	{
		m_Settings.UpdateValue([&](auto& settings)
		{
			settings.FillVideoScreen = fill;
		});
	}

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

		StopAllCalls();
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
			peer->Call = std::make_shared<Call_ThS>(event.GetPeerLUID(), *this, m_Settings, m_AVSource);

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

						IfGetCall(event.GetPeerLUID(), [&](auto& call)
						{
							if (call.IsDisconnected())
							{
								SLogInfo(SLogFmt(FGBrightCyan) << L"Incoming call from peer " << event.GetPeerLUID() << SLogFmt(Default));

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

								if (!success)
								{
									DiscardReturnValue(SendGeneralFailure(event.GetPeerLUID()));
								}
							}
						});

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

						auto call_ths = GetCall(event.GetPeerLUID());
						if (call_ths != nullptr)
						{
							call_ths->WithUniqueLock([&](auto& call)
							{
								if (call.IsCalling())
								{
									SLogInfo(SLogFmt(FGBrightCyan) << L"Peer " << event.GetPeerLUID() << L" accepted call" << SLogFmt(Default));

									if (call.AcceptCall())
									{
										success = true;
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
					case MessageType::AudioSample:
					{
						handled = true;

						UInt64 timestamp{ 0 };
						Buffer fmt_buffer(sizeof(AudioFormatData));
						Buffer buffer;

						if (rdr.Read(timestamp, fmt_buffer, WithSize(buffer, GetMaximumMessageDataSize())))
						{
							IfGetCall(event.GetPeerLUID(), [&](auto& call)
							{
								const AudioFormatData* fmtdata = reinterpret_cast<const AudioFormatData*>(fmt_buffer.GetBytes());

								call.OnAudioInSample(*fmtdata, timestamp, std::move(buffer));
								success = true;
							});
						}
						break;
					}
					case MessageType::VideoSample:
					{
						handled = true;

						UInt64 timestamp{ 0 };
						Buffer fmt_buffer(sizeof(VideoFormatData));
						Buffer buffer;

						if (rdr.Read(timestamp, fmt_buffer, WithSize(buffer, GetMaximumMessageDataSize())))
						{
							IfGetCall(event.GetPeerLUID(), [&](auto& call)
							{
								const VideoFormatData* fmtdata = reinterpret_cast<const VideoFormatData*>(fmt_buffer.GetBytes());

								call.OnVideoInSample(*fmtdata, timestamp, std::move(buffer));
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
					bool cancel_call{ false };
					CallType call_type{ CallType::None };

					it->second->Call->WithSharedLock([&](auto& call)
					{
						call_type = call.GetType();

						// If we've been waiting too long for a call to be
						// accepted cancel it
						if (call.IsCalling())
						{
							if (call.IsWaitExpired())
							{
								cancel_call = true;
							}
						}
					});

					if (cancel_call)
					{
						LogErr(L"Cancelling expired call %s peer %llu",
							(call_type == CallType::Incoming) ? L"from" : L"to", it->second->ID);

						DiscardReturnValue(it->second->Call->WithUniqueLock()->CancelCall());
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

				if (SendCallRequest(pluid))
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

				if (SendCallAccept(pluid))
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

	void Extender::StopAllCalls() noexcept
	{
		m_Peers.WithSharedLock([&](auto& peers)
		{
			for (auto it = peers.begin(); it != peers.end(); ++it)
			{
				Peer& peer = *it->second;

				auto call = peer.Call->WithUniqueLock();
				if (!call->IsDisconnected())
				{
					DiscardReturnValue(call->StopCall());
				}
			}
		});
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

	bool Extender::HaveActiveCalls() const noexcept
	{
		bool active_calls{ false };

		m_Peers.WithSharedLock([&](auto& peers)
		{
			for (auto it = peers.begin(); it != peers.end(); ++it)
			{
				Peer& peer = *it->second;

				if (!peer.Call->WithSharedLock()->IsDisconnected())
				{
					active_calls = true;
					return;
				}
			}
		});

		return active_calls;
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

	bool Extender::SendSimpleMessage(const PeerLUID pluid, const MessageType type,
									 const SendParameters::PriorityOption priority, const BufferView data) const noexcept
	{
		try
		{
			const UInt16 msgtype = static_cast<const UInt16>(type);

			BufferWriter writer(true);
			if (writer.WriteWithPreallocation(msgtype, data))
			{
				SendParameters params;
				params.Compress = m_Settings->UseCompression;
				params.Priority = priority;

				return SendMessageTo(pluid, writer.MoveWrittenBytes(), params).Succeeded();
			}
			else LogErr(L"Failed to prepare message for peer %llu", pluid);
		}
		catch (...)
		{
			LogErr(L"Failed to send message ID %u to peer %llu due to exception", type, pluid);
		}

		return false;
	}

	bool Extender::SendCallAudioSample(const PeerLUID pluid, const AudioFormat& afmt, const UInt64 timestamp,
									   const BufferView data, const bool compressed) const noexcept
	{
		try
		{
			const UInt16 msgtype = static_cast<const UInt16>(MessageType::AudioSample);

			AudioFormatData fmt_data;
			fmt_data.NumChannels = afmt.NumChannels;
			fmt_data.SamplesPerSecond = afmt.SamplesPerSecond;
			fmt_data.BlockAlignment = afmt.BlockAlignment;
			fmt_data.BitsPerSample = afmt.BitsPerSample;
			fmt_data.AvgBytesPerSecond = afmt.AvgBytesPerSecond;
			fmt_data.Compressed = compressed;

			BufferWriter writer(true);
			if (writer.WriteWithPreallocation(msgtype, timestamp,
											  BufferView(reinterpret_cast<const Byte*>(&fmt_data), sizeof(AudioFormatData)),
											  WithSize(data, GetMaximumMessageDataSize())))
			{
				SendParameters params;
				params.Compress = m_Settings->UseCompression;
				params.Priority = SendParameters::PriorityOption::Expedited;

				return SendMessageTo(pluid, writer.MoveWrittenBytes(), params).Succeeded();
			}
			else LogErr(L"Failed to prepare audio sample message for peer %llu", pluid);
		}
		catch (...) {}

		return false;
	}

	bool Extender::SendCallVideoSample(const PeerLUID pluid, const VideoFormat& vfmt, const UInt64 timestamp,
									   const BufferView data, const bool compressed) const noexcept
	{
		try
		{
			const UInt16 msgtype = static_cast<const UInt16>(MessageType::VideoSample);

			VideoFormatData fmt_data;
			fmt_data.Format = vfmt.Format;
			fmt_data.Width = vfmt.Width;
			fmt_data.Height = vfmt.Height;
			fmt_data.BytesPerPixel = vfmt.BytesPerPixel;
			fmt_data.Compressed = compressed;

			BufferWriter writer(true);
			if (writer.WriteWithPreallocation(msgtype, timestamp,
											  BufferView(reinterpret_cast<const Byte*>(&fmt_data), sizeof(VideoFormatData)),
											  WithSize(data, GetMaximumMessageDataSize())))
			{
				SendParameters params;
				params.Compress = m_Settings->UseCompression;
				params.Priority = SendParameters::PriorityOption::Normal;

				return SendMessageTo(pluid, writer.MoveWrittenBytes(), params).Succeeded();
			}
			else LogErr(L"Failed to prepare video sample message for peer %llu", pluid);
		}
		catch (...) {}

		return false;
	}

	bool Extender::SendCallRequest(const PeerLUID pluid) const noexcept
	{
		if (SendSimpleMessage(pluid, MessageType::CallRequest, SendParameters::PriorityOption::Normal))
		{
			return true;
		}
		else LogErr(L"Could not send CallRequest message to peer");

		return false;
	}

	bool Extender::SendCallAccept(const PeerLUID pluid) const noexcept
	{
		if (SendSimpleMessage(pluid, MessageType::CallAccept, SendParameters::PriorityOption::Normal))
		{
			return true;
		}
		else LogErr(L"Could not send CallAccept message to peer");

		return false;
	}


	bool Extender::SendCallHangup(const PeerLUID pluid) const noexcept
	{
		if (SendSimpleMessage(pluid, MessageType::CallHangup, SendParameters::PriorityOption::Normal))
		{
			return true;
		}
		else LogErr(L"Could not send CallHangup message to peer");

		return false;
	}

	bool Extender::SendCallDecline(const PeerLUID pluid) const noexcept
	{
		if (SendSimpleMessage(pluid, MessageType::CallDecline, SendParameters::PriorityOption::Normal))
		{
			return true;
		}
		else LogErr(L"Could not send CallDecline message to peer");

		return false;
	}

	bool Extender::SendGeneralFailure(const PeerLUID pluid) const noexcept
	{
		if (SendSimpleMessage(pluid, MessageType::GeneralFailure, SendParameters::PriorityOption::Normal))
		{
			return true;
		}
		else LogErr(L"Could not send GeneralFailure message to peer");

		return false;
	}

	bool Extender::StartAudioSourceReader() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();
		return StartAudioSourceReader(*avsource);
	}

	bool Extender::StartAudioSourceReader(AVSource& avsource) noexcept
	{
		if (avsource.AudioSourceReader.IsOpen()) return true;

		LogDbg(L"Starting audio source reader...");

		if (!avsource.AudioEndpointID.empty())
		{
			const auto result = avsource.AudioSourceReader.Open(avsource.AudioEndpointID.c_str(),
																{ MFAudioFormat_PCM, MFAudioFormat_Float }, nullptr);
			if (result.Succeeded())
			{
				if (avsource.AudioSourceReader.SetSampleFormat(AudioCompressor::GetEncoderInputFormat()))
				{
					return avsource.AudioSourceReader.BeginRead();
				}
				else
				{
					LogErr(L"Failed to set sample format on audio device; peers will not receive audio");
				}
			}
			else
			{
				LogErr(L"Failed to start audio source reader; peers will not receive audio (%s)",
					   result.GetErrorString().c_str());
			}
		}
		else
		{
			LogErr(L"No audio device endpoint ID set; peers will not receive audio");
		}

		return false;
	}

	void Extender::StopAudioSourceReader() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();
		StopAudioSourceReader(*avsource);
	}

	void Extender::StopAudioSourceReader(AVSource& avsource) noexcept
	{
		if (!avsource.AudioSourceReader.IsOpen()) return;

		LogDbg(L"Stopping audio source reader...");

		avsource.AudioSourceReader.Close();
	}

	bool Extender::StartVideoSourceReader() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();
		return StartVideoSourceReader(*avsource);
	}

	bool Extender::StartVideoSourceReader(AVSource& avsource) noexcept
	{
		if (avsource.VideoSourceReader.IsOpen()) return true;

		LogDbg(L"Starting video source reader...");

		if (!avsource.VideoSymbolicLink.empty())
		{
			auto width = static_cast<UInt16>((static_cast<double>(avsource.MaxVideoResolution) / 3.0) * 4.0);
			width = width - (width % 16);

			avsource.VideoSourceReader.SetPreferredSize(width, avsource.MaxVideoResolution);

			const auto result = avsource.VideoSourceReader.Open(avsource.VideoSymbolicLink.c_str(),
																{ MFVideoFormat_NV12, MFVideoFormat_I420, MFVideoFormat_RGB24 }, nullptr);
			if (result.Succeeded())
			{
				const auto fmt = avsource.VideoSourceReader.GetSampleFormat();

				// Make dimensions multiples of 16 for H.256
				// compression without artifacts
				{
					if (fmt.Height % 16 != 0 || fmt.Width % 16 != 0)
					{
						auto swidth = fmt.Width - (fmt.Width % 16);
						auto sheight = fmt.Height - (fmt.Height % 16);

						if (!avsource.VideoSourceReader.SetSampleSize(swidth, sheight))
						{
							LogErr(L"Failed to set sample size on video device");
						}
					}
				}

				return avsource.VideoSourceReader.BeginRead();
			}
			else
			{
				LogErr(L"Failed to start video source reader; peers will not receive video (%s)",
					   result.GetErrorString().c_str());
			}
		}
		else
		{
			LogErr(L"No video device symbolic link set; peers will not receive video");
		}

		return false;
	}

	void Extender::StopVideoSourceReader() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();
		StopVideoSourceReader(*avsource);
	}

	void Extender::StopVideoSourceReader(AVSource& avsource) noexcept
	{
		if (!avsource.VideoSourceReader.IsOpen()) return;

		LogDbg(L"Stopping video source reader...");

		avsource.VideoSourceReader.Close();
	}

	void Extender::StopAVSourceReaders() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();
		StopAudioSourceReader(*avsource);
		StopVideoSourceReader(*avsource);
	}

	void Extender::UpdateSendAudio(const PeerLUID pluid, const bool send_audio) noexcept
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

			call_ths->WithUniqueLock([&](auto& call)
			{
				call.SetSendAudio(send_audio);
			});
		}
	}

	void Extender::UpdateSendVideo(const PeerLUID pluid, const bool send_video) noexcept
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
			if (send_video) StartVideoSourceReader();

			call_ths->WithUniqueLock([&](auto& call)
			{
				call.SetSendVideo(send_video);
			});
		}
	}

	bool Extender::SetAudioEndpointID(const WCHAR* id)
	{
		auto success = true;

		m_AVSource.WithUniqueLock([&](auto& avsource)
		{
			const bool was_open = avsource.AudioSourceReader.IsOpen();

			StopAudioSourceReader(avsource);

			avsource.AudioEndpointID = id;

			if (was_open)
			{
				success = StartAudioSourceReader(avsource);
			}
		});

		m_Peers.WithSharedLock([&](auto& peers)
		{
			for (auto it = peers.begin(); it != peers.end(); ++it)
			{
				const Peer& peer = *it->second;

				auto call = peer.Call->WithUniqueLock();
				if (call->IsInCall())
				{
					call->OnAudioSourceChange();
				}
			}
		});

		return success;
	}

	bool Extender::SetVideoSymbolicLink(const WCHAR* id, const Size max_res)
	{
		auto success = true;

		m_AVSource.WithUniqueLock([&](auto& avsource)
		{
			const bool was_open = avsource.VideoSourceReader.IsOpen();

			StopVideoSourceReader(avsource);

			avsource.VideoSymbolicLink = id;
			avsource.MaxVideoResolution = max_res;

			if (was_open)
			{
				success = StartVideoSourceReader(avsource);
			}
		});

		m_Peers.WithSharedLock([&](auto& peers)
		{
			for (auto it = peers.begin(); it != peers.end(); ++it)
			{
				const Peer& peer = *it->second;

				auto call = peer.Call->WithUniqueLock();
				if (call->IsInCall())
				{
					call->OnVideoSourceChange();
				}
			}
		});

		return success;
	}

	Result<VideoFormat> Extender::StartVideoPreview(SourceReader::SampleEventDispatcher::FunctionType&& callback) noexcept
	{
		auto success = false;
		VideoFormat video_format;

		m_AVSource.WithUniqueLock([&](auto& avsource)
		{
			success = avsource.VideoSourceReader.IsOpen();
			if (!success)
			{
				success = StartVideoSourceReader(avsource);
			}

			if (success)
			{
				m_PreviewVideoSampleEventFunctionHandle = avsource.VideoSourceReader.AddSampleEventCallback(std::move(callback));
				video_format = avsource.VideoSourceReader.GetSampleFormat();
				avsource.Previewing = true;
			}
		});

		if (success)
		{
			return video_format;
		}

		return AVResultCode::Failed;
	}

	void Extender::StopVideoPreview() noexcept
	{
		if (m_PreviewVideoSampleEventFunctionHandle)
		{
			bool previewing{ true };

			m_AVSource.WithUniqueLock([&](auto& avsource)
			{
				avsource.VideoSourceReader.RemoveSampleEventCallback(m_PreviewVideoSampleEventFunctionHandle);

				if (!m_PreviewAudioSampleEventFunctionHandle)
				{
					avsource.Previewing = false;
					previewing = false;
				}
			});

			if (!previewing && !HaveActiveCalls())
			{
				StopAVSourceReaders();
			}
		}
	}

	Result<AudioFormat> Extender::StartAudioPreview(SourceReader::SampleEventDispatcher::FunctionType&& callback) noexcept
	{
		auto success = false;
		AudioFormat audio_format;

		m_AVSource.WithUniqueLock([&](auto& avsource)
		{
			success = avsource.AudioSourceReader.IsOpen();
			if (!success)
			{
				success = StartAudioSourceReader(avsource);
			}

			if (success)
			{
				m_PreviewAudioSampleEventFunctionHandle = avsource.AudioSourceReader.AddSampleEventCallback(std::move(callback));
				audio_format = avsource.AudioSourceReader.GetSampleFormat();
				avsource.Previewing = true;
			}
		});

		if (success)
		{
			return audio_format;
		}

		return AVResultCode::Failed;
	}

	void Extender::StopAudioPreview() noexcept
	{
		if (m_PreviewAudioSampleEventFunctionHandle)
		{
			bool previewing{ true };

			m_AVSource.WithUniqueLock([&](auto& avsource)
			{
				avsource.AudioSourceReader.RemoveSampleEventCallback(m_PreviewAudioSampleEventFunctionHandle);

				if (!m_PreviewVideoSampleEventFunctionHandle)
				{
					avsource.Previewing = false;
					previewing = false;
				}
			});

			if (!previewing && !HaveActiveCalls())
			{
				StopAVSourceReaders();
			}
		}
	}
}
