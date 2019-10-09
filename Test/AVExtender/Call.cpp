// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Call.h"
#include "AVExtender.h"

#include <Common\Util.h>

using namespace QuantumGate::Implementation;
using namespace std::literals;

namespace QuantumGate::AVExtender
{
	Call::~Call()
	{
		if (IsInCall())
		{
			DiscardReturnValue(StopCall());
		}
	}

	bool Call::SetStatus(const CallStatus status) noexcept
	{
		auto success = true;
		const auto prev_status = m_Status;

		switch (status)
		{
			case CallStatus::WaitingForAccept:
				assert(prev_status == CallStatus::Disconnected);
				if (prev_status == CallStatus::Disconnected) m_Status = status;
				else success = false;
				break;
			case CallStatus::NeedAccept:
				assert(prev_status == CallStatus::Disconnected);
				if (prev_status == CallStatus::Disconnected) m_Status = status;
				else success = false;
				break;
			case CallStatus::Connected:
				assert(prev_status == CallStatus::WaitingForAccept || prev_status == CallStatus::NeedAccept);
				if (prev_status == CallStatus::WaitingForAccept || prev_status == CallStatus::NeedAccept)
				{
					m_Status = status;
					m_StartSteadyTime = Util::GetCurrentSteadyTime();

					OnConnected();
				}
				else success = false;
				break;
			case CallStatus::Disconnected:
				assert(prev_status == CallStatus::WaitingForAccept || prev_status == CallStatus::NeedAccept ||
					   prev_status == CallStatus::Connected);
				if (prev_status == CallStatus::WaitingForAccept || prev_status == CallStatus::NeedAccept ||
					prev_status == CallStatus::Connected)
				{
					m_Status = status;

					OnDisconnected();
				}
				else success = false;
				break;
			default:
				assert(false);
				success = false;
				break;
		}

		if (success)
		{
			m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();
		}

		return success;
	}

	void Call::WorkerThreadLoop(Call* call)
	{
		LogDbg(L"Call worker thread %u starting", std::this_thread::get_id());

		Util::SetCurrentThreadName(L"AVExtender Call Thread");

		call->OpenVideoWindow();
		call->OpenAudioRenderer();

		// If the shutdown event is set quit the loop
		while (!call->m_ShutdownEvent.IsSet())
		{
			const auto settings = call->m_Settings.load();

			if (settings & static_cast<UInt8>(CallSetting::SendAudio))
			{
				// Try to send at most 1 second of audio data at once
				const auto max_send = call->m_AVSource.WithSharedLock()->AudioSourceReader.GetSampleFormat().AvgBytesPerSecond;

				// Audio frames total size should not be larger than what we can send
				assert(max_send <= call->m_Extender.GetMaximumMessageDataSize());

				call->m_AudioInSample.WithUniqueLock([&](auto& media_sample)
				{
					if (media_sample.New)
					{
						BufferView buf = media_sample.SampleBuffer;

						while (!buf.IsEmpty())
						{
							auto buf2 = buf;
							if (buf2.GetSize() > max_send)
							{
								buf2 = buf2.GetFirst(max_send);
							}

							DiscardReturnValue(call->m_Extender.SendCallAVSample(call->m_PeerLUID, MessageType::AudioSample,
																				 media_sample.TimeStamp, buf2));
							buf.RemoveFirst(buf2.GetSize());
						}

						media_sample.New = false;
						media_sample.SampleBuffer.Clear();
					}
				});
			}

			if (settings & static_cast<UInt8>(CallSetting::SendVideo))
			{
				call->m_VideoInSample.WithUniqueLock([&](auto& media_sample)
				{
					if (media_sample.New)
					{
						// Video frame size should not be larger than what we can send
						assert(media_sample.SampleBuffer.GetSize() <= call->m_Extender.GetMaximumMessageDataSize());

						DiscardReturnValue(call->m_Extender.SendCallAVSample(call->m_PeerLUID, MessageType::VideoSample,
																			 media_sample.TimeStamp, media_sample.SampleBuffer));
						media_sample.New = false;
						media_sample.SampleBuffer.Clear();
					}
				});
			}

			if (settings & static_cast<UInt8>(CallSetting::PeerSendAudio))
			{
				call->m_AudioOutSample.WithUniqueLock([&](auto& media_sample)
				{
					if (media_sample.New)
					{
						call->OnPeerAudioSample(media_sample.TimeStamp, media_sample.SampleBuffer);
						media_sample.New = false;
					}
				});
			}

			if (settings & static_cast<UInt8>(CallSetting::PeerSendVideo))
			{
				call->m_VideoOutSample.WithUniqueLock([&](auto& media_sample)
				{
					if (media_sample.New)
					{
						call->OnPeerVideoSample(media_sample.TimeStamp, media_sample.SampleBuffer);
						media_sample.New = false;
					}
				});
			}

			call->UpdateVideoWindow();

			// Sleep for a while or until we have to shut down
			call->m_ShutdownEvent.Wait(0ms);
		}

		call->CloseVideoWindow();
		call->CloseAudioRenderer();

		LogDbg(L"Call worker thread %u exiting", std::this_thread::get_id());
	}

