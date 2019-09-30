// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Protocol.h"
#include "AudioSourceReader.h"
#include "VideoSourceReader.h"
#include "VideoWindow.h"
#include "AudioRenderer.h"

#include <QuantumGate.h>
#include <Concurrency\ThreadSafe.h>
#include <Concurrency\EventCondition.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate;
	using namespace QuantumGate::Implementation;

	class Extender;

	struct AVSource
	{
		AudioSourceReader AudioSourceReader;
		String AudioEndpointID;
		VideoSourceReader VideoSourceReader;
		String VideoSymbolicLink;
	};

	using AVSource_ThS = Implementation::Concurrency::ThreadSafe<AVSource, std::shared_mutex>;

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

	struct MediaSample
	{
		bool New{ false };
		UInt64 TimeStamp{ 0 };
		Buffer SampleBuffer;
	};

	using MediaSample_ThS = QuantumGate::Implementation::Concurrency::ThreadSafe<MediaSample, std::shared_mutex>;

	struct VideoOut
	{
		VideoFormat VideoFormat;
		VideoWindow VideoWindow;
	};

	using VideoOut_ThS = QuantumGate::Implementation::Concurrency::ThreadSafe<VideoOut, std::shared_mutex>;

	struct AudioOut
	{
		AudioFormat AudioFormat;
		AudioRenderer AudioRenderer;
	};

	using AudioOut_ThS = QuantumGate::Implementation::Concurrency::ThreadSafe<AudioOut, std::shared_mutex>;

	class Call final
	{
	public:
		Call(Extender& extender, AVSource_ThS& avsource, const PeerLUID pluid) noexcept :
			m_PeerLUID(pluid), m_Extender(extender), m_AVSource(avsource)
		{};

		~Call();

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

		inline void SetType(const CallType type) noexcept { m_Type = type; }
		[[nodiscard]] inline CallType GetType() const noexcept { return m_Type; }

		[[nodiscard]] inline SteadyTime GetLastActiveSteadyTime() const noexcept { return m_LastActiveSteadyTime; }
		[[nodiscard]] inline SteadyTime GetStartSteadyTime() const noexcept { return m_StartSteadyTime; }
		[[nodiscard]] std::chrono::milliseconds GetDuration() const noexcept;

		inline void SetSendVideo(const bool send) noexcept { m_SendVideo = send; }
		[[nodiscard]] inline bool GetSendVideo() const noexcept { return m_SendVideo; }

		inline void SetSendAudio(const bool send) noexcept { m_SendAudio = send; }
		[[nodiscard]] inline bool GetSendAudio() const noexcept { return m_SendAudio; }

		[[nodiscard]] bool SetPeerAVFormat(const CallAVFormatData* fmtdata) noexcept;
		void OnAVSourceChange() noexcept;

		inline MediaSample_ThS& GetAudioOutSample() noexcept { return m_AudioOutSample; }
		inline MediaSample_ThS& GetVideoOutSample() noexcept { return m_VideoOutSample; }

	public:
		static constexpr std::chrono::seconds MaxWaitTimeForAccept{ 30 };

	private:
		[[nodiscard]] bool SetStatus(const CallStatus status) noexcept;

		inline void SetPeerSendVideo(const bool send) noexcept { m_PeerSendVideo = send; }
		[[nodiscard]] inline bool GetPeerSendVideo() const noexcept { return m_PeerSendVideo; }

		inline void SetPeerSendAudio(const bool send) noexcept { m_PeerSendAudio = send; }
		[[nodiscard]] inline bool GetPeerSendAudio() const noexcept { return m_PeerSendAudio; }

		void SetPeerAudioFormat(const AudioFormat& format) noexcept;
		void SetPeerVideoFormat(const VideoFormat& format) noexcept;

		static void WorkerThreadLoop(Call* call);

		void OnConnected();
		void OnDisconnected();

		void OnAudioSample(const UInt64 timestamp, IMFSample* sample);
		void OnVideoSample(const UInt64 timestamp, IMFSample* sample);

		void OnPeerAudioSample(const UInt64 timestamp, const Buffer& sample) noexcept;
		void OnPeerVideoSample(const UInt64 timestamp, const Buffer& sample) noexcept;

		void OpenVideoWindow() noexcept;
		void CloseVideoWindow() noexcept;
		void UpdateVideoWindow() noexcept;

		void OpenAudioRenderer() noexcept;
		void CloseAudioRenderer() noexcept;

		void SetAVCallbacks() noexcept;
		void UnsetAVCallbacks() noexcept;

		void OnInputSample(const UInt64 timestamp, IMFSample* sample, MediaSample_ThS& sample_ths);

	private:
		const PeerLUID m_PeerLUID{ 0 };
		Extender& m_Extender;
		AVSource_ThS& m_AVSource;

		CallType m_Type{ CallType::None };
		CallStatus m_Status{ CallStatus::Disconnected };
		SteadyTime m_LastActiveSteadyTime;
		SteadyTime m_StartSteadyTime;

		std::atomic_bool m_SendVideo{ false };
		std::atomic_bool m_SendAudio{ false };
		std::atomic_bool m_PeerSendVideo{ false };
		std::atomic_bool m_PeerSendAudio{ false };

		AudioOut_ThS m_AudioOut;
		VideoOut_ThS m_VideoOut;

		MediaSample_ThS m_AudioInSample;
		MediaSample_ThS m_AudioOutSample;
		MediaSample_ThS m_VideoInSample;
		MediaSample_ThS m_VideoOutSample;

		SourceReader::SampleEventDispatcher::FunctionHandle m_AudioSampleEventFunctionHandle;
		SourceReader::SampleEventDispatcher::FunctionHandle m_VideoSampleEventFunctionHandle;

		Concurrency::EventCondition m_ShutdownEvent{ false };
		std::thread m_Thread;
	};

	using Call_ThS = Implementation::Concurrency::ThreadSafe<Call, std::shared_mutex>;
}