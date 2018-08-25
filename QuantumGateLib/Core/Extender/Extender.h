// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Message.h"

namespace QuantumGate::Implementation::Core
{
	class Local;
}

namespace QuantumGate::Implementation::Core::Extender
{
	class Extender
	{
	public:
		Extender() = delete;
		Extender(const ExtenderUUID& uuid, const String& name) noexcept;
		Extender(const Extender&) = delete;
		Extender(Extender&&) = default;
		virtual ~Extender() {};
		Extender& operator=(const Extender&) = delete;
		Extender& operator=(Extender&&) = default;

		inline const ExtenderUUID& GetUUID() const noexcept { return m_UUID; }
		inline const String& GetName() const noexcept { return m_Name; }

		inline const bool IsRunning() const noexcept { return m_Running; }
		inline const bool HadException() const noexcept { return m_Exception; }

		Result<std::tuple<UInt, UInt, UInt, UInt>> GetLocalVersion() const noexcept;
		Result<std::pair<UInt, UInt>> GetLocalProtocolVersion() const noexcept;
		Result<PeerUUID> GetLocalUUID() const noexcept;

		Result<ConnectDetails> ConnectTo(ConnectParameters&& params) noexcept;
		Result<std::pair<PeerLUID, bool>> ConnectTo(ConnectParameters&& params,
													ConnectCallback&& function) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid) noexcept;
		Result<> DisconnectFrom(const PeerLUID pluid, DisconnectCallback&& function) noexcept;

		Result<> SendMessageTo(const PeerLUID pluid, Buffer&& buffer, const bool compress) const;

		inline const Size GetMaximumMessageDataSize() const noexcept { return Message::MaxMessageDataSize; }

		Result<PeerDetails> GetPeerDetails(const PeerLUID pluid) const noexcept;
		Result<std::vector<PeerLUID>> QueryPeers(const PeerQueryParameters& settings) const noexcept;

		inline void SetLocal(Local* local) noexcept { assert(local != nullptr); m_Local = local; }
		inline void ResetLocal() noexcept { m_Local = nullptr; }

		inline const Result<> SetStartupCallback(ExtenderStartupCallback&& function) noexcept
		{
			return SetCallback(m_StartupCallback, std::move(function));
		}

		inline const Result<> SetPostStartupCallback(ExtenderPostStartupCallback&& function) noexcept
		{
			return SetCallback(m_PostStartupCallback, std::move(function));
		}

		inline const Result<> SetPreShutdownCallback(ExtenderPreShutdownCallback&& function) noexcept
		{
			return SetCallback(m_PreShutdownCallback, std::move(function));
		}

		inline const Result<> SetShutdownCallback(ExtenderShutdownCallback&& function) noexcept
		{
			return SetCallback(m_ShutdownCallback, std::move(function));
		}

		inline const Result<> SetPeerEventCallback(ExtenderPeerEventCallback&& function) noexcept
		{
			return SetCallback(m_PeerEventCallback, std::move(function));
		}

		inline const Result<> SetPeerMessageCallback(ExtenderPeerMessageCallback&& function) noexcept
		{
			return SetCallback(m_PeerMessageCallback, std::move(function));
		}

		[[nodiscard]] inline const bool OnBeginStartup() noexcept
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

		inline void OnBeginShutdown() noexcept
		{
			try { m_PreShutdownCallback(); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }

			m_Running = false;
		}

		inline void OnEndShutdown() noexcept
		{
			try { m_ShutdownCallback(); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }
		}

		inline void OnPeerEvent(QuantumGate::API::PeerEvent&& event) noexcept
		{
			try { m_PeerEventCallback(std::move(event)); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }
		}

		[[nodiscard]] inline const std::pair<bool, bool> OnPeerMessage(QuantumGate::API::PeerEvent&& event) noexcept
		{
			try { return m_PeerMessageCallback(std::move(event)); }
			catch (const std::exception& e) { OnException(e); }
			catch (...) { OnException(); }

			return std::make_pair(false, false);
		}

	private:
		void OnException() noexcept;
		void OnException(const std::exception& e) noexcept;

		template<typename T>
		inline const Result<> SetCallback(T& var, T&& function) noexcept
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
		std::atomic_bool m_Exception{ false };
		const ExtenderUUID m_UUID;
		const String m_Name{ L"Unknown" };

		ExtenderStartupCallback m_StartupCallback{ []() -> const bool { return true; } };
		ExtenderPostStartupCallback m_PostStartupCallback{ [] {} };
		ExtenderPreShutdownCallback m_PreShutdownCallback{ [] {} };
		ExtenderShutdownCallback m_ShutdownCallback{ [] {} };
		ExtenderPeerEventCallback m_PeerEventCallback{ [](QuantumGate::API::PeerEvent&&) {} };
		ExtenderPeerMessageCallback m_PeerMessageCallback
		{ [](QuantumGate::API::PeerEvent&&) -> const std::pair<bool, bool> { return std::make_pair(false, false); } };
	};
}