// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Concurrency\Queue.h"
#include "..\..\Concurrency\ThreadSafe.h"
#include "..\..\Concurrency\ThreadPool.h"
#include "..\Peer\PeerEvent.h"
#include "Extender.h"
#include "ExtenderModule.h"

namespace QuantumGate::Implementation::Core::Extender
{
	class Manager;

	class Control final
	{
		using ThreadPoolKey = UInt64;

		struct Peer final
		{
			enum class Status { Unknown, Connected, Disconnected };

			Peer() = delete;

			Peer(const ThreadPoolKey thpkey, std::atomic<Size>& peercount, Status status) noexcept :
				Status(status), ThreadPoolKey(thpkey), ThreadPoolPeerCount(peercount)
			{
				++ThreadPoolPeerCount;
			}

			Peer(const Peer&) = delete;
			Peer(Peer&&) noexcept = default;

			~Peer()
			{
				--ThreadPoolPeerCount;
			}

			Peer& operator=(const Peer&) = delete;
			Peer& operator=(Peer&&) noexcept = default;

			Status Status{ Status::Unknown };
			Containers::Queue<Core::Peer::Event> EventQueue;
			Containers::Queue<Core::Peer::Event> MessageQueue;

			bool IsInQueue{ false };
			const ThreadPoolKey ThreadPoolKey{ 0 };
			std::atomic<Size>& ThreadPoolPeerCount;
		};

		using Peer_ThS = Concurrency::ThreadSafe<Peer, Concurrency::SpinMutex>;

		using PeerMap = Containers::UnorderedMap<PeerLUID, std::shared_ptr<Peer_ThS>>;

		using Queue_ThS = Concurrency::Queue<std::shared_ptr<Peer_ThS>>;

		struct ThreadPoolData final
		{
			const Manager& ExtenderManager;
			Extender* const ExtenderPointer{ nullptr };

			Queue_ThS Queue;
			std::atomic<Size> PeerCount{ 0 };

			ThreadPoolData(const Manager& mgr, Extender* extender_ptr) noexcept :
				ExtenderManager(mgr), ExtenderPointer(extender_ptr)
			{}
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData>;
		using ThreadPoolMap = Containers::UnorderedMap<ThreadPoolKey, std::unique_ptr<ThreadPool>>;

		struct Data final
		{
			std::shared_ptr<QuantumGate::API::Extender> Extender;
			ExtenderModuleID ExtenderModuleID{ 0 };
			SteadyTime SteadyTimeAdded;
			SteadyTime SteadyTimeRemoved;

			PeerMap Peers;
			ThreadPoolMap ThreadPools;
		};

		using Data_ThS = Concurrency::ThreadSafe<Data, std::shared_mutex>;

	public:
		enum class Status { Startup, Running, Shutdown, Stopped };

		Control() = delete;
		Control(const Manager& mgr, const std::shared_ptr<QuantumGate::API::Extender>& extender,
				const ExtenderModuleID moduleid) noexcept;
		Control(const Control&) = delete;
		Control(Control&&) noexcept = default;
		~Control() = default;
		Control& operator=(const Control&) = delete;
		Control& operator=(Control&&) noexcept = default;

		void SetStatus(const Status status) noexcept;
		inline Status GetStatus() const noexcept { return m_Status; }

		inline SteadyTime GetSteadyTimeRemoved() const noexcept { return m_Data.WithSharedLock()->SteadyTimeRemoved; }

		inline bool HasExtender() const noexcept { return (m_Data.WithSharedLock()->Extender != nullptr); }

		[[nodiscard]] inline bool IsSameExtender(const std::shared_ptr<QuantumGate::API::Extender>& extender,
												 const ExtenderModuleID moduleid) const noexcept
		{
			auto data = m_Data.WithSharedLock();
			return (data->Extender.get() == extender.get() && data->ExtenderModuleID == moduleid);
		}

		inline void ReleaseExtender() noexcept { m_Data.WithUniqueLock()->Extender.reset(); }

		inline const Extender& GetExtender() const noexcept
		{
			auto data = m_Data.WithSharedLock();
			assert(data->Extender != nullptr && data->Extender->m_Extender != nullptr);
			return *data->Extender->m_Extender;
		}

		inline Extender& GetExtender() noexcept
		{
			return const_cast<Extender&>(const_cast<const Control*>(this)->GetExtender());
		}

		inline const std::shared_ptr<QuantumGate::API::Extender>& GetAPIExtender() const noexcept
		{
			return m_Data.WithSharedLock()->Extender;
		}

		inline String GetExtenderName() const noexcept { return GetExtenderName(GetExtender()); }

		static inline String GetExtenderName(const Extender& extender) noexcept
		{
			return Util::FormatString(L"'%s' (UUID: %s)", extender.GetName().c_str(), extender.GetUUID().GetString().c_str());
		}

		[[nodiscard]] bool AddPeerEvent(Core::Peer::Event&& event) noexcept;

		[[nodiscard]] bool StartupExtenderThreadPools() noexcept;
		void ShutdownExtenderThreadPools() noexcept;

	private:
		void PreStartupExtenderThreadPools(Data& data) noexcept;
		void ResetState(Data& data) noexcept;

		static void WorkerThreadWait(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event);
		static void WorkerThreadWaitInterrupt(ThreadPoolData& thpdata);
		static void WorkerThreadProcessor(ThreadPoolData& thpdata, const Concurrency::Event& shutdown_event);

	private:
		const Manager& m_ExtenderManager;

		std::atomic<Status> m_Status{ Status::Stopped };
		Data_ThS m_Data;
	};
}