// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Version.h"
#include "..\..\Common\Dispatcher.h"
#include "..\..\Concurrency\Queue.h"
#include "..\..\Concurrency\RecursiveSharedMutex.h"
#include "..\KeyGeneration\KeyGenerationManager.h"
#include "..\Access\AccessManager.h"
#include "..\Message.h"
#include "..\Extender\ExtenderManager.h"
#include "PeerData.h"
#include "PeerGate.h"
#include "PeerKeyExchange.h"
#include "PeerKeyUpdate.h"
#include "PeerMessageProcessor.h"
#include "PeerNoiseQueue.h"

#include <bitset>

namespace QuantumGate::Implementation::Core::Peer
{
	enum class DisconnectCondition
	{
		None, Unknown, GeneralFailure, SocketError, ConnectError, TimedOutError, ReceiveError, SendError,
		UnknownMessageError, DisconnectRequest, IPNotAllowed, PeerNotAllowed
	};

	class Peer final : public Gate
	{
		enum class Flags
		{
			InQueue = 0,
			NeedsAccessCheck,
			ConcatenateMessages,
			HandshakeStartDelay,
			SendDisabled,
			NeedsExtenderUpdate
		};

		struct DelayedMessage final
		{
			[[nodiscard]] inline const bool IsTime() const noexcept
			{
				if ((Util::GetCurrentSteadyTime() - ScheduleSteadyTime) >= Delay) return true;

				return false;
			}

			Message Message;
			SteadyTime ScheduleSteadyTime;
			std::chrono::milliseconds Delay{ 0 };
		};

		using MessageQueue = Concurrency::Queue<Message>;
		using DelayedMessageQueue = Concurrency::Queue<DelayedMessage>;

		class EventBuffer final : public Buffer
		{
		public:
			inline void SetEvent() noexcept { m_EventState = true; }
			inline void ResetEvent() noexcept { m_EventState = false; }
			[[nodiscard]] inline const bool IsEventSet() const noexcept { return m_EventState; }

			using Buffer::operator=;

		private:
			bool m_EventState{ false };
		};

	public:
		Peer() = delete;
		Peer(Manager& peers, const GateType pgtype, const PeerConnectionType pctype,
			 std::optional<ProtectedBuffer>&& shared_secret);
		Peer(Manager& peers, const IP::AddressFamily af, const Socket::Type type,
			 const IP::Protocol protocol, const PeerConnectionType pctype,
			 std::optional<ProtectedBuffer>&& shared_secret);
		Peer(const Peer&) = delete;
		Peer(Peer&&) = default;
		~Peer() = default;
		Peer& operator=(const Peer&) = delete;
		Peer& operator=(Peer&&) = default;

		[[nodiscard]] const bool Initialize() noexcept;

		const Settings& GetSettings() const noexcept;
		KeyGeneration::Manager& GetKeyGenerationManager() const noexcept;
		Access::Manager& GetAccessManager() const noexcept;
		Manager& GetPeerManager() const noexcept;
		Relay::Manager& GetRelayManager() noexcept;

		inline const PeerLUID GetLUID() const noexcept
		{
			assert(m_PeerData.WithSharedLock()->LUID != 0);
			return m_PeerData.WithSharedLock()->LUID;
		}

		static const PeerLUID MakeLUID(const IPEndpoint& endpoint) noexcept;

		inline const PeerConnectionType GetConnectionType() const noexcept { return m_PeerData.WithSharedLock()->Type; }

		inline const Data_ThS& GetPeerData() const noexcept { return m_PeerData; }

		[[nodiscard]] const bool SetStatus(const Status status) noexcept;
		inline const Status GetStatus() const noexcept { return m_PeerData.WithSharedLock()->Status; }

		inline const bool IsReady() const noexcept { return (GetStatus() == Status::Ready); }
		inline const bool IsInSessionInit() const noexcept { return (GetStatus() == Status::SessionInit); }

		inline const bool IsInHandshake() const noexcept
		{
			return (GetStatus() > Status::Connected && GetStatus() < Status::Ready);
		}

		[[nodiscard]] inline const bool IsAuthenticated() const noexcept { return m_PeerData.WithSharedLock()->IsAuthenticated; }
		void SetAuthenticated(const bool auth) noexcept;

