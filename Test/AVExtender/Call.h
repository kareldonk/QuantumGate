// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Protocol.h"
#include "VideoWindow.h"
#include "AudioRenderer.h"

#include <QuantumGate.h>
#include <Concurrency\ThreadSafe.h>

#include <queue>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate;
	using namespace QuantumGate::Implementation;

	enum class CallType : UInt16
	{
		None,
		Incoming,
		Outgoing
	};

	enum class CallStatus : UInt16
	{
		Disconnected,
		NeedAccept,
		WaitingForAccept,
		Connected
	};

	using CallID = UInt64;

	class Call final
	{
	public:
		Call() noexcept {};
		~Call() {};

		[[nodiscard]] bool SetStatus(const CallStatus status) noexcept;
		[[nodiscard]] inline const CallStatus GetStatus() const noexcept { return m_Status; }
		[[nodiscard]] const WChar* GetStatusString() const noexcept;

		[[nodiscard]] bool BeginCall() noexcept;
		[[nodiscard]] bool CancelCall() noexcept;
		[[nodiscard]] bool AcceptCall() noexcept;
		[[nodiscard]] bool StopCall() noexcept;
		[[nodiscard]] bool ProcessIncomingCall() noexcept;
		[[nodiscard]] bool ProcessCallFailure() noexcept;

		[[nodiscard]] bool IsInCall() const noexcept;
		[[nodiscard]] bool IsCalling() const noexcept;
		[[nodiscard]] bool IsDisconnected() const noexcept;
		[[nodiscard]] bool IsWaitExpired() const noexcept;

		void OpenVideoWindow() noexcept;
		void CloseVideoWindow() noexcept;
		void UpdateVideoWindow() noexcept { return m_VideoWindow.ProcessMessages(); }
		[[nodiscard]] inline bool HasVideoWindow() const noexcept { return m_VideoWindow.IsOpen(); }

		void OpenAudioRenderer() noexcept;
		void CloseAudioRenderer() noexcept;
		[[nodiscard]] inline bool HasAudioRenderer() const noexcept { return m_AudioRenderer.IsOpen(); }

		inline void SetType(const CallType type) noexcept { m_Type = type; }
		[[nodiscard]] inline CallType GetType() const noexcept { return m_Type; }

		[[nodiscard]] inline SteadyTime GetLastActiveSteadyTime() const noexcept { return m_LastActiveSteadyTime; }
		[[nodiscard]] inline SteadyTime GetStartSteadyTime() const noexcept { return m_StartSteadyTime; }
		[[nodiscard]] std::chrono::milliseconds GetDuration() const noexcept;

		inline void SetSendVideo(const bool send) noexcept { m_SendVideo = send; }
		[[nodiscard]] inline bool GetSendVideo() const noexcept { return m_SendVideo; }

		inline void SetSendAudio(const bool send) noexcept { m_SendAudio = send; }
		[[nodiscard]] inline bool GetSendAudio() const noexcept { return m_SendAudio; }

		inline void SetPeerSendVideo(const bool send) noexcept { m_PeerSendVideo = send; }
		[[nodiscard]] inline bool GetPeerSendVideo() const noexcept { return m_PeerSendVideo; }

		inline void SetPeerSendAudio(const bool send) noexcept { m_PeerSendAudio = send; }
		[[nodiscard]] inline bool GetPeerSendAudio() const noexcept { return m_PeerSendAudio; }

		[[nodiscard]] bool SetPeerAVFormat(const CallAVFormatData* fmtdata) noexcept;
		void SetPeerAudioFormat(const AudioFormat& format) noexcept;
		void SetPeerVideoFormat(const VideoFormat& format) noexcept;

		void OnPeerAudioSample(const UInt64 timestamp, const Buffer& sample) noexcept;
		void OnPeerVideoSample(const UInt64 timestamp, const Buffer& sample) noexcept;

		inline Buffer& GetSampleBuffer() noexcept { return m_SampleBuffer; }

	public:
		static constexpr std::chrono::seconds MaxWaitTimeForAccept{ 30 };

	private:
		void OnDisconnected();

	private:
		CallType m_Type{ CallType::None };
		CallStatus m_Status{ CallStatus::Disconnected };
		SteadyTime m_LastActiveSteadyTime;
		SteadyTime m_StartSteadyTime;
		bool m_SendVideo{ false };
		bool m_SendAudio{ false };

		bool m_PeerSendVideo{ false };
		bool m_PeerSendAudio{ false };
		AudioFormat m_PeerAudioFormat;
		VideoFormat m_PeerVideoFormat;

		VideoWindow m_VideoWindow;
		AudioRenderer m_AudioRenderer;

		Buffer m_SampleBuffer;
	};

	using Call_ThS = Implementation::Concurrency::ThreadSafe<Call, std::shared_mutex>;
}