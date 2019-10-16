// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Call.h"
#include "AVExtender.h"
#include "AudioCompressor.h"
#include "VideoCompressor.h"

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

	void Call::StartAVThreads() noexcept
	{
		m_DisconnectEvent.Reset();

		m_AudioInQueue.WithUniqueLock()->Event().Reset();
		m_AudioOutQueue.WithUniqueLock()->Event().Reset();
		m_VideoInQueue.WithUniqueLock()->Event().Reset();
		m_VideoOutQueue.WithUniqueLock()->Event().Reset();

		m_AudioInThread = std::thread(Call::AudioInWorkerThreadLoop, this);
		m_AudioOutThread = std::thread(Call::AudioOutWorkerThreadLoop, this);
		m_VideoInThread = std::thread(Call::VideoInWorkerThreadLoop, this);
		m_VideoOutThread = std::thread(Call::VideoOutWorkerThreadLoop, this);
	}

	void Call::StopAVThreads() noexcept
	{
		// Set shutdown event to let the
		// threads begin exiting 
		m_DisconnectEvent.Set();

		m_AudioInQueue.WithUniqueLock()->Event().Set();
		if (m_AudioInThread.joinable())
		{
			m_AudioInThread.join();
		}

		m_AudioOutQueue.WithUniqueLock()->Event().Set();
		if (m_AudioOutThread.joinable())
		{
			m_AudioOutThread.join();
		}

		m_VideoInQueue.WithUniqueLock()->Event().Set();
		if (m_VideoInThread.joinable())
		{
			m_VideoInThread.join();
		}

		m_VideoOutQueue.WithUniqueLock()->Event().Set();
		if (m_VideoOutThread.joinable())
		{
			m_VideoOutThread.join();
		}

		m_AVInFormats.WithUniqueLock()->Clear();

		// Clear queues
		m_AudioInQueue.WithUniqueLock()->Clear();
		m_VideoInQueue.WithUniqueLock()->Clear();
		m_AudioOutQueue.WithUniqueLock()->Clear();
		m_VideoOutQueue.WithUniqueLock()->Clear();
	}

	void Call::AudioInWorkerThreadLoop(Call* call)
	{
		LogDbg(L"Call audio in worker thread %u starting for peer %llu", std::this_thread::get_id(), call->GetPeerLUID());

		Util::SetCurrentThreadName(L"AVExtender Call AudioIn Thread");

		AudioFormat rcv_audio_in_format;
		AudioCompressor audio_decompressor{ AudioCompressor::Type::Decoder };

		auto& event = call->m_AudioInQueue.WithUniqueLock()->Event();

		while (true)
		{
			// Wait for work
			event.Wait();

			// If the shutdown event is set quit the loop
			if (call->m_DisconnectEvent.IsSet()) break;

			auto optsample = call->GetSampleFromQueue<AudioSample>(call->m_AudioInQueue);
			if (optsample.has_value())
			{
				auto& media_sample = optsample.value();

				bool changed{ false };

				call->m_AudioRenderer.WithUniqueLock([&](auto& ar)
				{
					if (rcv_audio_in_format != media_sample.Format)
					{
						rcv_audio_in_format = media_sample.Format;
						changed = true;
					}
				});

				if (changed)
				{
					audio_decompressor.Close();
					if (!audio_decompressor.Create())
					{
						LogErr(L"Failed to create audio decompressor; cannot play compressed audio from peer");
					}

					call->CloseAudioRenderer();
					call->OpenAudioRenderer(rcv_audio_in_format);
				}

				call->m_AudioRenderer.WithUniqueLock([&](auto& ar)
				{
					if (ar.IsOpen())
					{
						if (media_sample.Compressed && audio_decompressor.IsOpen())
						{
							DiscardReturnValue(audio_decompressor.AddInput(media_sample.TimeStamp, media_sample.SampleBuffer));
							while (audio_decompressor.GetOutput(media_sample.SampleBuffer))
							{
								if (!ar.Render(media_sample.TimeStamp, media_sample.SampleBuffer))
								{
									LogErr(L"Failed to render audio sample");
								}
							}
						}
						else if (!media_sample.Compressed)
						{
							if (!ar.Render(media_sample.TimeStamp, media_sample.SampleBuffer))
							{
								LogErr(L"Failed to render audio sample");
							}
						}
					}
				});
			}
		}

		audio_decompressor.Close();

		call->CloseAudioRenderer();

		LogDbg(L"Call audio in worker thread %u exiting for peer %llu", std::this_thread::get_id(), call->GetPeerLUID());
	}

	void Call::AudioOutWorkerThreadLoop(Call* call)
	{
		LogDbg(L"Call audio out worker thread %u starting for peer %llu", std::this_thread::get_id(), call->GetPeerLUID());

		Util::SetCurrentThreadName(L"AVExtender Call AudioOut Thread");

		AudioFormat snd_audio_in_format;
		AudioCompressor audio_compressor{ AudioCompressor::Type::Encoder };

		auto send_audio = [](Call* call, AudioSample& media_sample)
		{
			// Try to send at most 1 second of audio data at once
			const auto max_send{ media_sample.Format.AvgBytesPerSecond };

			// Audio frames total size should not be larger than what we can send
			assert(max_send <= call->m_Extender.GetMaximumMessageDataSize());

			BufferView buf{ media_sample.SampleBuffer };

			while (!buf.IsEmpty())
			{
				auto buf2 = buf;
				if (buf2.GetSize() > max_send)
				{
					buf2 = buf2.GetFirst(max_send);
				}

				if (!call->m_Extender.SendCallAudioSample(call->m_PeerLUID, media_sample.Format,
														  media_sample.TimeStamp, buf2, media_sample.Compressed))
				{
					LogErr(L"Failed to send audio sample to peer");
				}

				buf.RemoveFirst(buf2.GetSize());
			}
		};

		auto& event = call->m_AudioOutQueue.WithUniqueLock()->Event();

		while (true)
		{
			// Wait for work
			event.Wait();

			// If the shutdown event is set quit the loop
			if (call->m_DisconnectEvent.IsSet()) break;

			const auto settings = call->m_Settings.load();

			if (settings & static_cast<UInt8>(CallSetting::SendAudio))
			{
				auto optsample = call->GetSampleFromQueue<AudioSample>(call->m_AudioOutQueue);
				if (optsample.has_value())
				{
					auto& media_sample = optsample.value();

					if (snd_audio_in_format != media_sample.Format)
					{
						snd_audio_in_format = media_sample.Format;

						audio_compressor.Close();
						if (!audio_compressor.Create())
						{
							LogErr(L"Failed to create audio compressor; cannot send compressed audio to peer");
						}
					}

					const auto use_compression = call->m_ExtenderSettings->UseAudioCompression;

					if (use_compression && audio_compressor.IsOpen())
					{
						DiscardReturnValue(audio_compressor.AddInput(media_sample.TimeStamp, media_sample.SampleBuffer));
						while (audio_compressor.GetOutput(media_sample.SampleBuffer))
						{
							media_sample.Compressed = true;
							send_audio(call, media_sample);
						}
					}
					else if (!use_compression)
					{
						send_audio(call, media_sample);
					}
				}
			}
		}

		audio_compressor.Close();

		LogDbg(L"Call audio out worker thread %u exiting for peer %llu", std::this_thread::get_id(), call->GetPeerLUID());
	}

	void Call::VideoInWorkerThreadLoop(Call* call)
	{
		LogDbg(L"Call video in worker thread %u starting for peer %llu", std::this_thread::get_id(), call->GetPeerLUID());

		Util::SetCurrentThreadName(L"AVExtender Call VideoIn Thread");

		call->OpenVideoRenderer();

		bool video_fill{ false };
		VideoFormat rcv_video_in_format;
		VideoCompressor video_decompressor{ VideoCompressor::Type::Decoder };

		auto& event = call->m_VideoInQueue.WithUniqueLock()->Event();

		while (true)
		{
			// Wait for work for a brief period
			// to allow updating video window
			event.Wait(100ms);

			// If the shutdown event is set quit the loop
			if (call->m_DisconnectEvent.IsSet()) break;

			auto optsample = call->GetSampleFromQueue<VideoSample>(call->m_VideoInQueue);
			if (optsample.has_value())
			{
				auto& media_sample = optsample.value();

				call->m_VideoRenderer.WithUniqueLock([&](auto& vr)
				{
					if (rcv_video_in_format != media_sample.Format)
					{
						rcv_video_in_format = media_sample.Format;

						video_decompressor.Close();
						video_decompressor.SetFormat(rcv_video_in_format.Width, rcv_video_in_format.Height,
													 CaptureDevices::GetMFVideoFormat(rcv_video_in_format.Format));
						if (!video_decompressor.Create())
						{
							LogErr(L"Failed to create video decompressor; cannot display compressed video from peer");
						}

						if (!vr.SetInputFormat(rcv_video_in_format))
						{
							LogErr(L"Failed to set output format for video window");
						}
					}

					if (vr.IsOpen())
					{
						const auto fill_screen = call->m_ExtenderSettings->FillVideoScreen;

						if (video_fill != fill_screen)
						{
							video_fill = fill_screen;

							vr.SetRenderSize(video_fill ? AVExtender::VideoRenderer::RenderSize::Cover :
											 AVExtender::VideoRenderer::RenderSize::Fit);
						}

						if (media_sample.Compressed && video_decompressor.IsOpen())
						{
							DiscardReturnValue(video_decompressor.AddInput(media_sample.TimeStamp, media_sample.SampleBuffer));
							while (video_decompressor.GetOutput(media_sample.SampleBuffer))
							{
								if (!vr.Render(media_sample.TimeStamp, media_sample.SampleBuffer))
								{
									LogErr(L"Failed to render video sample");
								}
							}
						}
						else if (!media_sample.Compressed)
						{
							if (!vr.Render(media_sample.TimeStamp, media_sample.SampleBuffer))
							{
								LogErr(L"Failed to render video sample");
							}
						}
					}
				});
			}

			call->UpdateVideoRenderer();
		}

		video_decompressor.Close();

		call->CloseVideoRenderer();

		LogDbg(L"Call video in worker thread %u exiting for peer %llu", std::this_thread::get_id(), call->GetPeerLUID());
	}

	void Call::VideoOutWorkerThreadLoop(Call* call)
	{
		LogDbg(L"Call video out worker thread %u starting for peer %llu", std::this_thread::get_id(), call->GetPeerLUID());

		Util::SetCurrentThreadName(L"AVExtender Call VideoOut Thread");

		VideoFormat snd_video_in_format;
		VideoCompressor video_compressor{ VideoCompressor::Type::Encoder };

		auto send_video = [](Call* call, VideoSample& media_sample)
		{
			// Video frame size should not be larger than what we can send
			assert(media_sample.SampleBuffer.GetSize() <= call->m_Extender.GetMaximumMessageDataSize());

			if (!call->m_Extender.SendCallVideoSample(call->m_PeerLUID, media_sample.Format,
													  media_sample.TimeStamp, media_sample.SampleBuffer,
													  media_sample.Compressed))
			{
				LogErr(L"Failed to send video sample to peer");
			}
		};

		auto& event = call->m_VideoOutQueue.WithUniqueLock()->Event();

		while (true)
		{
			// Wait for work
			event.Wait();

			// If the shutdown event is set quit the loop
			if (call->m_DisconnectEvent.IsSet()) break;

			const auto settings = call->m_Settings.load();

			if (settings & static_cast<UInt8>(CallSetting::SendVideo))
			{
				auto optsample = call->GetSampleFromQueue<VideoSample>(call->m_VideoOutQueue);
				if (optsample.has_value())
				{
					auto& media_sample = optsample.value();

					if (snd_video_in_format != media_sample.Format)
					{
						snd_video_in_format = media_sample.Format;

						video_compressor.Close();
						video_compressor.SetFormat(snd_video_in_format.Width, snd_video_in_format.Height,
												   CaptureDevices::GetMFVideoFormat(snd_video_in_format.Format));
						if (!video_compressor.Create())
						{
							LogErr(L"Failed to create video compressor; cannot send compressed video to peer");
						}
					}

					const auto use_compression = call->m_ExtenderSettings->UseVideoCompression;

					if (use_compression && video_compressor.IsOpen())
					{
						DiscardReturnValue(video_compressor.AddInput(media_sample.TimeStamp, media_sample.SampleBuffer));
						while (video_compressor.GetOutput(media_sample.SampleBuffer))
						{
							media_sample.Compressed = true;
							send_video(call, media_sample);
						}
					}
					else if (!use_compression)
					{
						send_video(call, media_sample);
					}
				}
			}
		}

		video_compressor.Close();

		LogDbg(L"Call video out worker thread %u exiting for peer %llu", std::this_thread::get_id(), call->GetPeerLUID());
	}

	void Call::SetAudioCallbacks() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();

		if (GetSendAudio())
		{
			m_AVInFormats.WithUniqueLock()->AudioFormat = avsource->AudioSourceReader.GetSampleFormat();

			auto audiocb = QuantumGate::MakeCallback(this, &Call::OnAudioOutSample);
			m_SampleEventHandles.WithUniqueLock()->AudioSampleEventFunctionHandle = avsource->AudioSourceReader.AddSampleEventCallback(std::move(audiocb));
		}
	}

	void Call::SetVideoCallbacks() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();

		if (GetSendVideo())
		{
			m_AVInFormats.WithUniqueLock()->VideoFormat = avsource->VideoSourceReader.GetSampleFormat();

			auto videocb = QuantumGate::MakeCallback(this, &Call::OnVideoOutSample);
			m_SampleEventHandles.WithUniqueLock()->VideoSampleEventFunctionHandle = avsource->VideoSourceReader.AddSampleEventCallback(std::move(videocb));
		}
	}

	void Call::UnsetAVCallbacks() noexcept
	{
		auto avsource = m_AVSource.WithUniqueLock();
		avsource->AudioSourceReader.RemoveSampleEventCallback(m_SampleEventHandles.WithUniqueLock()->AudioSampleEventFunctionHandle);
		avsource->VideoSourceReader.RemoveSampleEventCallback(m_SampleEventHandles.WithUniqueLock()->VideoSampleEventFunctionHandle);
	}

	void Call::SetSendVideo(const bool send) noexcept
	{
		SetSetting(CallSetting::SendVideo, send);

		if (IsInCall() && send)
		{
			auto avsource = m_AVSource.WithUniqueLock();
			m_AVInFormats.WithUniqueLock()->VideoFormat = avsource->VideoSourceReader.GetSampleFormat();

			auto videocb = QuantumGate::MakeCallback(this, &Call::OnVideoOutSample);

			m_SampleEventHandles.WithUniqueLock()->VideoSampleEventFunctionHandle = avsource->VideoSourceReader.AddSampleEventCallback(std::move(videocb));
		}
		else
		{
			auto avsource = m_AVSource.WithUniqueLock();
			avsource->VideoSourceReader.RemoveSampleEventCallback(m_SampleEventHandles.WithUniqueLock()->VideoSampleEventFunctionHandle);

			m_AVInFormats.WithUniqueLock()->VideoFormat = {};
		}
	}

	void Call::SetSendAudio(const bool send) noexcept
	{
		SetSetting(CallSetting::SendAudio, send);

		if (IsInCall() && send)
		{
			auto avsource = m_AVSource.WithUniqueLock();
			m_AVInFormats.WithUniqueLock()->AudioFormat = avsource->AudioSourceReader.GetSampleFormat();

			auto audiocb = QuantumGate::MakeCallback(this, &Call::OnAudioOutSample);

			m_SampleEventHandles.WithUniqueLock()->AudioSampleEventFunctionHandle = avsource->AudioSourceReader.AddSampleEventCallback(std::move(audiocb));
		}
		else
		{
			auto avsource = m_AVSource.WithUniqueLock();
			avsource->AudioSourceReader.RemoveSampleEventCallback(m_SampleEventHandles.WithUniqueLock()->AudioSampleEventFunctionHandle);

			m_AVInFormats.WithUniqueLock()->AudioFormat = {};
		}
	}

	void Call::OnConnected()
	{
		SetAudioCallbacks();
		SetVideoCallbacks();

		StartAVThreads();
	}

	void Call::OnDisconnected()
	{
		UnsetAVCallbacks();

		StopAVThreads();
	}

	void Call::OnAudioSourceChange() noexcept
	{
		if (IsInCall())
		{
			SetAudioCallbacks();
		}
	}

	void Call::OnVideoSourceChange() noexcept
	{
		if (IsInCall())
		{
			SetVideoCallbacks();
		}
	}

	template<typename T, typename U>
	bool Call::AddSampleToQueue(T&& sample, U& queue_ths) noexcept
	{
		auto success{ false };

		auto max_queue_size{ 4u };

		if constexpr (std::is_same_v<T, AudioFormat>)
		{
			max_queue_size = 16u;
		}

		queue_ths.WithUniqueLock([&](auto& queue)
		{
			try
			{
				if (queue.GetSize() <= max_queue_size)
				{
					queue.Push(std::move(sample));
				}

				success = true;
			}
			catch (...) {}
		});

		if (!success)
		{
			if constexpr (std::is_same_v<T, AudioFormat>)
			{
				LogDbg(L"Audio sample queue is full");
			}
			else if constexpr (std::is_same_v<T, VideoFormat>)
			{
				LogDbg(L"Video sample queue is full");
			}
		}

		return success;
	}

	template<typename T, typename U>
	std::optional<T> Call::GetSampleFromQueue(U& queue_ths) noexcept
	{
		auto queue = queue_ths.WithUniqueLock();
		if (!queue->Empty())
		{
			T sample = std::move(queue->Front());
			queue->Pop();
			return sample;
		}

		return std::nullopt;
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

	void Call::OpenVideoRenderer() noexcept
	{
		m_VideoRenderer.WithUniqueLock([&](auto& vr)
		{
			auto title = Util::FormatString(L"%s call from peer %llu",
											GetType() == CallType::Incoming ? L"Incoming" : L"Outgoing", GetPeerLUID());

			if (!vr.Create(title.c_str(), NULL, WS_OVERLAPPED | WS_THICKFRAME,
						   CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, true, NULL))
			{
				LogErr(L"Failed to create call video window; cannot display video from peer");
			}
		});
	}

	void Call::CloseVideoRenderer() noexcept
	{
		m_VideoRenderer.WithUniqueLock()->Close();
	}

	void Call::UpdateVideoRenderer() noexcept
	{
		m_VideoRenderer.WithUniqueLock()->ProcessMessages();
	}

	void Call::OpenAudioRenderer(const AudioFormat& fmt) noexcept
	{
		m_AudioRenderer.WithUniqueLock([&](auto& ar)
		{
			if (ar.Create(fmt))
			{
				DiscardReturnValue(ar.Play());
			}
			else
			{
				LogErr(L"Failed to create call audio renderer; cannot play audio from peer");
			}
		});
	}

	void Call::CloseAudioRenderer() noexcept
	{
		m_AudioRenderer.WithUniqueLock()->Close();
	}

	std::chrono::milliseconds Call::GetDuration() const noexcept
	{
		if (IsInCall())
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() - GetStartSteadyTime());
		}

		return 0ms;
	}

	void Call::OnAudioInSample(const AudioFormatData& fmt, const UInt64 timestamp, Buffer&& sample) noexcept
	{
		AudioSample asample;
		asample.Format.NumChannels = fmt.NumChannels;
		asample.Format.BlockAlignment = fmt.BlockAlignment;
		asample.Format.BitsPerSample = fmt.BitsPerSample;
		asample.Format.SamplesPerSecond = fmt.SamplesPerSecond;
		asample.Format.AvgBytesPerSecond = fmt.AvgBytesPerSecond;

		asample.TimeStamp = timestamp;
		asample.Compressed = fmt.Compressed;
		asample.SampleBuffer = std::move(sample);

		DiscardReturnValue(AddSampleToQueue(std::move(asample), m_AudioInQueue));
	}

	void Call::OnVideoInSample(const VideoFormatData& fmt, const UInt64 timestamp, Buffer&& sample) noexcept
	{
		VideoSample vsample;
		vsample.Format.Format = fmt.Format;
		vsample.Format.Width = fmt.Width;
		vsample.Format.Height = fmt.Height;
		vsample.Format.BytesPerPixel = fmt.BytesPerPixel;

		vsample.TimeStamp = timestamp;
		vsample.Compressed = fmt.Compressed;
		vsample.SampleBuffer = std::move(sample);

		DiscardReturnValue(AddSampleToQueue(std::move(vsample), m_VideoInQueue));
	}

	void Call::OnAudioOutSample(const UInt64 timestamp, IMFSample* sample)
	{
		AudioSample asample;
		asample.TimeStamp = timestamp;
		asample.Format = m_AVInFormats.WithUniqueLock()->AudioFormat;
		if (CopySample(timestamp, sample, asample.SampleBuffer))
		{
			DiscardReturnValue(AddSampleToQueue(std::move(asample), m_AudioOutQueue));
		}
	}

	void Call::OnVideoOutSample(const UInt64 timestamp, IMFSample* sample)
	{
		VideoSample vsample;
		vsample.TimeStamp = timestamp;
		vsample.Format = m_AVInFormats.WithUniqueLock()->VideoFormat;
		if (CopySample(timestamp, sample, vsample.SampleBuffer))
		{
			DiscardReturnValue(AddSampleToQueue(std::move(vsample), m_VideoOutQueue));
		}
	}

	bool Call::CopySample(const UInt64 timestamp, IMFSample* sample, Buffer& sample_buffer)
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
				sample_buffer = BufferView(reinterpret_cast<Byte*>(data), data_len);

				media_buffer->Unlock();

				return true;
			}
		}

		return false;
	}
}