		[[nodiscard]] inline const bool IsRelayed() const noexcept { return m_PeerData.WithSharedLock()->IsRelayed; }

		inline std::pair<UInt8, UInt8> GetLocalProtocolVersion() const noexcept { return m_PeerData.WithSharedLock()->LocalProtocolVersion; }
		inline void SetPeerProtocolVersion(const std::pair<UInt8, UInt8>& version) noexcept { m_PeerData.WithUniqueLock()->PeerProtocolVersion = version; }
		inline std::pair<UInt8, UInt8> GetPeerProtocolVersion() const noexcept { return m_PeerData.WithSharedLock()->PeerProtocolVersion; }

		const String GetLocalName() const noexcept override;
		const String GetPeerName() const noexcept override;

		inline const PeerUUID& GetLocalUUID() const noexcept { return GetSettings().Local.UUID; }
		inline void SetPeerUUID(const PeerUUID& puuid) noexcept { m_PeerData.WithUniqueLock()->PeerUUID = puuid; }
		inline const PeerUUID& GetPeerUUID() const noexcept { return m_PeerData.WithSharedLock()->PeerUUID; }

		inline UInt64 GetLocalSessionID() const noexcept { return m_PeerData.WithSharedLock()->LocalSessionID; }
		inline void SetPeerSessionID(UInt64 id) noexcept { m_PeerData.WithUniqueLock()->PeerSessionID = id; }
		inline UInt64 GetPeerSessionID() const noexcept { return m_PeerData.WithSharedLock()->PeerSessionID; }

		inline const Size GetExtendersBytesReceived() const noexcept { return m_PeerData.WithSharedLock()->ExtendersBytesReceived; }
		inline const Size GetExtendersBytesSent() const noexcept { return m_PeerData.WithSharedLock()->ExtendersBytesSent; }

		inline MessageProcessor& GetMessageProcessor() noexcept { return m_MessageProcessor; }

		[[nodiscard]] const bool Send(Message&& msg, const std::chrono::milliseconds delay = std::chrono::milliseconds(0));
		[[nodiscard]] const bool Send(const MessageType msgtype, Buffer&& buffer,
									  const std::chrono::milliseconds delay = std::chrono::milliseconds(0),
									  const bool compress = true);
		[[nodiscard]] const bool SendWithRandomDelay(const MessageType msgtype, Buffer&& buffer,
													 const std::chrono::milliseconds maxdelay);

		std::chrono::milliseconds GetHandshakeDelayPerMessage() const noexcept;

		const LocalAlgorithms& GetSupportedAlgorithms() const noexcept;

		[[nodiscard]] const bool SetAlgorithms(const Algorithm::Hash ha, const Algorithm::Asymmetric paa,
											   const Algorithm::Asymmetric saa, const Algorithm::Symmetric sa,
											   const Algorithm::Compression ca) noexcept;

		inline const Algorithms& GetAlgorithms() const noexcept { return m_Algorithms; }

		[[nodiscard]] inline const bool IsUsingGlobalSharedSecret() const noexcept { return !GetGlobalSharedSecret().IsEmpty(); }
		const ProtectedBuffer& GetGlobalSharedSecret() const noexcept;
		inline const ProtectedBuffer& GetLocalPrivateKey() const noexcept { return GetSettings().Local.Keys.PrivateKey; }
		const ProtectedBuffer* GetPeerPublicKey() const noexcept;

		inline SymmetricKeys& GetKeys() noexcept { return m_Keys; }
		[[nodiscard]] const bool InitializeKeyExchange() noexcept;
		void ReleaseKeyExchange() noexcept;
		inline KeyExchange& GetKeyExchange() noexcept { assert(m_KeyExchange != nullptr); return *m_KeyExchange; }
		inline const KeyExchange& GetKeyExchange() const noexcept { assert(m_KeyExchange != nullptr); return *m_KeyExchange; }

		inline KeyUpdate& GetKeyUpdate() noexcept { return m_KeyUpdate; }

