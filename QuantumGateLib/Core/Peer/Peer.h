// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Version.h"
#include "..\..\Common\Dispatcher.h"
#include "..\KeyGeneration\KeyGenerationManager.h"
#include "..\Access\AccessManager.h"
#include "..\Extender\ExtenderManager.h"
#include "PeerData.h"
#include "PeerGate.h"
#include "PeerKeyExchange.h"
#include "PeerKeyUpdate.h"
#include "PeerMessageProcessor.h"
#include "PeerNoiseQueue.h"
#include "PeerSendQueues.h"
#include "PeerReceiveQueues.h"

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
			FastRequeue,
			NeedsAccessCheck,
			ConcatenateMessages,
			HandshakeStartDelay,
			SendDisabled,
			NeedsExtenderUpdate
		};

		class EventBuffer final : public Buffer
		{
		public:
			inline void SetEvent() noexcept { m_EventState = true; }
			inline void ResetEvent() noexcept { m_EventState = false; }
			[[nodiscard]] inline bool IsEventSet() const noexcept { return m_EventState; }

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
		Peer(Peer&&) noexcept = default;
		~Peer() = default;
		Peer& operator=(const Peer&) = delete;
		Peer& operator=(Peer&&) noexcept = default;

		[[nodiscard]] bool Initialize(PeerWeakPointer&& peer_ths) noexcept;

		[[nodiscard]] const PeerWeakPointer& GetWeakPointer() const noexcept { return m_PeerPointer; }

		const Settings& GetSettings() const noexcept;
		KeyGeneration::Manager& GetKeyGenerationManager() const noexcept;
		Access::Manager& GetAccessManager() const noexcept;
		Manager& GetPeerManager() const noexcept;
		Relay::Manager& GetRelayManager() noexcept;

		[[nodiscard]] inline PeerLUID GetLUID() const noexcept
		{
			assert(m_PeerData.WithSharedLock()->LUID != 0);
			return m_PeerData.WithSharedLock()->LUID;
		}

		static PeerLUID MakeLUID(const IPEndpoint& endpoint, const UInt64 unique_data) noexcept;

		inline PeerConnectionType GetConnectionType() const noexcept { return m_PeerData.WithSharedLock()->Type; }

		inline const Data_ThS& GetPeerData() const noexcept { return m_PeerData; }

		[[nodiscard]] bool SetStatus(const Status status) noexcept;
		[[nodiscard]] inline Status GetStatus() const noexcept { return m_PeerData.WithSharedLock()->Status; }

		[[nodiscard]] inline bool IsReady() const noexcept { return (GetStatus() == Status::Ready); }
		[[nodiscard]] inline bool IsInSessionInit() const noexcept { return (GetStatus() == Status::SessionInit); }

		[[nodiscard]] inline bool IsInHandshake() const noexcept
		{
			return (GetStatus() > Status::Connected&& GetStatus() < Status::Ready);
		}

		[[nodiscard]] inline bool IsAuthenticated() const noexcept { return m_PeerData.WithSharedLock()->IsAuthenticated; }
		void SetAuthenticated(const bool auth) noexcept;

		[[nodiscard]] inline bool IsRelayed() const noexcept { return m_PeerData.WithSharedLock()->IsRelayed; }

		[[nodiscard]] inline std::pair<UInt8, UInt8> GetLocalProtocolVersion() const noexcept { return m_PeerData.WithSharedLock()->LocalProtocolVersion; }
		inline void SetPeerProtocolVersion(const std::pair<UInt8, UInt8>& version) noexcept { m_PeerData.WithUniqueLock()->PeerProtocolVersion = version; }
		[[nodiscard]] inline std::pair<UInt8, UInt8> GetPeerProtocolVersion() const noexcept { return m_PeerData.WithSharedLock()->PeerProtocolVersion; }

		[[nodiscard]] String GetLocalName() const noexcept override;
		[[nodiscard]] String GetPeerName() const noexcept override;

		[[nodiscard]] inline const PeerUUID& GetLocalUUID() const noexcept { return GetSettings().Local.UUID; }
		inline void SetPeerUUID(const PeerUUID& puuid) noexcept { m_PeerData.WithUniqueLock()->PeerUUID = puuid; }
		[[nodiscard]] inline const PeerUUID& GetPeerUUID() const noexcept { return m_PeerData.WithSharedLock()->PeerUUID; }

		[[nodiscard]] inline UInt64 GetLocalSessionID() const noexcept { return m_PeerData.WithSharedLock()->LocalSessionID; }
		inline void SetPeerSessionID(UInt64 id) noexcept { m_PeerData.WithUniqueLock()->PeerSessionID = id; }
		[[nodiscard]] inline UInt64 GetPeerSessionID() const noexcept { return m_PeerData.WithSharedLock()->PeerSessionID; }

		[[nodiscard]] inline Size GetExtendersBytesReceived() const noexcept { return m_PeerData.WithSharedLock()->ExtendersBytesReceived; }
		[[nodiscard]] inline Size GetExtendersBytesSent() const noexcept { return m_PeerData.WithSharedLock()->ExtendersBytesSent; }

		[[nodiscard]] inline MessageProcessor& GetMessageProcessor() noexcept { return m_MessageProcessor; }

		[[nodiscard]] Result<> Send(Message&& msg, const SendParameters::PriorityOption priority = SendParameters::PriorityOption::Normal,
									const std::chrono::milliseconds delay = std::chrono::milliseconds(0), SendCallback&& callback = nullptr) noexcept;
		[[nodiscard]] Result<> Send(const MessageType msgtype, Buffer&& buffer,
									const SendParameters::PriorityOption priority = SendParameters::PriorityOption::Normal,
									const std::chrono::milliseconds delay = std::chrono::milliseconds(0),
									const bool compress = true, SendCallback&& callback = nullptr) noexcept;
		[[nodiscard]] Result<> SendWithRandomDelay(const MessageType msgtype, Buffer&& buffer,
												   const std::chrono::milliseconds maxdelay) noexcept;

		[[nodiscard]] inline MessageRateLimits& GetMessageRateLimits() noexcept { return m_RateLimits; }

		[[nodiscard]] inline Size GetAvailableExtenderCommunicationSendBufferSize() const noexcept
		{
			return m_RateLimits.GetAvailable<MessageRateLimits::Type::ExtenderCommunicationSend>();
		}

		[[nodiscard]] inline Size GetAvailableRelayDataSendBufferSize() const noexcept
		{
			return m_RateLimits.GetAvailable<MessageRateLimits::Type::RelayDataSend>();
		}

		[[nodiscard]] inline Size GetAvailableNoiseSendBufferSize() const noexcept
		{
			return m_RateLimits.GetAvailable<MessageRateLimits::Type::NoiseSend>();
		}

		[[nodiscard]] std::chrono::milliseconds GetHandshakeDelayPerMessage() const noexcept;

		[[nodiscard]] const LocalAlgorithms& GetSupportedAlgorithms() const noexcept;

		[[nodiscard]] bool SetAlgorithms(const Algorithm::Hash ha, const Algorithm::Asymmetric paa,
										 const Algorithm::Asymmetric saa, const Algorithm::Symmetric sa,
										 const Algorithm::Compression ca) noexcept;

		[[nodiscard]] inline const Algorithms& GetAlgorithms() const noexcept { return m_Algorithms; }

		[[nodiscard]] inline bool IsUsingGlobalSharedSecret() const noexcept { return !GetGlobalSharedSecret().IsEmpty(); }
		[[nodiscard]] const ProtectedBuffer& GetGlobalSharedSecret() const noexcept;
		[[nodiscard]] inline const ProtectedBuffer& GetLocalPrivateKey() const noexcept { return GetSettings().Local.Keys.PrivateKey; }
		[[nodiscard]] const ProtectedBuffer* GetPeerPublicKey() const noexcept;

		[[nodiscard]] inline SymmetricKeys& GetKeys() noexcept { return m_Keys; }
		[[nodiscard]] bool InitializeKeyExchange() noexcept;
		void ReleaseKeyExchange() noexcept;
		[[nodiscard]] inline KeyExchange& GetKeyExchange() noexcept { assert(m_KeyExchange != nullptr); return *m_KeyExchange; }
		[[nodiscard]] inline const KeyExchange& GetKeyExchange() const noexcept { assert(m_KeyExchange != nullptr); return *m_KeyExchange; }

		[[nodiscard]] inline KeyUpdate& GetKeyUpdate() noexcept { return m_KeyUpdate; }

		[[nodiscard]] UInt8 SetLocalMessageCounter() noexcept;
		[[nodiscard]] std::optional<UInt8> GetNextLocalMessageCounter() noexcept;
		void SetPeerMessageCounter(const UInt8 counter) noexcept;
		[[nodiscard]] std::optional<UInt8> GetNextPeerMessageCounter() noexcept;

		[[nodiscard]] SerializedIPEndpoint GetPublicIPEndpointToReport() const noexcept;
		[[nodiscard]] bool AddReportedPublicIPEndpoint(const SerializedIPEndpoint& pub_endpoint) noexcept;

		[[nodiscard]] const Extender::ActiveExtenderUUIDs& GetLocalExtenderUUIDs() noexcept;
		[[nodiscard]] inline ExtenderUUIDs& GetPeerExtenderUUIDs() noexcept { return m_PeerExtenderUUIDs; }

		void AddConnectCallback(ConnectCallback&& function) noexcept { m_ConnectCallbacks.Add(std::move(function)); }
		void AddDisconnectCallback(DisconnectCallback&& function) noexcept { m_DisconnectCallbacks.Add(std::move(function)); }

		[[nodiscard]] inline bool IsFastRequeue() const noexcept { return IsFlagSet(Flags::FastRequeue); }
		inline void SetFastRequeue(const bool flag) noexcept { SetFlag(Flags::FastRequeue, flag); }
		inline void ResetFastRequeue() noexcept { SetFastRequeue(true); }

		[[nodiscard]] inline bool IsInQueue() const noexcept { return IsFlagSet(Flags::InQueue); }
		inline void SetInQueue(const bool flag) noexcept { SetFlag(Flags::InQueue, flag); }

		[[nodiscard]] inline UInt64 GetThreadPoolKey() const noexcept { return m_ThreadPoolKey; }
		inline void SetThreadPoolKey(const UInt64 key) noexcept { m_ThreadPoolKey = key; }

		[[nodiscard]] inline bool ShouldDisconnect() const noexcept { return (m_DisconnectCondition != DisconnectCondition::None); }
		[[nodiscard]] inline DisconnectCondition GetDisconnectCondition() const noexcept { return m_DisconnectCondition; }
		inline void SetDisconnectCondition(const DisconnectCondition dc) noexcept { if (!ShouldDisconnect()) m_DisconnectCondition = dc; }

		[[nodiscard]] bool UpdateSocketStatus() noexcept;
		[[nodiscard]] bool CheckStatus(const bool noise_enabled, const std::chrono::seconds max_connect_duration,
									   std::chrono::seconds max_handshake_duration) noexcept;

		void UpdateReputation(const Access::IPReputationUpdate rep_update) noexcept;

		[[nodiscard]] bool HasPendingEvents() noexcept;
		[[nodiscard]] bool ProcessEvents();
		void ProcessLocalExtenderUpdate(const Vector<ExtenderUUID>& extuuids);
		[[nodiscard]] bool ProcessPeerExtenderUpdate(Vector<ExtenderUUID>&& uuids) noexcept;

		inline void SetNeedsAccessCheck() noexcept { SetFlag(Flags::NeedsAccessCheck, true); }
		[[nodiscard]] inline bool NeedsAccessCheck() const noexcept { return IsFlagSet(Flags::NeedsAccessCheck); }
		void CheckAccess() noexcept;

		inline void SetNeedsExtenderUpdate() noexcept { SetFlag(Flags::NeedsExtenderUpdate, true); }
		[[nodiscard]] inline bool NeedsExtenderUpdate() const noexcept { return IsFlagSet(Flags::NeedsExtenderUpdate); }

		void ScheduleCallback(Callback<void()>&& callback) noexcept;

		void OnUnhandledExtenderMessage(const ExtenderUUID& extuuid, const API::Extender::PeerEvent::Result& result) noexcept;

	protected:
		void OnConnecting() noexcept override;
		void OnAccept() noexcept override;
		bool OnConnect() noexcept override;
		void OnClose() noexcept override;

	private:
		void SetLUID() noexcept;

		Extender::Manager& GetExtenderManager() const noexcept;

		[[nodiscard]] bool OnStatusChange(const Status old_status, const Status new_status);

		[[nodiscard]] bool SendFromNoiseQueue(const Settings& settings) noexcept;

		void EnableSend() noexcept;
		void DisableSend() noexcept;
		void DisableSend(const std::chrono::milliseconds duration) noexcept;

		[[nodiscard]] Result<> SendNoise(const Size minsize, const Size maxsize,
										 const std::chrono::milliseconds delay = std::chrono::milliseconds(0)) noexcept;
		[[nodiscard]] Result<> SendNoise(const Size maxnum, const Size minsize, const Size maxsize) noexcept;

		[[nodiscard]] inline bool HasReceiveEvents() noexcept
		{
			return (GetIOStatus().CanRead() || m_ReceiveBuffer.IsEventSet() || m_ReceiveQueues.HaveMessages());
		}

		[[nodiscard]] inline bool HasSendEvents() noexcept
		{
			return (GetIOStatus().CanWrite() && !IsFlagSet(Flags::SendDisabled) &&
				(m_SendBuffer.IsEventSet() || m_SendQueues.HaveMessages()));
		}

		[[nodiscard]] bool SendFromQueues(const Settings& settings);

		[[nodiscard]] bool ProcessFromReceiveQueues(const Settings& settings);
		[[nodiscard]] bool ReceiveAndProcess(const Settings& settings);
		[[nodiscard]] std::tuple<bool, Size, UInt16> ProcessMessageTransport(const BufferView msgbuf, const Settings& settings);
		[[nodiscard]] std::pair<bool, Size> ProcessMessages(BufferView buffer, const Crypto::SymmetricKeyData& symkey);

		[[nodiscard]] bool QueueOrProcessMessage(Message&& msg) noexcept;
		[[nodiscard]] bool ProcessMessage(Message& msg);
		[[nodiscard]] MessageProcessor::Result ProcessMessage(MessageDetails&& msg) noexcept;

		void ProcessEvent(const Event::Type etype) noexcept;
		void ProcessEvent(const Vector<ExtenderUUID>& extuuids, const Event::Type etype) noexcept;

		[[nodiscard]] bool CheckAndProcessKeyUpdate() noexcept;

		void SetInitialConditionsWithGlobalSharedSecret(const ProtectedBuffer& encr_authkey,
														const ProtectedBuffer& decr_authkey) noexcept;

		[[nodiscard]] inline bool IsAutoGenKeyAllowed() const noexcept;

		ForceInline void SetFlag(const Flags flag, const bool state) noexcept
		{
			m_Flags.set(static_cast<Size>(flag), state);
		}

		[[nodiscard]] ForceInline bool IsFlagSet(const Flags flag) const noexcept
		{
			return (m_Flags.test(static_cast<Size>(flag)));
		}

		[[nodiscard]] ResultCode GetDisconnectConditionResultCode() const noexcept;

	private:
		static constexpr Size NumHandshakeDelayMessages{ 8 };

	private:
		Manager& m_PeerManager;

		Data_ThS m_PeerData;

		PeerWeakPointer m_PeerPointer;

		SteadyTime m_LastStatusChangeSteadyTime;

		std::bitset<8> m_Flags{ 0 };

		MessageTransport::DataSizeSettings m_MessageTransportDataSizeSettings;

		UInt16 m_NextLocalRandomDataPrefixLength{ 0 };
		UInt16 m_NextPeerRandomDataPrefixLength{ 0 };

		UInt64 m_ThreadPoolKey{ 0 };

		DisconnectCondition m_DisconnectCondition{ DisconnectCondition::None };

		ExtenderUUIDs m_PeerExtenderUUIDs;

		MessageRateLimits m_RateLimits;
		PeerReceiveQueues m_ReceiveQueues;
		PeerSendQueues m_SendQueues{ *this };

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
}
