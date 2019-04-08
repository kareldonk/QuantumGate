// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "VideoWindow.h"

#include <QuantumGate.h>
#include <Concurrency\ThreadSafe.h>
#include <Concurrency\EventCondition.h>

namespace QuantumGate::AVExtender
{
	using namespace QuantumGate;
	using namespace QuantumGate::Implementation;

	enum class MessageType : UInt16
	{
		Unknown = 0,
		CallRequest,
		CallAccept,
		CallHangup,
		GeneralFailure
	};

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

		inline void SetType(const CallType type) noexcept { m_Type = type; }
		[[nodiscard]] inline CallType GetType() const noexcept { return m_Type; }

		[[nodiscard]] inline SteadyTime GetLastActiveSteadyTime() const noexcept { return m_LastActiveSteadyTime; }
		[[nodiscard]] inline SteadyTime GetStartSteadyTime() const noexcept { return m_StartSteadyTime; }
		[[nodiscard]] std::chrono::milliseconds GetDuration() const noexcept;

		inline void SetSendVideo(const bool send) noexcept { m_SendVideo = send; }
		[[nodiscard]] inline bool GetSendVideo() const noexcept { return m_SendVideo; }

		inline void SetSendAudio(const bool send) noexcept { m_SendAudio = send; }
		[[nodiscard]] inline bool GetSendAudio() const noexcept { return m_SendAudio; }

	public:
		static constexpr std::chrono::seconds MaxWaitTimeForAccept{ 30 };

	private:
		CallType m_Type{ CallType::None };
		CallStatus m_Status{ CallStatus::Disconnected };
		SteadyTime m_LastActiveSteadyTime;
		SteadyTime m_StartSteadyTime;
		bool m_SendVideo{ true };
		bool m_SendAudio{ true };
		VideoWindow m_VideoWindow;
	};

	using Call_ThS = Implementation::Concurrency::ThreadSafe<Call, std::shared_mutex>;

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
	public:
		Extender(HWND hwnd);
		virtual ~Extender();

		inline void SetUseCompression(const bool compression) noexcept { m_UseCompression = compression; }
		[[nodiscard]] inline bool IsUsingCompression() const noexcept { return m_UseCompression; }

		[[nodiscard]] inline const Peers_ThS& GetPeers() const noexcept { return m_Peers; }

		[[nodiscard]] bool BeginCall(const PeerLUID pluid) noexcept;
		[[nodiscard]] bool AcceptCall(const PeerLUID pluid) noexcept;
		[[nodiscard]] bool DeclineCall(const PeerLUID pluid) noexcept;
		[[nodiscard]] bool HangupCall(const PeerLUID pluid) noexcept;

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

		[[nodiscard]] bool SendSimpleMessage(const PeerLUID pluid, const MessageType type);
		[[nodiscard]] bool SendCallRequest(const PeerLUID pluid);
		[[nodiscard]] bool SendCallAccept(const PeerLUID pluid);
		[[nodiscard]] bool SendCallHangup(const PeerLUID pluid);
		[[nodiscard]] bool SendGeneralFailure(const PeerLUID pluid);

	public:
		inline static constexpr ExtenderUUID UUID{ 0x10a86749, 0x7e9e, 0x297d, 0x1e1c3a7ddc723f66 };

	private:
		std::atomic_bool m_UseCompression{ true };

		HWND m_Window{ nullptr };
		Peers_ThS m_Peers;

		Concurrency::EventCondition m_ShutdownEvent{ false };
		std::thread m_Thread;
	};
}