		UInt8 SetLocalMessageCounter() noexcept;
		std::optional<UInt8> GetNextLocalMessageCounter() noexcept;
		void SetPeerMessageCounter(const UInt8 counter) noexcept;
		std::optional<UInt8> GetNextPeerMessageCounter() noexcept;

		SerializedIPEndpoint GetPublicIPEndpointToReport() const noexcept;
		[[nodiscard]] const bool AddReportedPublicIPEndpoint(const SerializedIPEndpoint& pub_endpoint) noexcept;

		const Extender::ActiveExtenderUUIDs& GetLocalExtenderUUIDs() noexcept;
		inline ExtenderUUIDs& GetPeerExtenderUUIDs() noexcept { return m_PeerExtenderUUIDs; }

		void AddConnectCallback(ConnectCallback&& function) noexcept { m_ConnectCallbacks.Add(std::move(function)); }
		void AddDisconnectCallback(DisconnectCallback&& function) noexcept { m_DisconnectCallbacks.Add(std::move(function)); }

		[[nodiscard]] inline const bool IsInQueue() const noexcept { return IsFlagSet(Flags::InQueue); }
		inline void SetInQueue(const bool flag) noexcept { SetFlag(Flags::InQueue, flag); }

		inline const UInt64 GetThreadPoolKey() const noexcept { return m_ThreadPoolKey; }
		inline void SetThreadPoolKey(const UInt64 key) noexcept { m_ThreadPoolKey = key; }

		[[nodiscard]] inline const bool ShouldDisconnect() const noexcept { return (m_DisconnectCondition != DisconnectCondition::None); }
		inline DisconnectCondition GetDisconnectCondition() const noexcept { return m_DisconnectCondition; }
		inline void SetDisconnectCondition(const DisconnectCondition dc) noexcept { if (!ShouldDisconnect()) m_DisconnectCondition = dc; }

		[[nodiscard]] const bool UpdateSocketStatus() noexcept;
		[[nodiscard]] const bool CheckStatus(const bool noise_enabled, const std::chrono::seconds max_connect_duration,
											 std::chrono::seconds max_handshake_duration) noexcept;

		void UpdateReputation(const Access::IPReputationUpdate rep_update) noexcept;

		[[nodiscard]] const bool HasPendingEvents() noexcept;
		[[nodiscard]] const bool ProcessEvents();
		void ProcessLocalExtenderUpdate(const Vector<ExtenderUUID>& extuuids);
		[[nodiscard]] const bool ProcessPeerExtenderUpdate(Vector<ExtenderUUID>&& uuids) noexcept;

		inline void SetNeedsAccessCheck() noexcept { SetFlag(Flags::NeedsAccessCheck, true); }
		[[nodiscard]] inline const bool NeedsAccessCheck() const noexcept { return IsFlagSet(Flags::NeedsAccessCheck); }
		void CheckAccess() noexcept;

		inline void SetNeedsExtenderUpdate() noexcept { SetFlag(Flags::NeedsExtenderUpdate, true); }
		[[nodiscard]] inline const bool NeedsExtenderUpdate() const noexcept { return IsFlagSet(Flags::NeedsExtenderUpdate); }

		void OnUnhandledExtenderMessage(const ExtenderUUID& extuuid, const std::pair<bool, bool>& result) noexcept;

	protected:
		void OnConnecting() noexcept override;
		void OnAccept() noexcept override;
		const bool OnConnect() noexcept override;
		void OnClose() noexcept override;

	private:
		void SetLUID() noexcept;

		Extender::Manager& GetExtenderManager() const noexcept;

		[[nodiscard]] const bool OnStatusChange(const Status old_status, const Status new_status);

		[[nodiscard]] const bool SendFromNoiseQueue();

		void EnableSend() noexcept;
		void DisableSend() noexcept;
		void DisableSend(const std::chrono::milliseconds duration) noexcept;

		[[nodiscard]] const bool SendNoise(const Size minsize, const Size maxsize,
										   const std::chrono::milliseconds delay = std::chrono::milliseconds(0));
		[[nodiscard]] const bool SendNoise(const Size maxnum, const Size minsize, const Size maxsize);

		[[nodiscard]] inline const bool HasReceiveEvents() noexcept
		{
			return (GetIOStatus().CanRead() || m_ReceiveBuffer.IsEventSet());
		}

