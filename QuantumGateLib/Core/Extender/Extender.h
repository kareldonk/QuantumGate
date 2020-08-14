// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Message.h"
#include "..\..\API\Extender.h"

namespace QuantumGate::Implementation::Core
{
	class Local;
}

namespace QuantumGate::Implementation::Core::Extender
{
	class Extender final
	{
		using StartupCallback = QuantumGate::API::Extender::StartupCallback;
		using PostStartupCallback = QuantumGate::API::Extender::PostStartupCallback;
		using PreShutdownCallback = QuantumGate::API::Extender::PreShutdownCallback;
		using ShutdownCallback = QuantumGate::API::Extender::ShutdownCallback;
		using PeerEventCallback = QuantumGate::API::Extender::PeerEventCallback;
		using PeerMessageCallback = QuantumGate::API::Extender::PeerMessageCallback;

	public:
		Extender() = delete;
		Extender(const ExtenderUUID& uuid, const String& name) noexcept;
		Extender(const Extender&) = delete;
		Extender(Extender&&) noexcept = default;
		~Extender() {};
		Extender& operator=(const Extender&) = delete;
		Extender& operator=(Extender&&) noexcept = default;

		inline const ExtenderUUID& GetUUID() const noexcept { return m_UUID; }
		inline const String& GetName() const noexcept { return m_Name; }

		inline bool IsRunning() const noexcept { return m_Running; }
		inline bool HadException() const noexcept { return m_Exception; }

		Result<std::tuple<UInt, UInt, UInt, UInt>> GetLocalVersion() const noexcept;
		Result<std::pair<UInt, UInt>> GetLocalProtocolVersion() const noexcept;
		Result<PeerUUID> GetLocalUUID() const noexcept;

		Result<API::Peer> ConnectTo(ConnectParameters&& params) noexcept;
		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params,
													ConnectCallback&& function) noexcept;

		Result<> DisconnectFrom(const PeerLUID pluid) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept;
		Result<> DisconnectFrom(API::Peer& peer) noexcept;
		Result<> DisconnectFrom(API::Peer& peer, DisconnectCallback&& function) noexcept;

		Result<Size> SendMessage(const PeerLUID pluid, const BufferView& buffer,
								 const SendParameters& params, SendCallback&& callback) const noexcept;
		Result<Size> SendMessage(API::Peer& peer, const BufferView& buffer,
								 const SendParameters& params, SendCallback&& callback) const noexcept;

		Result<> SendMessageTo(const PeerLUID pluid, Buffer&& buffer,
							   const SendParameters& params, SendCallback&& callback) const noexcept;
		Result<> SendMessageTo(API::Peer& peer, Buffer&& buffer,
							   const SendParameters& params, SendCallback&& callback) const noexcept;

		inline static Size GetMaximumMessageDataSize() noexcept { return Message::MaxMessageDataSize; }

		Result<API::Peer> GetPeer(const PeerLUID pluid) const noexcept;

		Result<Vector<PeerLUID>> QueryPeers(const PeerQueryParameters& params) const noexcept;
		Result<> QueryPeers(const PeerQueryParameters& params, Vector<PeerLUID>& pluids) const noexcept;

		inline void SetLocal(Local* local) noexcept { assert(local != nullptr); m_Local = local; }
		inline void ResetLocal() noexcept { m_Local = nullptr; }

		inline Result<> SetStartupCallback(StartupCallback&& function) noexcept
		{
			return SetCallback(m_StartupCallback, std::move(function));
		}

		inline Result<> SetPostStartupCallback(PostStartupCallback&& function) noexcept
		{
			return SetCallback(m_PostStartupCallback, std::move(function));
		}

		inline Result<> SetPreShutdownCallback(PreShutdownCallback&& function) noexcept
		{
			return SetCallback(m_PreShutdownCallback, std::move(function));
		}

		inline Result<> SetShutdownCallback(ShutdownCallback&& function) noexcept
		{
			return SetCallback(m_ShutdownCallback, std::move(function));
		}

		inline Result<> SetPeerEventCallback(PeerEventCallback&& function) noexcept
		{
			return SetCallback(m_PeerEventCallback, std::move(function));
		}

		inline Result<> SetPeerMessageCallback(PeerMessageCallback&& function) noexcept
		{
			return SetCallback(m_PeerMessageCallback, std::move(function));
		}

		[[nodiscard]] inline bool OnBeginStartup() noexcept
		{
			m_Exception = false;
			
			try { return m_StartupCallback(); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }

			return false;
		}

		inline void OnEndStartup() noexcept
		{
			m_Running = true;

			try { m_PostStartupCallback(); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }
		}

		inline void OnReady() noexcept
		{
			m_Ready = true;
		}

		inline void OnBeginShutdown() noexcept
		{
			try { m_PreShutdownCallback(); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }

			m_Ready = false;
			m_Running = false;
		}

		inline void OnEndShutdown() noexcept
		{
			try { m_ShutdownCallback(); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }
		}

		inline void OnPeerEvent(QuantumGate::API::Extender::PeerEvent&& event) noexcept
		{
			try { m_PeerEventCallback(std::move(event)); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }
		}

		[[nodiscard]] inline QuantumGate::API::Extender::PeerEvent::Result
			OnPeerMessage(QuantumGate::API::Extender::PeerEvent&& event) noexcept
		{
			try { return m_PeerMessageCallback(std::move(event)); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }

			return {};
		}

	private:
		void OnException() noexcept;
		void OnException(const std::exception& e) noexcept;

		template<typename T>
		inline Result<> SetCallback(T& var, T&& function) noexcept
		{
			assert(!IsRunning() && function);

			if (!IsRunning())
			{
				if (function)
				{
					var = std::move(function);
					return ResultCode::Succeeded;
				}
				else return ResultCode::InvalidArgument;
			}

			return ResultCode::Failed;
		}

	private:
		std::atomic<Core::Local*> m_Local{ nullptr };
		std::atomic_bool m_Running{ false };
		std::atomic_bool m_Ready{ false };
		std::atomic_bool m_Exception{ false };
		const ExtenderUUID m_UUID;
		const String m_Name{ L"Unknown" };

		StartupCallback m_StartupCallback{ []() mutable -> bool { return true; } };
		PostStartupCallback m_PostStartupCallback{ []() mutable {} };
		PreShutdownCallback m_PreShutdownCallback{ []() mutable {} };
		ShutdownCallback m_ShutdownCallback{ []() mutable {} };
		PeerEventCallback m_PeerEventCallback{ [](QuantumGate::API::Extender::PeerEvent&&) mutable {} };
		PeerMessageCallback m_PeerMessageCallback
		{ [](QuantumGate::API::Extender::PeerEvent&&) mutable -> QuantumGate::API::Extender::PeerEvent::Result { return {}; } };
	};
}