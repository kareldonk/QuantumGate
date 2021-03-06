// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <unordered_map>
#include <chrono>
#include <atomic>

#include <QuantumGate.h>
#include <Concurrency\ThreadSafe.h>
#include <Concurrency\Event.h>

namespace TestExtender
{
	using namespace QuantumGate;
	using namespace QuantumGate::Implementation;

	enum class MessageType : UInt16
	{
		Unknown = 0,
		MessageString,
		BenchmarkStart,
		BenchmarkEnd,
		FileTransferStart,
		FileTransferAccept,
		FileTransferData,
		FileTransferDataAck,
		FileTransferCancel,
		Echo,
		EchoReply
	};

	enum class FileTransferType : UInt16
	{
		Unknown,
		Incoming,
		Outgoing
	};

	enum class FileTransferStatus : UInt16
	{
		Unknown,
		NeedAccept,
		WaitingForAccept,
		Transfering,
		Error,
		Cancelled,
		Succeeded
	};

	using FileTransferID = UInt64;

	class FileTransfer final
	{
	public:
		FileTransfer(QuantumGate::Peer& peer, const FileTransferType type, const Size trfbuf_size,
					 const bool autotrf, const bool benchmark, const Size benchmark_size) noexcept;
		FileTransfer(QuantumGate::Peer& peer, const FileTransferType type, const FileTransferID id,
					 const Size filesize, const String& filename, Buffer&& filehash,
					 const Size trfbuf_size, const bool autotrf, const bool benchmark) noexcept;
		~FileTransfer();

		bool OpenSourceFile(const String& filename);
		Size ReadFromFile(Byte* buffer, Size size) noexcept;
		bool OpenDestinationFile(const String& filename);
		bool WriteToFile(const Byte* buffer, const Size size) noexcept;

		void SetStatus(const FileTransferStatus status) noexcept;

		inline QuantumGate::Peer& GetPeer() noexcept { return m_Peer; }
		inline const FileTransferStatus GetStatus() const noexcept { return m_Status; }
		const WChar* GetStatusString() const noexcept;
		inline const FileTransferType GetType() const noexcept { return m_Type; }
		inline bool IsAuto() const noexcept { return m_Auto; }
		inline bool IsBenchmark() const noexcept { return m_Benchmark; }
		inline const SteadyTime GetLastActiveSteadyTime() const noexcept { return m_LastActiveSteadyTime; }
		inline const SteadyTime GetTransferStartSteadyTime() const noexcept { return m_TransferStartSteadyTime; }
		inline String GetFileName() const noexcept { return m_FileName; }
		inline const FileTransferID GetID() const noexcept { return m_ID; }
		inline Size GetFileSize() const noexcept { return m_FileSize; }
		inline Size GetNumBytesTransferred() const noexcept { return m_NumBytesTransferred; }
		inline Buffer* GetFileHash() noexcept { return &m_FileHash; }
		inline Buffer& GetTransferBuffer() noexcept { return m_TransferBuffer; }

	private:
		bool CalcFileHash(Buffer& hashbuff) noexcept;
		void TransferEndStats() const noexcept;

	private:
		QuantumGate::Peer& m_Peer;
		FileTransferType m_Type{ FileTransferType::Unknown };
		FileTransferStatus m_Status{ FileTransferStatus::Unknown };
		bool m_Auto{ false };
		bool m_Benchmark{ false };
		Size m_BenchmarkSize{ 0 };
		Buffer m_BenchmarkBuffer;
		FileTransferID m_ID{ 0 };
		Buffer m_FileHash;
		String m_FileName;
		std::FILE* m_File{ nullptr };
		Size m_FileSize{ 0 };
		Size m_NumBytesTransferred{ 0 };
		Buffer m_TransferBuffer;
		SteadyTime m_TransferStartSteadyTime;
		SteadyTime m_LastActiveSteadyTime;
	};

	using FileTransfers = std::unordered_map<FileTransferID, std::unique_ptr<FileTransfer>>;
	using FileTransfers_ThS = Implementation::Concurrency::ThreadSafe<FileTransfers, std::shared_mutex>;

	struct PeerData final
	{
		PeerData(QuantumGate::Peer&& peer) noexcept : Peer(std::move(peer)) {}

		QuantumGate::Peer Peer;
		FileTransfers_ThS FileTransfers;
	};

	using Peers = std::unordered_map<PeerLUID, std::unique_ptr<PeerData>>;
	using Peers_ThS = Implementation::Concurrency::ThreadSafe<Peers, std::shared_mutex>;