		[[nodiscard]] inline const bool HasSendEvents() noexcept
		{
			return (GetIOStatus().CanWrite() && !IsFlagSet(Flags::SendDisabled) &&
				(m_SendBuffer.IsEventSet() || m_SendQueue.Event().IsSet() ||
					(m_DelayedSendQueue.Event().IsSet() && m_DelayedSendQueue.Front().IsTime())));
		}

		[[nodiscard]] const bool SendFromQueue();
		[[nodiscard]] const std::pair<bool, Size> GetMessagesFromSendQueue(Buffer& buffer,
																		   const Crypto::SymmetricKeyData& symkey);

		[[nodiscard]] const bool ReceiveAndProcess();
		[[nodiscard]] const std::tuple<bool, Size, UInt16> ProcessMessage(const BufferView msgbuf,
																		  const Settings& settings);
		[[nodiscard]] const std::pair<bool, Size> ProcessMessages(BufferView buffer,
																  const Crypto::SymmetricKeyData& symkey);

		[[nodiscard]] const bool ProcessMessage(Message& msg);
		[[nodiscard]] const std::pair<bool, bool> ProcessMessage(MessageDetails&& msg);

		void ProcessEvent(const PeerEventType etype) noexcept;
		void ProcessEvent(const Vector<ExtenderUUID>& extuuids, const PeerEventType etype) noexcept;

		[[nodiscard]] const bool CheckAndProcessKeyUpdate() noexcept;

		void SetInitialConditionsWithGlobalSharedSecret(const ProtectedBuffer& encr_authkey,
														const ProtectedBuffer& decr_authkey) noexcept;

		[[nodiscard]] inline const bool IsAutoGenKeyAllowed() const noexcept;

		ForceInline void SetFlag(const Flags flag, const bool state) noexcept
		{
			m_Flags.set(static_cast<Size>(flag), state);
		}

		[[nodiscard]] ForceInline const bool IsFlagSet(const Flags flag) const noexcept
		{
			return (m_Flags.test(static_cast<Size>(flag)));
		}

		const ResultCode GetDisconnectConditionResultCode() const noexcept;

	private:
		static constexpr Size NumHandshakeDelayMessages{ 8 };

		Manager& m_PeerManager;

		Data_ThS m_PeerData;

		SteadyTime m_LastStatusChangeSteadyTime;

		std::bitset<8> m_Flags{ 0 };

		MessageTransport::DataSizeSettings m_MessageTransportDataSizeSettings;

		UInt16 m_NextLocalRandomDataPrefixLength{ 0 };
		UInt16 m_NextPeerRandomDataPrefixLength{ 0 };

		UInt64 m_ThreadPoolKey{ 0 };

		DisconnectCondition m_DisconnectCondition{ DisconnectCondition::None };

		ExtenderUUIDs m_PeerExtenderUUIDs;

		MessageQueue m_SendQueue;
		DelayedMessageQueue m_DelayedSendQueue;

		EventBuffer m_ReceiveBuffer;
		EventBuffer m_SendBuffer;
		std::optional<MessageDetails> m_MessageFragments;

		NoiseQueue m_NoiseQueue;

		std::optional<UInt8> m_LocalMessageCounter;
		std::optional<UInt8> m_PeerMessageCounter;

		std::chrono::milliseconds m_SendDisabledDuration{ 0 };
		SteadyTime m_SendDisabledSteadyTime;

		Algorithms m_Algorithms;

		SymmetricKeys m_Keys;
		std::unique_ptr<KeyExchange> m_KeyExchange{ nullptr };
		KeyUpdate m_KeyUpdate{ *this };

		std::optional<ProtectedBuffer> m_GlobalSharedSecret;

		MessageProcessor m_MessageProcessor{ *this };

		Dispatcher<void(const PeerLUID, const Result<ConnectDetails> result)> m_ConnectCallbacks;
		Dispatcher<void(const PeerLUID, const PeerUUID)> m_DisconnectCallbacks;
	};

	using Peer_ThS = Concurrency::ThreadSafe<Peer, Concurrency::RecursiveSharedMutex>;
}
