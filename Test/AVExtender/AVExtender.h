// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

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
		CallRequest
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
		Cancelled,
		Error,
		Connected
	};

	using CallID = UInt64;

	class Call final
	{
	public:
		Call() noexcept {};
		~Call() {};

		void SetStatus(const CallStatus status) noexcept;
		inline const CallStatus GetStatus() const noexcept { return m_Status; }
		const WChar* GetStatusString() const noexcept;

		const bool IsInCall() const noexcept;

		inline void SetType(const CallType type) noexcept { m_Type = type; }
		inline CallType GetType() const noexcept { return m_Type; }
		inline CallID GetID() const noexcept { return m_ID; }

		inline SteadyTime GetLastActiveSteadyTime() const noexcept { return m_LastActiveSteadyTime; }
		inline SteadyTime GetStartSteadyTime() const noexcept { return m_StartSteadyTime; }
		std::chrono::milliseconds GetDuration() const noexcept;

		inline void SetSendVideo(const bool send) noexcept { m_SendVideo = send; }
		inline const bool GetSendVideo() const noexcept { return m_SendVideo; }

		inline void SetSendAudio(const bool send) noexcept { m_SendAudio = send; }
		inline const bool GetSendAudio() const noexcept { return m_SendAudio; }

	private:
		CallType m_Type{ CallType::None };
		CallStatus m_Status{ CallStatus::Disconnected };
		CallID m_ID{ 0 };
		SteadyTime m_LastActiveSteadyTime;
		SteadyTime m_StartSteadyTime;
		bool m_SendVideo{ true };
		bool m_SendAudio{ true };
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

	enum class WindowsMessage : UINT
	{
		PeerEvent = WM_USER + 1,
		ExtenderInit = WM_USER + 2,
		ExtenderDeinit = WM_USER + 3
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
		inline const bool IsUsingCompression() const noexcept { return m_UseCompression; }

		inline const Peers_ThS& GetPeers() const noexcept { return m_Peers; }

		const bool BeginCall(const PeerLUID pluid) noexcept;

	protected:
		const bool OnStartup();
		void OnPostStartup();
		void OnPreShutdown();
		void OnShutdown();
		void OnPeerEvent(PeerEvent&& event);
		const std::pair<bool, bool> OnPeerMessage(PeerEvent&& event);

	private:
		static void WorkerThreadLoop(Extender* extender);

		template<typename Func>
		const bool IfHasCall(const PeerLUID pluid, const CallID id, Func&& func);

		template<typename Func>
		const bool IfNotHasCall(const PeerLUID pluid, Func&& func);

		const bool SendCallRequest(const PeerLUID pluid, Call& call);

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