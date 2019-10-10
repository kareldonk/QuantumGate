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

	public:
		Extender(HWND hwnd);
		virtual ~Extender();

		inline void SetUseCompression(const bool compression) noexcept { m_UseCompression = compression; }
		[[nodiscard]] inline bool IsUsingCompression() const noexcept { return m_UseCompression; }

		[[nodiscard]] inline const Peers_ThS& GetPeers() const noexcept { return m_Peers; }

		[[nodiscard]] bool BeginCall(const PeerLUID pluid, const bool send_video, const bool send_audio) noexcept;
		[[nodiscard]] bool AcceptCall(const PeerLUID pluid) noexcept;
		[[nodiscard]] bool DeclineCall(const PeerLUID pluid) noexcept;
		[[nodiscard]] bool HangupCall(const PeerLUID pluid) noexcept;

		void HangupAllCalls() noexcept;

		void UpdateSendAudioVideo(const PeerLUID pluid, const bool send_video, const bool send_audio);

		[[nodiscard]] bool SetAudioEndpointID(const WCHAR* id);
		[[nodiscard]] bool SetVideoSymbolicLink(const WCHAR* id, const Size max_res);

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

		[[nodiscard]] bool SendCallAVSample(const PeerLUID pluid, const MessageType type, const UInt64 timestamp,
											const BufferView data);

	private:
		static void WorkerThreadLoop(Extender* extender);

		[[nodiscard]] CallAVFormatData GetCallAVFormatData(const bool send_audio, const bool send_video);

		template<typename Func>
		void IfGetCall(const PeerLUID pluid, Func&& func) noexcept(noexcept(func(std::declval<Call&>())));

		[[nodiscard]] bool HaveActiveCalls() const noexcept;

		std::shared_ptr<Call_ThS> GetCall(const PeerLUID pluid) const noexcept;

		[[nodiscard]] bool HangupCall(std::shared_ptr<Call_ThS>& call_ths) noexcept;
		
		void StopAllCalls() noexcept;

		[[nodiscard]] bool SendSimpleMessage(const PeerLUID pluid, const MessageType type, const BufferView data = {});
		[[nodiscard]] bool SendCallRequest(const PeerLUID pluid, const bool send_audio, const bool send_video);
		[[nodiscard]] bool SendCallAccept(const PeerLUID pluid, const bool send_audio, const bool send_video);
		[[nodiscard]] bool SendCallHangup(const PeerLUID pluid);
		[[nodiscard]] bool SendCallDecline(const PeerLUID pluid);
		[[nodiscard]] bool SendGeneralFailure(const PeerLUID pluid);
		[[nodiscard]] bool SendCallAVUpdate(const PeerLUID pluid, const bool send_audio, const bool send_video);

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
		std::atomic_bool m_UseCompression{ true };

		const HWND m_Window{ nullptr };
		Peers_ThS m_Peers;

		Concurrency::EventCondition m_ShutdownEvent{ false };
		std::thread m_Thread;

		SourceReader::SampleEventDispatcher::FunctionHandle m_PreviewAudioSampleEventFunctionHandle;
		SourceReader::SampleEventDispatcher::FunctionHandle m_PreviewVideoSampleEventFunctionHandle;

		AVSource_ThS m_AVSource;
	};
}