	void Call::SetAVCallbacks() noexcept
	{
		auto audiocb = QuantumGate::MakeCallback(this, &Call::OnAudioSample);
		auto videocb = QuantumGate::MakeCallback(this, &Call::OnVideoSample);

		auto avsource = m_AVSource.WithUniqueLock();
		m_AudioSampleEventFunctionHandle = avsource->AudioSourceReader.AddSampleEventCallback(std::move(audiocb));
		m_VideoSampleEventFunctionHandle = avsource->VideoSourceReader.AddSampleEventCallback(std::move(videocb));
	}

	void Call::UnsetAVCallbacks() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();
		avsource->AudioSourceReader.RemoveSampleEventCallback(m_AudioSampleEventFunctionHandle);
		avsource->VideoSourceReader.RemoveSampleEventCallback(m_VideoSampleEventFunctionHandle);
	}

	void Call::OnConnected()
	{
		SetAVCallbacks();

		m_ShutdownEvent.Reset();

		m_Thread = std::thread(Call::WorkerThreadLoop, this);
	}

	void Call::OnDisconnected()
	{
		UnsetAVCallbacks();

		m_ShutdownEvent.Set();

		if (m_Thread.joinable())
		{
			m_Thread.join();
		}
	}

	void Call::OnAVSourceChange() noexcept
	{
		if (IsInCall())
		{
			SetAVCallbacks();
		}
	}

	const WChar* Call::GetStatusString() const noexcept
	{
		switch (GetStatus())
		{
			case CallStatus::Disconnected:
				return L"Disconnected";
			case CallStatus::NeedAccept:
				return L"Need accept";
			case CallStatus::WaitingForAccept:
				return L"Waiting for accept";
			case CallStatus::Connected:
				return L"Connected";
			default:
				break;
		}

		return L"Unknown";
	}

	bool Call::BeginCall() noexcept
	{
		if (SetStatus(CallStatus::WaitingForAccept))
		{
			SetType(CallType::Outgoing);
			return true;
		}

		return false;
	}

	bool Call::CancelCall() noexcept
	{
		if (SetStatus(CallStatus::Disconnected))
		{
			SetType(CallType::None);
			return true;
		}

		return false;
	}

	bool Call::AcceptCall() noexcept
	{
		if (SetStatus(CallStatus::Connected))
		{
			return true;
		}

		return false;
	}

	bool Call::StopCall() noexcept
	{
		if (SetStatus(CallStatus::Disconnected))
		{
			SetType(CallType::None);
			return true;
		}

		return false;
	}

	bool Call::ProcessIncomingCall() noexcept
	{
		if (SetStatus(CallStatus::NeedAccept))
		{
			SetType(CallType::Incoming);
			return true;
		}

		return false;
	}

	bool Call::ProcessCallFailure() noexcept
	{
		if (SetStatus(CallStatus::Disconnected))
		{
			SetType(CallType::None);
			return true;
		}

		return false;
	}

	bool Call::IsInCall() const noexcept
	{
		if (GetType() != CallType::None && GetStatus() == CallStatus::Connected)
		{
			return true;
		}

		return false;
	}

	bool Call::IsCalling() const noexcept
	{
		if (GetType() != CallType::None &&
			(GetStatus() == CallStatus::NeedAccept || GetStatus() == CallStatus::WaitingForAccept))
		{
			return true;
		}

		return false;
	}

	bool Call::IsDisconnected() const noexcept
	{
		if (GetType() == CallType::None && GetStatus() == CallStatus::Disconnected)
		{
			return true;
		}

		return false;
	}

	bool Call::IsWaitExpired() const noexcept
	{
		if (Util::GetCurrentSteadyTime() - GetLastActiveSteadyTime() > Call::MaxWaitTimeForAccept)
		{
			return true;
		}

		return false;
	}

	void Call::OpenVideoWindow() noexcept
	{
		m_VideoOut.WithUniqueLock([&](auto& vout)
		{
			const bool visible = (vout.VideoFormat.Format != VideoFormat::PixelFormat::Unknown);

			if (vout.VideoWindow.Create(GetType() == CallType::Incoming ? L"Incoming" : L"Outgoing",
										NULL, WS_OVERLAPPED | WS_THICKFRAME,
										CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, visible, NULL))
			{
				if (vout.VideoFormat.Format != VideoFormat::PixelFormat::Unknown)
				{
					if (!vout.VideoWindow.SetInputFormat(vout.VideoFormat))
					{
						LogErr(L"Failed to set output format for video window");
					}
				}
			}
			else
			{
				LogErr(L"Failed to create call video window");
			}
		});
	}

	void Call::CloseVideoWindow() noexcept
	{
		m_VideoOut.WithUniqueLock()->VideoWindow.Close();
	}

	void Call::UpdateVideoWindow() noexcept
	{
		m_VideoOut.WithUniqueLock([](auto& vout)
		{
			if (vout.VideoFormat.Format == VideoFormat::PixelFormat::Unknown)
			{
				vout.VideoWindow.SetWindowVisible(false);
			}
			else
			{
				vout.VideoWindow.SetWindowVisible(true);
			}

			vout.VideoWindow.ProcessMessages();
		});
	}

	void Call::OpenAudioRenderer() noexcept
	{
		m_AudioOut.WithUniqueLock([](auto& aout)
		{
			if (aout.AudioFormat.NumChannels > 0)
			{
				if (aout.AudioRenderer.Create(aout.AudioFormat))
				{
					DiscardReturnValue(aout.AudioRenderer.Play());
				}
				else
				{
					LogErr(L"Failed to create call audio renderer");
				}
			}
		});
	}

	void Call::CloseAudioRenderer() noexcept
	{
		m_AudioOut.WithUniqueLock()->AudioRenderer.Close();
	}

	bool Call::SetPeerAVFormat(const CallAVFormatData* fmtdata) noexcept
	{
		if (fmtdata->SendAudio == 1)
		{
			if (fmtdata->AudioFormat.AvgBytesPerSecond !=
				(fmtdata->AudioFormat.SamplesPerSecond * fmtdata->AudioFormat.BlockAlignment))
			{
				return false;
			}

			AudioFormat afmt;
			afmt.NumChannels = fmtdata->AudioFormat.NumChannels;
			afmt.SamplesPerSecond = fmtdata->AudioFormat.SamplesPerSecond;
			afmt.AvgBytesPerSecond = fmtdata->AudioFormat.AvgBytesPerSecond;
			afmt.BlockAlignment = fmtdata->AudioFormat.BlockAlignment;
			afmt.BitsPerSample = fmtdata->AudioFormat.BitsPerSample;

			SetPeerSendAudio(true);
			SetPeerAudioFormat(afmt);
		}
		else
		{
			SetPeerSendAudio(false);
			SetPeerAudioFormat({});
		}

		if (fmtdata->SendVideo == 1)
		{
			VideoFormat vfmt;
			vfmt.Format = fmtdata->VideoFormat.Format;
			vfmt.BytesPerPixel = fmtdata->VideoFormat.BytesPerPixel;
			vfmt.Width = fmtdata->VideoFormat.Width;
			vfmt.Height = fmtdata->VideoFormat.Height;

			SetPeerSendVideo(true);
			SetPeerVideoFormat(vfmt);
		}
		else
		{
			SetPeerSendVideo(false);
			SetPeerVideoFormat({});
		}

		return true;
	}

	void Call::SetPeerAudioFormat(const AudioFormat& format) noexcept
	{
		bool changed{ false };

		m_AudioOut.WithUniqueLock([&](auto& aout)
		{
			changed = (std::memcmp(&aout.AudioFormat, &format, sizeof(AudioFormat)) != 0);

			aout.AudioFormat = format;
		});

		if (changed && IsInCall())
		{
			CloseAudioRenderer();
			OpenAudioRenderer();
		}
	}

	void Call::SetPeerVideoFormat(const VideoFormat& format) noexcept
	{
		m_VideoOut.WithUniqueLock([&](auto& vout)
		{
			const bool changed = (std::memcmp(&vout.VideoFormat, &format, sizeof(AudioFormat)) != 0);

			vout.VideoFormat = format;

			if (changed && IsInCall())
			{
				if (vout.VideoFormat.Format != VideoFormat::PixelFormat::Unknown)
				{
					if (!vout.VideoWindow.SetInputFormat(vout.VideoFormat))
					{
						LogErr(L"Failed to set output format for video window");
					}
				}
			}
		});
	}

	std::chrono::milliseconds Call::GetDuration() const noexcept
	{
		if (IsInCall())
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() - GetStartSteadyTime());
		}

		return 0ms;
	}

	void Call::OnPeerAudioSample(const UInt64 timestamp, const Buffer& sample) noexcept
	{
		m_AudioOut.WithUniqueLock([&](auto& aout)
		{
			if (aout.AudioRenderer.IsOpen())
			{
				DiscardReturnValue(aout.AudioRenderer.Render(timestamp, sample));
			}
		});
	}

	void Call::OnPeerVideoSample(const UInt64 timestamp, const Buffer& sample) noexcept
	{
		m_VideoOut.WithUniqueLock([&](auto& vout)
		{
			if (vout.VideoWindow.IsOpen() && vout.VideoFormat.Format != VideoFormat::PixelFormat::Unknown)
			{
				DiscardReturnValue(vout.VideoWindow.Render(timestamp, sample));
			}
		});
	}

	void Call::OnAudioSample(const UInt64 timestamp, IMFSample* sample)
	{
		OnInputSample(timestamp, sample, m_AudioInSample, true);
	}

	void Call::OnVideoSample(const UInt64 timestamp, IMFSample* sample)
	{
		OnInputSample(timestamp, sample, m_VideoInSample, false);
	}

	void Call::OnInputSample(const UInt64 timestamp, IMFSample* sample, MediaSample_ThS& sample_ths, const bool accumulate)
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
				sample_ths.WithUniqueLock([&](auto& s)
				{
					s.New = true;
					s.TimeStamp = timestamp;

					if (accumulate)
					{
						s.SampleBuffer += BufferView(reinterpret_cast<Byte*>(data), data_len);
					}
					else s.SampleBuffer = BufferView(reinterpret_cast<Byte*>(data), data_len);
				});

				media_buffer->Unlock();
			}
		}
	}
}