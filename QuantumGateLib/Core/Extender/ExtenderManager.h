// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\Dispatcher.h"
#include "..\..\Concurrency\ThreadLocalCache.h"
#include "..\Peer\PeerExtenderUUIDs.h"
#include "ExtenderControl.h"

#include <unordered_map> 

namespace QuantumGate::Implementation::Core::Extender
{
	struct ActiveExtenderUUIDs
	{
		Vector<ExtenderUUID> UUIDs;
		Vector<SerializedUUID> SerializedUUIDs;
	};

	using CachedActiveExtenderUUIDs_ThS = Concurrency::ThreadLocalCache<ActiveExtenderUUIDs, Concurrency::SpinMutex, 369>;

	class Manager final
	{
		friend class Control;

		using ExtenderMap = std::unordered_map<ExtenderUUID, std::unique_ptr<Control_ThS>>;
		using ExtenderMap_ThS = Concurrency::ThreadSafe<ExtenderMap, std::shared_mutex>;

	public:
		using ExtenderUpdateCallbacks = Dispatcher<void(const Vector<ExtenderUUID>&, const bool)>;
		using ExtenderUpdateCallbackHandle = ExtenderUpdateCallbacks::FunctionHandle;
		using ExtenderUpdateCallbacks_ThS = Concurrency::ThreadSafe<ExtenderUpdateCallbacks, std::shared_mutex>;

		using UnhandledExtenderMessageCallbacks = Dispatcher<void(const ExtenderUUID&, const PeerLUID,
																  const std::pair<bool, bool>&) noexcept>;
		using UnhandledExtenderMessageCallbackHandle = UnhandledExtenderMessageCallbacks::FunctionHandle;
		using UnhandledExtenderMessageCallbacks_ThS = Concurrency::ThreadSafe<UnhandledExtenderMessageCallbacks, std::shared_mutex>;

		Manager() = delete;
		Manager(const Settings_CThS& settings) noexcept;
		Manager(const Manager&) = delete;
		Manager(Manager&&) = delete;
		virtual ~Manager() = default;
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = delete;

		[[nodiscard]] const bool Startup();
		void Shutdown();
		inline const bool IsRunning() const noexcept { return m_Running; }

		Result<bool> AddExtender(const std::shared_ptr<QuantumGate::API::Extender>& extender,
								 const ExtenderModuleID moduleid) noexcept;
		Result<> RemoveExtender(const std::shared_ptr<QuantumGate::API::Extender>& extender,
								const ExtenderModuleID moduleid) noexcept;

		Result<> StartExtender(const ExtenderUUID& extuuid) noexcept;
		Result<> ShutdownExtender(const ExtenderUUID& extuuid) noexcept;

		const bool HasExtender(const ExtenderUUID& extuuid) const noexcept;
		std::weak_ptr<QuantumGate::API::Extender> GetExtender(const ExtenderUUID& extuuid) const noexcept;

		void OnPeerEvent(const Vector<ExtenderUUID>& extuuids, Peer::Event&& event) noexcept;
		const std::pair<bool, bool> OnPeerMessage(Peer::Event&& event) noexcept;

		const ActiveExtenderUUIDs& GetActiveExtenderUUIDs() const noexcept;

		inline ExtenderUpdateCallbacks_ThS& GetExtenderUpdateCallbacks() noexcept { return m_ExtenderUpdateCallbacks; }

		inline UnhandledExtenderMessageCallbacks_ThS& GetUnhandledExtenderMessageCallbacks() noexcept
		{
			return m_UnhandledExtenderMessageCallbacks;
		}

	private:
		[[nodiscard]] const bool StartExtenders() noexcept;
		void ShutdownExtenders() noexcept;

		Result<Control_ThS*> GetExtenderControl(const std::shared_ptr<QuantumGate::API::Extender>& extender,
												const std::optional<ExtenderModuleID> moduleid = std::nullopt) const noexcept;

		[[nodiscard]] const bool StartExtender(Control_ThS& extctrl_ths, const bool update_active);
		[[nodiscard]] const bool ShutdownExtender(Control_ThS& extctrl_ths, const bool update_active);

		void UpdateActiveExtenderUUIDs(const ExtenderMap& extenders) noexcept;

		const Settings& GetSettings() const noexcept;

	public:
		static constexpr Size MaximumNumberOfExtenders{ 4096 };

	private:
		std::atomic_bool m_Running{ false };
		const Settings_CThS& m_Settings;

		ExtenderMap_ThS m_Extenders;
		CachedActiveExtenderUUIDs_ThS m_ActiveExtenderUUIDs;

		ExtenderUpdateCallbacks_ThS m_ExtenderUpdateCallbacks;
		UnhandledExtenderMessageCallbacks_ThS m_UnhandledExtenderMessageCallbacks;
	};
}
