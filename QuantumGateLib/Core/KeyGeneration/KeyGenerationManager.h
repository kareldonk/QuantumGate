// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "KeyGenerationEvent.h"
#include "..\..\Concurrency\ThreadPool.h"
#include "..\..\Concurrency\Queue.h"

namespace QuantumGate::Implementation::Core::KeyGeneration
{
	class Manager final
	{
		using KeyQueueMap = std::unordered_map<Algorithm::Asymmetric, std::unique_ptr<KeyQueue_ThS>>;
		using KeyQueueMap_ThS = Concurrency::ThreadSafe<KeyQueueMap, Concurrency::SharedSpinMutex>;

		using EventQueue = Concurrency::Queue<Event>;
		using EventQueue_ThS = Concurrency::ThreadSafe<EventQueue, Concurrency::SpinMutex>;

		struct ThreadPoolData
		{
			EventQueue_ThS KeyGenEventQueue;
			Concurrency::EventCondition PrimaryThreadEvent;
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData>;

	public:
		Manager(const Settings_CThS& settings) noexcept;
		Manager(const Manager&) = delete;
		Manager(Manager&&) = default;
		~Manager() = default;
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = default;

		const Settings& GetSettings() const noexcept;

		const bool Startup() noexcept;
		void Shutdown() noexcept;

		inline const bool IsRunning() const noexcept { return m_Running; }

		std::optional<Crypto::AsymmetricKeyData> GetAsymmetricKeys(const Algorithm::Asymmetric alg) noexcept;

	private:
		void PreStartup() noexcept;
		void ResetState() noexcept;

		const bool AddKeyQueues() noexcept;
		void ClearKeyQueues() noexcept;

		const bool StartupThreadPool() noexcept;
		void ShutdownThreadPool() noexcept;

		const std::pair<bool, bool> PrimaryThreadProcessor(ThreadPoolData& thpdata,
														   const Concurrency::EventCondition& shutdown_event);

		const std::pair<bool, bool> WorkerThreadProcessor(ThreadPoolData& thpdata,
														  const Concurrency::EventCondition& shutdown_event);

	private:
		std::atomic_bool m_Running{ false };
		const Settings_CThS& m_Settings;

		KeyQueueMap_ThS m_KeyQueues;
		ThreadPool m_ThreadPool;
	};
}