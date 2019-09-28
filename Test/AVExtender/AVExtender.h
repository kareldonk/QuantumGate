// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Protocol.h"
#include "Call.h"
#include "AudioSourceReader.h"

#include <Concurrency\EventCondition.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate;
	using namespace QuantumGate::Implementation;

	struct Peer final
	{
		Peer(const PeerLUID pluid) noexcept : ID(pluid) {}

		const PeerLUID ID{ 0 };
		Call_ThS Call;
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
		struct AudioSource
		{
			AudioSourceReader AudioSourceReader;
			String AudioEndpointID;
		};

		using AudioSource_ThS = Implementation::Concurrency::ThreadSafe<AudioSource, std::shared_mutex>;

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

		void UpdateSendAudioVideo(const PeerLUID pluid, const bool send_video, const bool send_audio);

		inline void SetAudioEndpointID(const WCHAR* id) { m_AudioSource.WithUniqueLock()->AudioEndpointID = id; }

	protected:
		bool OnStartup();
		void OnPostStartup();
		void OnPreShutdown();
		void OnShutdown();
		void OnPeerEvent(PeerEvent&& event);
		const std::pair<bool, bool> OnPeerMessage(PeerEvent&& event);

	private:
		static void WorkerThreadLoop(Extender* extender);

		template<typename Func>
		void IfGetCall(const PeerLUID pluid, Func&& func) noexcept(noexcept(func(std::declval<Call&>())));

		[[nodiscard]] bool SendSimpleMessage(const PeerLUID pluid, const MessageType type, const BufferView& data = {});
		[[nodiscard]] bool SendCallRequest(const PeerLUID pluid, const Call& call);
		[[nodiscard]] bool SendCallAccept(const PeerLUID pluid, const Call& call);
		[[nodiscard]] bool SendCallHangup(const PeerLUID pluid);
		[[nodiscard]] bool SendGeneralFailure(const PeerLUID pluid);
		[[nodiscard]] bool SendCallAVUpdate(const PeerLUID pluid, const Call& call);
		[[nodiscard]] bool SendCallAVSample(const PeerLUID pluid, const MessageType type, const UInt64 timestamp,
											const BufferView& data);

		void StartAudioSourceReader() noexcept;
		void StopAudioSourceReader() noexcept;

		void OnAudioSample(const UInt64 timestamp, IMFSample* sample);

	public:
		inline static constexpr ExtenderUUID UUID{ 0x10a86749, 0x7e9e, 0x297d, 0x1e1c3a7ddc723f66 };

	private:
		std::atomic_bool m_UseCompression{ true };

		const HWND m_Window{ nullptr };
		Peers_ThS m_Peers;

		Concurrency::EventCondition m_ShutdownEvent{ false };
		std::thread m_Thread;

		AudioSource_ThS m_AudioSource;
	};
}