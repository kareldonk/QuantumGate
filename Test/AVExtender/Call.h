// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Protocol.h"
#include "AudioSourceReader.h"
#include "VideoSourceReader.h"
#include "VideoRenderer.h"
#include "AudioRenderer.h"

#include <Concurrency\Event.h>
#include <Concurrency\SpinMutex.h>
#include <Concurrency\SharedSpinMutex.h>
#include <Concurrency\Queue.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate;
	using namespace QuantumGate::Implementation;

	class Extender;

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

	enum class CallSetting : UInt8
	{
		SendAudio = 0b00000001,
		SendVideo = 0b00000010
	};

	using CallID = UInt64;

	struct AVSource
	{
		bool Previewing{ false };
		AudioSourceReader AudioSourceReader;
		String AudioEndpointID;
		VideoSourceReader VideoSourceReader;
		String VideoSymbolicLink;
		UInt16 MaxVideoResolution{ 90 };
		bool ForceMaxVideoResolution{ false };
	};

	using AVSource_ThS = Concurrency::ThreadSafe<AVSource, std::shared_mutex>;

	struct AVFormats
	{
		AudioFormat AudioFormat;
		VideoFormat VideoFormat;

		void Clear() noexcept
		{
			VideoFormat = {};
			AudioFormat = {};
		}
	};

	using AVFormats_ThS = Concurrency::ThreadSafe<AVFormats, Concurrency::SpinMutex>;

	struct SampleEventHandles
	{
		SourceReader::SampleEventDispatcher::FunctionHandle AudioSampleEventFunctionHandle;
		SourceReader::SampleEventDispatcher::FunctionHandle VideoSampleEventFunctionHandle;
	};

	using SampleEventHandles_ThS = Concurrency::ThreadSafe<SampleEventHandles, Concurrency::SpinMutex>;

	template<typename T>
	struct MediaSample
	{
		T Format;
		UInt64 TimeStamp{ 0 };
		bool Compressed{ false };
		Buffer SampleBuffer;

		MediaSample() noexcept = default;
		MediaSample(const MediaSample&) = delete;
		MediaSample(MediaSample&&) noexcept = default;
		~MediaSample() = default;
		MediaSample& operator=(const MediaSample&) = delete;
		MediaSample& operator=(MediaSample&&) noexcept = default;
	};

	using AudioSample = MediaSample<AudioFormat>;
	using VideoSample = MediaSample<VideoFormat>;

	using AudioSampleQueue_ThS = Concurrency::Queue<AudioSample>;
	using VideoSampleQueue_ThS = Concurrency::Queue<VideoSample>;

	class Call final
	{
	public:
		Call(const PeerLUID pluid, const Extender& extender, const Settings_ThS& settings, AVSource_ThS& avsource) noexcept :
			m_PeerLUID(pluid), m_Extender(extender), m_ExtenderSettings(settings), m_AVSource(avsource)
		{};

		~Call();

		[[nodiscard]] inline PeerLUID GetPeerLUID() const noexcept { return m_PeerLUID; }

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

		void SetSendVideo(const bool send) noexcept;
		[[nodiscard]] inline bool GetSendVideo() const noexcept { return GetSetting(CallSetting::SendVideo); }

		void SetSendAudio(const bool send) noexcept;
		[[nodiscard]] inline bool GetSendAudio() const noexcept { return GetSetting(CallSetting::SendAudio); }

		void OnAudioSourceChange() noexcept;
		void OnVideoSourceChange() noexcept;

		void OnAudioInSample(const AudioFormatData& fmt, const UInt64 timestamp, Buffer&& sample) noexcept;
		void OnVideoInSample(const VideoFormatData& fmt, const UInt64 timestamp, Buffer&& sample) noexcept;

	public:
		static constexpr std::chrono::seconds MaxWaitTimeForAccept{ 30 };

	private:
		[[nodiscard]] bool SetStatus(const CallStatus status) noexcept;

		inline void SetSetting(const CallSetting csetting, const bool state) noexcept
		{
			auto settings = m_Settings.load();

			if (state) settings |= static_cast<UInt8>(csetting);
			else settings &= ~static_cast<UInt8>(csetting);

			m_Settings.store(settings);
		}

		inline bool GetSetting(const CallSetting csetting) const noexcept
		{
			const auto settings = m_Settings.load();
			return (settings & static_cast<UInt8>(csetting));
		}

		void StartAVThreads() noexcept;
		void StopAVThreads() noexcept;

		static void AudioInWorkerThreadLoop(Call* call);
		static void AudioOutWorkerThreadLoop(Call* call);
		static void VideoInWorkerThreadLoop(Call* call);
		static void VideoOutWorkerThreadLoop(Call* call);

		void OnConnected();
		void OnDisconnected();

		void OnAudioOutSample(const UInt64 timestamp, IMFSample* sample);
		void OnVideoOutSample(const UInt64 timestamp, IMFSample* sample);

		void OpenVideoRenderer() noexcept;
		void CloseVideoRenderer() noexcept;
		void UpdateVideoRenderer() noexcept;

		void OpenAudioRenderer(const AudioFormat& fmt) noexcept;
		void CloseAudioRenderer() noexcept;

		void SetAudioCallbacks() noexcept;
		void SetVideoCallbacks() noexcept;
		void UnsetAVCallbacks() noexcept;

		template<typename T, typename U>
		[[nodiscard]] bool AddSampleToQueue(T&& sample, U& queue_ths) noexcept;

		template<typename T, typename U>
		[[nodiscard]] std::optional<T> GetSampleFromQueue(U& queue_ths) noexcept;

		[[nodiscard]] bool CopySample(const UInt64 timestamp, IMFSample* sample, Buffer& sample_buffer);

	private:
		const PeerLUID m_PeerLUID{ 0 };
		const Extender& m_Extender;
		const Settings_ThS& m_ExtenderSettings;
		AVSource_ThS& m_AVSource;

		CallType m_Type{ CallType::None };
		CallStatus m_Status{ CallStatus::Disconnected };
		SteadyTime m_LastActiveSteadyTime;
		SteadyTime m_StartSteadyTime;

		std::atomic<UInt8> m_Settings{ 0 };

		AudioRenderer_ThS m_AudioRenderer;
		VideoRenderer_ThS m_VideoRenderer;

		AVFormats_ThS m_AVInFormats;
		AudioSampleQueue_ThS m_AudioInQueue;
		VideoSampleQueue_ThS m_VideoInQueue;

		AudioSampleQueue_ThS m_AudioOutQueue;
		VideoSampleQueue_ThS m_VideoOutQueue;

		SampleEventHandles_ThS m_SampleEventHandles;

		Concurrency::Event m_DisconnectEvent;

		std::thread m_AudioInThread;
		std::thread m_AudioOutThread;
		std::thread m_VideoInThread;
		std::thread m_VideoOutThread;
	};

	using Call_ThS = Implementation::Concurrency::ThreadSafe<Call, std::shared_mutex>;
}