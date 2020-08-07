// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "KeyGenerationManager.h"
#include "..\..\Crypto\Crypto.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::KeyGeneration
{
	Manager::Manager(const Settings_CThS& settings) noexcept :
		m_Settings(settings)
	{}

	const Settings& Manager::GetSettings() const noexcept
	{
		return m_Settings.GetCache();
	}

	bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"Keymanager starting...");

		PreStartup();

		if (!AddKeyQueues() || !StartupThreadPool())
		{
			ShutdownThreadPool();
			ClearKeyQueues();

			LogErr(L"Keymanager startup failed");

			return false;
		}

		m_Running = true;

		LogSys(L"Keymanager startup successful");

		// Set event so that initial keys get generated
		m_ThreadPool.GetData().PrimaryThreadEvent.Set();

		return true;
	}

	void Manager::Shutdown() noexcept
	{
		if (!m_Running) return;

		m_Running = false;

		LogSys(L"Keymanager shutting down...");

		ShutdownThreadPool();

		ResetState();

		LogSys(L"Keymanager shut down");
	}

	void Manager::PreStartup() noexcept
	{
		ResetState();
	}

	void Manager::ResetState() noexcept
	{
		// Events need to be cleared first because
		// they contain pointers to the key queues
		m_ThreadPool.GetData().KeyGenEventQueue.Clear();

		ClearKeyQueues();
	}

	bool Manager::AddKeyQueues() noexcept
	{
		bool success = true;

		try
		{
			const auto& settings = GetSettings();

			const auto& algs1 = settings.Local.SupportedAlgorithms.PrimaryAsymmetric;
			const auto& algs2 = settings.Local.SupportedAlgorithms.SecondaryAsymmetric;

			Vector<Algorithm::Asymmetric> dstalgs;
			std::set_union(algs1.begin(), algs1.end(),
						   algs2.begin(), algs2.end(),
						   std::back_inserter(dstalgs));

			m_KeyQueues.WithUniqueLock([&](KeyQueueMap& queues)
			{
				for (const auto alg : dstalgs)
				{
					LogDbg(L"Keymanager adding key queue for algorithm %s", Crypto::GetAlgorithmName(alg));

					[[maybe_unused]] const auto [it, inserted] =
						queues.insert({ alg, std::make_unique<KeyQueue_ThS>(alg) });

					assert(inserted);
					if (!inserted)
					{
						LogErr(L"Keymanager couldn't add key queue for algorithm %s", Crypto::GetAlgorithmName(alg));
						success = false;
						return;
					}
				}
			});
		}
		catch (const std::exception& e)
		{
			LogErr(L"Exception while adding key queues for Keymanager - %s",
				   Util::ToStringW(e.what()).c_str());
			success = false;
		}

		return success;
	}

	void Manager::ClearKeyQueues() noexcept
	{
		m_KeyQueues.WithUniqueLock()->clear();
	}

	bool Manager::StartupThreadPool() noexcept
	{
		const auto& settings = GetSettings();

		const auto numthreadsperpool = Util::GetNumThreadsPerPool(settings.Local.Concurrency.KeyGenerationManager.MinThreads,
																  settings.Local.Concurrency.KeyGenerationManager.MaxThreads, 2u);

		// Must have at least two threads in pool 
		// one of which will be the primary thread
		assert(numthreadsperpool > 1);

		LogSys(L"Creating key generation threadpool with %zu worker %s",
			   numthreadsperpool, numthreadsperpool > 1 ? L"threads" : L"thread");

		auto error = false;

		// Create the worker threads
		for (Size x = 0; x < numthreadsperpool; ++x)
		{
			// First thread is primary worker thread
			if (x == 0)
			{
				if (!m_ThreadPool.AddThread(L"QuantumGate KeyManager Thread (Main)",
											MakeCallback(this, &Manager::PrimaryThreadProcessor),
											MakeCallback(this, &Manager::PrimaryThreadWait),
											MakeCallback(this, &Manager::PrimaryThreadWaitInterrupt)))
				{
					error = true;
					break;
				}
			}
			else
			{
				if (!m_ThreadPool.AddThread(L"QuantumGate KeyManager Thread",
											MakeCallback(this, &Manager::WorkerThreadProcessor),
											MakeCallback(this, &Manager::WorkerThreadWait),
											MakeCallback(this, &Manager::WorkerThreadWaitInterrupt)))
				{
					error = true;
					break;
				}
			}
		}

		if (!error && m_ThreadPool.Startup())
		{
			return true;
		}

		return false;
	}

	void Manager::ShutdownThreadPool() noexcept
	{
		m_ThreadPool.Shutdown();
		m_ThreadPool.Clear();
	}

	std::optional<Crypto::AsymmetricKeyData> Manager::GetAsymmetricKeys(const Algorithm::Asymmetric alg) noexcept
	{
		if (!IsRunning()) return std::nullopt;

		std::optional<Crypto::AsymmetricKeyData> keydata;

		m_KeyQueues.WithSharedLock([&](const KeyQueueMap& key_queues)
		{
			// Find the keypair queue for the algorithm
			if (const auto it = key_queues.find(alg); it != key_queues.end())
			{
				it->second->WithUniqueLock([&](KeyQueue& key_queue)
				{
					if (!key_queue.Queue.empty())
					{
						// Get the first keypair and
						// remove it from the queue
						keydata = std::move(key_queue.Queue.front());
						key_queue.Queue.pop();

						// Set event to generate more keys and
						// fill the queue again
						m_ThreadPool.GetData().PrimaryThreadEvent.Set();
					}
				});
			}
		});

		return keydata;
	}

	void Manager::PrimaryThreadWait(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event)
	{
		thpdata.PrimaryThreadEvent.Wait(shutdown_event);
	}

	void Manager::PrimaryThreadWaitInterrupt(ThreadPoolData& thpdata)
	{
		thpdata.PrimaryThreadEvent.InterruptWait();
	}

	void Manager::PrimaryThreadProcessor(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event)
	{
		auto has_inactive = false;

		m_KeyQueues.WithSharedLock([&](const KeyQueueMap& queues)
		{
			// Reset event; after we check and generate the keys below
			// this event will be set again when a key gets removed from the queues
			// and we need to fill the queue again
			thpdata.PrimaryThreadEvent.Reset();

			for (auto it = queues.begin(); it != queues.end() && !shutdown_event.IsSet(); ++it)
			{
				auto active = false;
				Size queue_size{ 0 };
				Size num_pending_events{ 0 };

				it->second->WithUniqueLock([&](KeyQueue& key_queue)
				{
					active = key_queue.Active;
					queue_size = key_queue.Queue.size();
					num_pending_events = key_queue.NumPendingEvents;
				});

				if (active)
				{
					Size numkeys{ 0 };
					const Size pending{ queue_size + num_pending_events };
					const Size numpregen{ GetSettings().Local.NumPreGeneratedKeysPerAlgorithm };

					if (pending < numpregen)
					{
						numkeys = numpregen - pending;
					}

					if (numkeys > 0)
					{
						LogDbg(L"Keymanager scheduling generation of %u keys for algorithm %s",
							   numkeys, Crypto::GetAlgorithmName(it->first));

						while (numkeys > 0)
						{
							thpdata.KeyGenEventQueue.Push({ it->second.get() });
							--numkeys;
						}
					}
				}
				else
				{
					has_inactive = true;
				}
			}
		});

		if (has_inactive)
		{
			m_KeyQueues.WithUniqueLock([&](KeyQueueMap& queues)
			{
				auto it = queues.begin();
				while (it != queues.end() && !shutdown_event.IsSet())
				{
					auto erase = false;

					it->second->WithUniqueLock([&](KeyQueue& key_queue)
					{
						if (!key_queue.Active && key_queue.NumPendingEvents == 0)
						{
							erase = true;
						}
					});

					if (erase)
					{
						it = queues.erase(it);
					}
					else ++it;
				}
			});
		}
	}

	void Manager::WorkerThreadWait(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event)
	{
		thpdata.KeyGenEventQueue.Wait(shutdown_event);
	}

	void Manager::WorkerThreadWaitInterrupt(ThreadPoolData& thpdata)
	{
		thpdata.KeyGenEventQueue.InterruptWait();
	}

	void Manager::WorkerThreadProcessor(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event)
	{
		Event event;
		thpdata.KeyGenEventQueue.PopFrontIf([&](auto& fevent) noexcept -> bool
		{
			event = std::move(fevent);
			return true;
		});

		if (event)
		{
			Algorithm::Asymmetric alg{ Algorithm::Asymmetric::Unknown };
			auto active = false;

			event.GetQueue()->WithUniqueLock([&](KeyQueue& key_queue)
			{
				alg = key_queue.Algorithm;
				active = key_queue.Active;
			});

			if (active)
			{
				LogDbg(L"Generating key for asymmetric algorithm %s", Crypto::GetAlgorithmName(alg));

				Crypto::AsymmetricKeyData keydata(alg);

				if (Crypto::GenerateAsymmetricKeys(keydata))
				{
					event.GetQueue()->WithUniqueLock()->Queue.emplace(std::move(keydata));
				}
				else
				{
					LogErr(L"Keymanager failed to generate a key for algorithm %s; will stop trying for this algorithm",
						   Crypto::GetAlgorithmName(alg));

					event.GetQueue()->WithUniqueLock()->Active = false;
				}
			}
		}
	}
}