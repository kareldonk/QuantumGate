// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <QuantumGate.h>
#include <Concurrency\ThreadSafe.h>

#include <unordered_map>

namespace QuantumGate::AVExtender
{
	struct Peer final
	{
		PeerLUID ID{ 0 };
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

	private:
		const bool OnStartup();
		void OnPostStartup();
		void OnPreShutdown();
		void OnShutdown();
		void OnPeerEvent(PeerEvent&& event);
		const std::pair<bool, bool> OnPeerMessage(PeerEvent&& event);

	public:
		inline static constexpr ExtenderUUID UUID{ 0x10a86749, 0x7e9e, 0x297d, 0x1e1c3a7ddc723f66 };

	private:
		std::atomic_bool m_UseCompression{ true };

		HWND m_Window{ nullptr };
		Peers_ThS m_Peers;
	};
}