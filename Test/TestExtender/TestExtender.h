// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <unordered_map>
#include <chrono>

#include <QuantumGate.h>
#include <Concurrency\ThreadSafe.h>
#include <Concurrency\EventCondition.h>

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
		FileTransferCancel
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
		FileTransfer(const FileTransferType type, const Size trfbuf_size, const bool autotrf) noexcept;
		FileTransfer(const FileTransferType type, const FileTransferID id,
					 const Size filesize, const String& filename, Buffer&& filehash,
					 const Size trfbuf_size, const bool autotrf) noexcept;
		~FileTransfer();

		const bool OpenSourceFile(const String& filename);
		const Size ReadFromFile(Byte* buffer, Size size);
		const bool OpenDestinationFile(const String& filename);
		const bool WriteToFile(const Byte* buffer, const Size size);

		void SetStatus(const FileTransferStatus status) noexcept;

		inline const FileTransferStatus GetStatus() const noexcept { return m_Status; }
		const WChar* GetStatusString() const noexcept;
		inline const FileTransferType GetType() const noexcept { return m_Type; }
		inline const bool IsAuto() const noexcept { return m_Auto; }
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
		FileTransferType m_Type{ FileTransferType::Unknown };
		FileTransferStatus m_Status{ FileTransferStatus::Unknown };
		bool m_Auto{ false };
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

	struct Peer final
	{
		Peer(const PeerLUID pluid) noexcept : ID(pluid) {}

		const PeerLUID ID{ 0 };
		FileTransfers_ThS FileTransfers;
	};

	using Peers = std::unordered_map<PeerLUID, std::unique_ptr<Peer>>;
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
		ExtenderDeinit = WM_USER + 4
	};

	struct Event final
	{
		PeerEventType Type{ PeerEventType::Unknown };
		PeerLUID PeerLUID{ 0 };
	};

	class Extender final : public QuantumGate::Extender
	{
		using String_ThS = Concurrency::ThreadSafe<String, std::shared_mutex>;
		using atomic_time_point = std::atomic<std::chrono::time_point<std::chrono::high_resolution_clock>>;

	public:
		Extender(HWND wnd) noexcept;
		virtual ~Extender();

		inline void SetAutoFileTransferPath(const String& path) { m_AutoFileTransferPath.WithUniqueLock() = path; }
		inline void SetUseCompression(const bool compression) noexcept { m_UseCompression = compression; }
		inline const bool IsUsingCompression() const noexcept { return m_UseCompression; }

		const bool SendMessage(const PeerLUID pluid, const std::wstring& msg) const;

		const bool SendBenchmarkStart(const PeerLUID pluid);
		const bool SendBenchmarkEnd(const PeerLUID pluid);

		const bool SendFile(const PeerLUID pluid, const String filename, const bool autotrf);
		const bool AcceptFile(const PeerLUID pluid, const FileTransferID ftid, const String& filename);

		const Peers_ThS* GetPeers() const noexcept;

	protected:
		const bool OnStartup();
		void OnPostStartup();
		void OnShutdown();
		void OnPeerEvent(PeerEvent&& event);
		const std::pair<bool, bool> OnPeerMessage(PeerEvent&& event);

	private:
		static void WorkerThreadLoop(Extender* extender);

		template<typename Func>
		bool IfHasFileTransfer(const PeerLUID pluid, const FileTransferID ftid, Func&& func);

		template<typename Func>
		bool IfNotHasFileTransfer(const PeerLUID pluid, const FileTransferID ftid, Func&& func);

		const bool AcceptFile(const PeerLUID pluid, const String& filename, FileTransfer& ft);

		const bool SendFileTransferStart(const PeerLUID pluid, FileTransfer& ft);
		const bool SendFileTransferCancel(const PeerLUID pluid, FileTransfer& ft);
		const Size GetFileTransferDataSize() const noexcept;
		const bool SendFileData(const PeerLUID pluid, FileTransfer& ft);
		const bool SendFileDataAck(const PeerLUID pluid, FileTransfer& ft);

	private:
		HWND m_Window{ nullptr };

		std::atomic_bool m_UseCompression{ true };
		Concurrency::EventCondition m_ShutdownEvent{ false };
		std::thread m_Thread;
		Peers_ThS m_Peers;

		String_ThS m_AutoFileTransferPath;

		std::atomic_bool m_IsLocalBenchmarking{ false };
		std::atomic_bool m_IsPeerBenchmarking{ false };
		atomic_time_point m_LocalBenchmarkStart;
		atomic_time_point m_PeerBenchmarkStart;

		std::chrono::seconds m_MaxFileTransferInactivePeriod{ 120 };
	};
}