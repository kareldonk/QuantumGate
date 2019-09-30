// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Call.h"

#include <Common\Util.h>

using namespace QuantumGate::Implementation;
using namespace std::literals;

namespace QuantumGate::AVExtender
{
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

	void Call::OnDisconnected()
	{
		m_SampleBuffer.Clear();
		m_SampleBuffer.FreeUnused();
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
		if (!m_VideoWindow.Create(GetType() == CallType::Incoming ? L"Incoming" : L"Outgoing",
								  NULL, WS_OVERLAPPED | WS_THICKFRAME,
								  CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL))
		{
			LogErr(L"Failed to create call video window");
		}
	}

	void Call::CloseVideoWindow() noexcept
	{
		m_VideoWindow.Close();
	}

	void Call::OpenAudioRenderer() noexcept
	{
		if (m_AudioRenderer.Create(m_PeerAudioFormat))
		{
			DiscardReturnValue(m_AudioRenderer.Play());
		}
		else
		{
			LogErr(L"Failed to create call audio renderer");
		}
	}

	void Call::CloseAudioRenderer() noexcept
	{
		m_AudioRenderer.Close();
	}

	bool Call::SetPeerAVFormat(const CallAVFormatData* fmtdata) noexcept
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

		if (std::abs(fmtdata->VideoFormat.Stride) !=
			(fmtdata->VideoFormat.Width * fmtdata->VideoFormat.BytesPerPixel))
		{
			return false;
		}

		VideoFormat vfmt;
		vfmt.Format = fmtdata->VideoFormat.Format;
		vfmt.BytesPerPixel = fmtdata->VideoFormat.BytesPerPixel;
		vfmt.Stride = fmtdata->VideoFormat.Stride;
		vfmt.Width = fmtdata->VideoFormat.Width;
		vfmt.Height = fmtdata->VideoFormat.Height;

		SetPeerSendAudio(fmtdata->SendAudio == 1);
		SetPeerSendVideo(fmtdata->SendVideo == 1);
		SetPeerAudioFormat(afmt);
		SetPeerVideoFormat(vfmt);

		return true;
	}

	void Call::SetPeerAudioFormat(const AudioFormat& format) noexcept
	{
		const bool changed = (std::memcmp(&m_PeerAudioFormat, &format, sizeof(AudioFormat)) != 0);
		
		m_PeerAudioFormat = format;

		if (changed && m_AudioRenderer.IsOpen())
		{
			CloseAudioRenderer();
			OpenAudioRenderer();
		}
	}

	void Call::SetPeerVideoFormat(const VideoFormat& format) noexcept
	{
		m_PeerVideoFormat = format;
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
		if (m_AudioRenderer.IsOpen())
		{
			DiscardReturnValue(m_AudioRenderer.Render(timestamp, sample));
		}
	}

	void Call::OnPeerVideoSample(const UInt64 timestamp, const Buffer& sample) noexcept
	{
		if (m_VideoWindow.IsOpen())
		{
			DiscardReturnValue(m_VideoWindow.Render(sample, m_PeerVideoFormat));
		}
	}
}