	struct FileAccept final
	{
		PeerLUID PeerLUID{ 0 };
		FileTransferID FileTransferID{ 0 };
	};

	enum class WindowsMessage : UINT
	{
		PeerEvent = WM_USER + 1,
		FileAccept = WM_USER + 2,
		ExtenderInit = WM_USER + 3,
		ExtenderDeinit = WM_USER + 4,
		PingResult = WM_USER + 5
	};

	struct Event final
	{
		QuantumGate::Extender::PeerEvent::Type Type{ QuantumGate::Extender::PeerEvent::Type::Unknown };
		PeerLUID PeerLUID{ 0 };
	};

	struct PingData final
	{
		bool Active{ false };
		PeerLUID PeerLUID{ 0 };
		SteadyTime TimeSent;
		std::chrono::milliseconds TimeOut{ 5000 };
		Buffer Data;

		void Reset()
		{
			Active = false;
			PeerLUID = 0;
			TimeSent = SteadyTime{};
			TimeOut = std::chrono::milliseconds{ 5000 };
			Data.Clear();
			Data.FreeUnused();
		}
	};

	using Ping_ThS = Implementation::Concurrency::ThreadSafe<PingData, std::shared_mutex>;

	class Extender final : public QuantumGate::Extender
	{
		using String_ThS = Concurrency::ThreadSafe<String, std::shared_mutex>;
		using atomic_time_point = std::atomic<std::chrono::time_point<std::chrono::high_resolution_clock>>;

	public:
		Extender(HWND wnd) noexcept;
		virtual ~Extender();

		inline void SetAutoFileTransferPath(const String& path) { m_AutoFileTransferPath.WithUniqueLock() = path; }
		inline void SetUseCompression(const bool compression) noexcept { m_UseCompression = compression; }
		inline bool IsUsingCompression() const noexcept { return m_UseCompression; }

		bool SendMessage(const PeerLUID pluid, const String& msg, const SendParameters::PriorityOption priority,
						 const std::chrono::milliseconds delay) const;

		bool SendBenchmarkStart(const PeerLUID pluid) noexcept;
		bool SendBenchmarkEnd(const PeerLUID pluid) noexcept;

		bool Ping(const PeerLUID pluid, const Size size, const std::chrono::milliseconds timeout) noexcept;
		Size GetMaxPingSize() const noexcept;
		[[nodiscard]] bool IsPingActive() const noexcept;

		bool SendFile(const PeerLUID pluid, const String filename, const bool autotrf,
					  const bool benchmark, const Size benchmark_size);
		bool AcceptFile(const PeerLUID pluid, const FileTransferID ftid, const String& filename);

		const Peers_ThS* GetPeers() const noexcept;

	protected:
		bool OnStartup();
		void OnPostStartup();
		void OnShutdown();
		void OnPeerEvent(PeerEvent&& event);
		QuantumGate::Extender::PeerEvent::Result OnPeerMessage(PeerEvent&& event);

	private:
		static void WorkerThreadLoop(Extender* extender);

		template<typename Func>
		bool IfHasFileTransfer(const PeerLUID pluid, const FileTransferID ftid, Func&& func);

		template<typename Func>
		bool IfNotHasFileTransfer(const PeerLUID pluid, const FileTransferID ftid, Func&& func);

		bool AcceptFile(const String& filename, FileTransfer& ft);

		bool SendFileTransferStart(FileTransfer& ft);
		bool SendFileTransferCancel(FileTransfer& ft) noexcept;
		Size GetFileTransferDataSize() const noexcept;
		bool SendFileData(FileTransfer& ft);
		bool SendFileDataAck(FileTransfer& ft) noexcept;

		bool SendEcho(const PeerLUID pluid, const BufferView ping_data) noexcept;
		bool SendEchoReply(Peer& peer, const BufferView ping_data) noexcept;

	private:
		HWND m_Window{ nullptr };

		std::atomic_bool m_UseCompression{ true };
		Concurrency::Event m_ShutdownEvent;
		std::thread m_Thread;
		Peers_ThS m_Peers;

		Ping_ThS m_Ping;

		String_ThS m_AutoFileTransferPath;

		std::atomic_bool m_IsLocalBenchmarking{ false };
		std::atomic_bool m_IsPeerBenchmarking{ false };
		atomic_time_point m_LocalBenchmarkStart;
		atomic_time_point m_PeerBenchmarkStart;

		std::chrono::seconds m_MaxFileTransferInactivePeriod{ 120 };
	};
}