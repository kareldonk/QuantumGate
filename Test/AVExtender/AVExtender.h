// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Call.h"

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate;
	using namespace QuantumGate::Implementation;

	struct Peer final
	{
		Peer(const PeerLUID pluid) noexcept : ID(pluid) {}

		const PeerLUID ID{ 0 };
		std::shared_ptr<Call_ThS> Call;
	};

	using Peers = std::unordered_map<PeerLUID, std::unique_ptr<Peer>>;
	using Peers_ThS = Implementation::Concurrency::ThreadSafe<Peers, std::shared_mutex>;

	struct CallAccept final
	{
		CallAccept(const PeerLUID pluid) noexcept : PeerLUID(pluid) {}

		const PeerLUID PeerLUID{ 0 };
	};

	enum class WindowsMessage : UINT
	{
		PeerEvent = WM_USER + 1,
		ExtenderInit = WM_USER + 2,
		ExtenderDeinit = WM_USER + 3,
		AcceptIncomingCall = WM_USER + 4
	};

	struct Event final
	{
		PeerEventType Type{ PeerEventType::Unknown };
		PeerLUID PeerLUID{ 0 };
	};

	class Extender final : public QuantumGate::Extender
	{
		friend Call;

		struct PreviewEventHandlers
		{
			SourceReader::SampleEventDispatcher::FunctionHandle AudioSampleEventFunctionHandle;
			SourceReader::SampleEventDispatcher::FunctionHandle VideoSampleEventFunctionHandle;
		};

		using PreviewEventHandlers_ThS = Concurrency::ThreadSafe<PreviewEventHandlers, std::shared_mutex>;

	public:
		Extender(HWND hwnd);
		virtual ~Extender();

		void SetUseCompression(const bool compression) noexcept;
		[[nodiscard]] inline bool IsUsingCompression() const noexcept { return m_Settings->UseCompression; }

		void SetUseAudioCompression(const bool compression) noexcept;
		[[nodiscard]] inline bool IsUsingAudioCompression() const noexcept { return m_Settings->UseAudioCompression; }

		void SetUseVideoCompression(const bool compression) noexcept;
		[[nodiscard]] inline bool IsUsingVideoCompression() const noexcept { return m_Settings->UseVideoCompression; }

		void SetFillVideoScreen(const bool fill) noexcept;
		[[nodiscard]] inline bool GetFillVideoScreen() const noexcept { return m_Settings->FillVideoScreen; }

		[[nodiscard]] inline const Peers_ThS& GetPeers() const noexcept { return m_Peers; }

		[[nodiscard]] bool BeginCall(const PeerLUID pluid, const bool send_video, const bool send_audio) noexcept;
		[[nodiscard]] bool AcceptCall(const PeerLUID pluid) noexcept;
		[[nodiscard]] bool DeclineCall(const PeerLUID pluid) noexcept;
		[[nodiscard]] bool HangupCall(const PeerLUID pluid) noexcept;

		void HangupAllCalls() noexcept;

		void UpdateSendAudio(const PeerLUID pluid, const bool send_audio) noexcept;
		void UpdateSendVideo(const PeerLUID pluid, const bool send_video) noexcept;

		[[nodiscard]] bool SetAudioEndpointID(const WCHAR* id);
		[[nodiscard]] bool SetVideoSymbolicLink(const WCHAR* id, const UInt16 max_res);

		Result<VideoFormat> StartVideoPreview(SourceReader::SampleEventDispatcher::FunctionType&& callback) noexcept;
		void StopVideoPreview() noexcept;

		Result<AudioFormat> StartAudioPreview(SourceReader::SampleEventDispatcher::FunctionType&& callback) noexcept;
		void StopAudioPreview() noexcept;

		void StopAVSourceReaders() noexcept;

	protected:
		bool OnStartup();
		void OnPostStartup();
		void OnPreShutdown();
		void OnShutdown();
		void OnPeerEvent(PeerEvent&& event);
		const std::pair<bool, bool> OnPeerMessage(PeerEvent&& event);

		[[nodiscard]] bool SendCallAudioSample(const PeerLUID pluid, const AudioFormat& afmt, const UInt64 timestamp,
											   const BufferView data, const bool compressed) const noexcept;
		[[nodiscard]] bool SendCallVideoSample(const PeerLUID pluid, const VideoFormat& vfmt, const UInt64 timestamp,
											   const BufferView data, const bool compressed) const noexcept;
	private:
		static void WorkerThreadLoop(Extender* extender);

		template<typename Func>
		void IfGetCall(const PeerLUID pluid, Func&& func) noexcept(noexcept(func(std::declval<Call&>())));

		[[nodiscard]] bool HaveActiveCalls() const noexcept;

		std::shared_ptr<Call_ThS> GetCall(const PeerLUID pluid) const noexcept;

		[[nodiscard]] bool HangupCall(std::shared_ptr<Call_ThS>& call_ths) noexcept;

		void StopAllCalls() noexcept;

		[[nodiscard]] bool SendSimpleMessage(const PeerLUID pluid, const MessageType type,
											 const SendParameters::PriorityOption priority, const BufferView data = {}) const noexcept;
		[[nodiscard]] bool SendCallRequest(const PeerLUID pluid) const noexcept;
		[[nodiscard]] bool SendCallAccept(const PeerLUID pluid) const noexcept;
		[[nodiscard]] bool SendCallHangup(const PeerLUID pluid) const noexcept;
		[[nodiscard]] bool SendCallDecline(const PeerLUID pluid) const noexcept;
		[[nodiscard]] bool SendGeneralFailure(const PeerLUID pluid) const noexcept;

		bool StartAudioSourceReader() noexcept;
		bool StartAudioSourceReader(AVSource& avsource) noexcept;
		void StopAudioSourceReader() noexcept;
		void StopAudioSourceReader(AVSource& avsource) noexcept;

		bool StartVideoSourceReader() noexcept;
		bool StartVideoSourceReader(AVSource& avsource) noexcept;
		void StopVideoSourceReader() noexcept;
		void StopVideoSourceReader(AVSource& avsource) noexcept;

	public:
		inline static constexpr ExtenderUUID UUID{ 0x10a86749, 0x7e9e, 0x297d, 0x1e1c3a7ddc723f66 };

	private:
		const HWND m_Window{ nullptr };
		Settings_ThS m_Settings;
		Peers_ThS m_Peers;

		AVSource_ThS m_AVSource;

		Concurrency::EventCondition m_ShutdownEvent{ false };
		std::thread m_Thread;

		PreviewEventHandlers_ThS m_PreviewEventHandlers;
	};
}