// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <QuantumGate.h>

namespace QuantumGate::AVExtender
{
	class Extender final : public QuantumGate::Extender
	{
	public:
		Extender() noexcept;
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
		inline static ExtenderUUID UUID{ L"10a86749-7e9e-297d-1e1c-3a7ddc723f66" };

	private:
		std::atomic_bool m_UseCompression{ true };
	};
}