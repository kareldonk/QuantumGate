// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "TestExtender.h"

extern "C"
{
#include <monocypher.h>
#include <monocypher.c>
}

#include <cassert>
#include <functional>

#include "Console.h"
#include "Common\Util.h"
#include "Memory\BufferWriter.h"
#include "Memory\BufferReader.h"

using namespace QuantumGate::Implementation;
using namespace QuantumGate::Implementation::Memory;
using namespace std::literals;

namespace TestExtender
{
	FileTransfer::FileTransfer(const FileTransferType type, const Size trfbuf_size, const bool autotrf) noexcept
	{
		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();
		m_Type = type;
		m_Auto = autotrf;
		m_TransferBuffer.Allocate(trfbuf_size);
	}

	FileTransfer::FileTransfer(const FileTransferType type,
							   const FileTransferID id, const Size filesize,
							   const String& filename, Buffer&& filehash,
							   const Size trfbuf_size, const bool autotrf) noexcept
	{
		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();
		m_Type = type;
		m_Auto = autotrf;
		m_ID = id;
		m_FileSize = filesize;
		m_FileName = filename;
		m_FileHash = std::move(filehash);
		m_TransferBuffer.Allocate(trfbuf_size);
	}

	FileTransfer::~FileTransfer()
	{
		if (m_File != nullptr)
		{
			fclose(m_File);
			m_File = nullptr;
		}

		if (((m_Status != FileTransferStatus::Succeeded || IsAuto()) &&
			m_Type == FileTransferType::Incoming) &&
			!m_FileName.empty())
		{
			DeleteFile(m_FileName.c_str());
		}
	}

	const bool FileTransfer::OpenSourceFile(const String& filename)
	{
		auto success = false;

		if (fopen_s(&m_File, Util::ToStringA(filename).c_str(), "rb") == 0)
		{
			// Seek to end
			if (fseek(m_File, 0, SEEK_END) == 0)
			{
				m_FileSize = ftell(m_File);
				m_FileName = filename;

				if (CalcFileHash(m_FileHash))
				{
					// Seek to beginning
					if (fseek(m_File, 0, SEEK_SET) == 0)
					{
						m_ID = Util::PersistentHash(*Util::GetBase64(m_FileHash));
						m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();

						success = true;
					}
					else LogErr(L"Could not seek in file %s", filename.c_str());
				}
			}
			else LogErr(L"Could not seek in file %s", filename.c_str());

			if (!success)
			{
				fclose(m_File);
				m_File = nullptr;
			}
		}
		else LogErr(L"Could not open file %s", filename.c_str());

		return success;
	}

	const Size FileTransfer::ReadFromFile(Byte* buffer, Size size)
	{
		if (m_NumBytesTransferred == 0) m_TransferStartSteadyTime = Util::GetCurrentSteadyTime();

		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();
		const auto numread = fread(buffer, sizeof(Byte), size, m_File);

		if (numread < size)
		{
			if (m_NumBytesTransferred + numread != m_FileSize)
			{
				LogErr(L"Error reading file %s", m_FileName.c_str());
				SetStatus(FileTransferStatus::Error);
			}
			else
			{
				// Transfer end
				TransferEndStats();
			}
		}

		m_NumBytesTransferred += numread;

		return numread;
	}

	const bool FileTransfer::OpenDestinationFile(const String& filename)
	{
		if (fopen_s(&m_File, Util::ToStringA(filename).c_str(), "w+b") == 0)
		{
			m_FileName = filename;
			m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();

			return true;
		}

		return false;
	}

	const bool FileTransfer::WriteToFile(const Byte* buffer, const Size size)
	{
		if (m_NumBytesTransferred == 0) m_TransferStartSteadyTime = Util::GetCurrentSteadyTime();

		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();

		const auto numwritten = fwrite(buffer, sizeof(Byte), size, m_File);
		if (numwritten == size)
		{
			m_NumBytesTransferred += numwritten;

			if (m_NumBytesTransferred == m_FileSize)
			{
				// Transfer end
				TransferEndStats();

				Buffer hash;

				if (CalcFileHash(hash) && hash == m_FileHash)
				{
					SetStatus(FileTransferStatus::Succeeded);
				}
				else
				{
					LogErr(L"File transfer error: hash for file %s doesn't match", m_FileName.c_str());

					SetStatus(FileTransferStatus::Error);
					return false;
				}
			}

			return true;
		}

		SetStatus(FileTransferStatus::Error);

		return false;
	}

	void FileTransfer::SetStatus(const FileTransferStatus status) noexcept
	{
		m_Status = status;
		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();
	}

	const String FileTransfer::GetStatusString() const noexcept
	{
		switch (GetStatus())
		{
			case FileTransferStatus::NeedAccept:
				return L"Need accept";
			case FileTransferStatus::WaitingForAccept:
				return L"Waiting for accept";
			case FileTransferStatus::Transfering:
				return L"Transfering";
			case FileTransferStatus::Error:
				return L"Failed";
			case FileTransferStatus::Cancelled:
				return L"Cancelled";
			case FileTransferStatus::Succeeded:
				return L"Succeeded";
			default:
				break;
		}

		return L"Unknown";
	}

	bool FileTransfer::CalcFileHash(Buffer& hashbuff) noexcept
	{
		const Size bufsize{ 1024 * 1000 };
		std::vector<Byte> buffer(bufsize);
		Size bytesread{ 0 };

		// Seek to start
		if (fseek(m_File, 0, SEEK_SET) == 0)
		{
			LogInfo(L"Calculating hash for file %s", m_FileName.c_str());

			Buffer fhash(64);
			crypto_blake2b_ctx ctx;
			crypto_blake2b_init(&ctx);

			while (true)
			{
				const auto numread = fread(buffer.data(), sizeof(Byte), buffer.size(), m_File);

				bytesread += static_cast<unsigned int>(numread);

				crypto_blake2b_update(&ctx, reinterpret_cast<const UChar*>(buffer.data()), numread);

				if (numread < bufsize)
				{
					if (bytesread != m_FileSize)
					{
						LogErr(L"Error reading file %s", m_FileName.c_str());
					}
					else
					{
						crypto_blake2b_final(&ctx, reinterpret_cast<UChar*>(fhash.GetBytes()));

						hashbuff = std::move(fhash);
						return true;
					}

					break;
				}
			}
		}
		else LogErr(L"Could not seek in file %s", m_FileName.c_str());

		return false;
	}

	void FileTransfer::TransferEndStats() const noexcept
	{
		const auto msecs = std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() -
																				 m_TransferStartSteadyTime);
		double kbsecs = static_cast<double>(m_FileSize);

		if (msecs.count() > 0)
		{
			kbsecs = (static_cast<double>(m_FileSize) / (static_cast<double>(msecs.count()) / 1000.0)) / 1024.0;
		}

		SLogInfo(SLogFmt(FGBrightCyan) << L"Stats for filetransfer " <<
				 m_FileName << L": " << SLogFmt(FGBrightYellow) << msecs.count() << L" ms, " <<
				 SLogFmt(FGBrightCyan) << kbsecs << L" KB/s" << SLogFmt(Default));
	}

	Extender::Extender(HWND wnd) noexcept :
		QuantumGate::Extender(ExtenderUUID(L"40fcae06-d89b-0970-2e63-148521af0aac"),
							  String(L"QuantumGate Test Extender"))
	{
		m_Window = wnd;

		if (!SetStartupCallback(MakeCallback(this, &Extender::OnStartup)) ||
			!SetPostStartupCallback(MakeCallback(this, &Extender::OnPostStartup)) ||
			!SetShutdownCallback(MakeCallback(this, &Extender::OnShutdown)) ||
			!SetPeerEventCallback(MakeCallback(this, &Extender::OnPeerEvent)) ||
			!SetPeerMessageCallback(MakeCallback(this, &Extender::OnPeerMessage)))
		{
			LogErr(GetName() + L": couldn't set one or more extender callbacks");
		}
	}

	Extender::~Extender()
	{}

	const bool Extender::OnStartup()
	{
		LogDbg(L"Extender '" + GetName() + L"' starting...");

		m_ShutdownEvent.Reset();

		m_Thread = std::thread(Extender::WorkerThreadLoop, this);

		PostMessage(m_Window, WMQG_EXTENDER_INIT, 0, 0);

		// Return true if initialization was successful, otherwise return false and
		// QuantumGate won't be sending this extender any notifications
		return true;
	}

	void Extender::OnPostStartup()
	{
		LogDbg(L"Extender '" + GetName() + L"' running...");
	}

	void Extender::OnShutdown()
	{
		LogDbg(L"Extender '" + GetName() + L"' shutting down...");

		// Set the shutdown event to notify thread that we're shutting down
		m_ShutdownEvent.Set();

		if (m_Thread.joinable())
		{
			// Wait for the thread to shut down
			m_Thread.join();
		}

		m_Peers.WithUniqueLock()->clear();

		PostMessage(m_Window, WMQG_EXTENDER_DEINIT, 0, 0);
	}

	void Extender::OnPeerEvent(PeerEvent&& event)
	{
		String ev(L"Unknown");

		if (event.GetType() == PeerEventType::Connected)
		{
			ev = L"Connect";

			auto peer = std::make_unique<Peer>();
			peer->ID = event.GetPeerLUID();

			m_Peers.WithUniqueLock()->insert({ event.GetPeerLUID(), std::move(peer) });
		}
		else if (event.GetType() == PeerEventType::Disconnected)
		{
			ev = L"Disconnect";

			m_Peers.WithUniqueLock()->erase(event.GetPeerLUID());
		}

		LogInfo(L"Extender '" + GetName() + L"' got peer event: %s, Peer LUID: %llu", ev.c_str(), event.GetPeerLUID());

		if (m_Window != nullptr)
		{
			// Must be deallocated in message handler
			Event* ev = new Event({ event.GetType(), event.GetPeerLUID() });

			// Using PostMessage because the current QuantumGate worker thread should NOT be calling directly to the UI;
			// only the thread that created the Window should do that, to avoid deadlocks
			PostMessage(m_Window, WMQG_PEER_EVENT, reinterpret_cast<WPARAM>(ev), 0);
		}
	}

	const std::pair<bool, bool> Extender::OnPeerMessage(PeerEvent&& event)
	{
		auto handled = false;
		auto success = false;

		if (event.GetType() == PeerEventType::Message)
		{
			auto msgdata = event.GetMessageData();

			if (msgdata != nullptr)
			{
				UInt16 mtype = 0;
				BufferReader rdr(*msgdata, true);

				// Get message type
				if (rdr.Read(mtype))
				{
					const MessageType type = static_cast<MessageType>(mtype);
					switch (type)
					{
						case MessageType::MessageString:
						{
							handled = true;

							std::wstring str;
							str.resize((msgdata->GetSize() - sizeof(UInt16)) / sizeof(wchar_t));

							if (rdr.Read(str))
							{
								SLogInfo(L"Message from " << event.GetPeerLUID() << L": " <<
										 SLogFmt(FGBrightGreen) << str << SLogFmt(Default));

								success = true;
							}
							break;
						}
						case MessageType::BenchmarkStart:
						{
							handled = true;

							if (m_IsPeerBenchmarking)
							{
								LogErr(L"There's already a peer benchmark running");
							}
							else
							{
								m_IsPeerBenchmarking = true;
								m_PeerBenchmarkStart = std::chrono::high_resolution_clock::now();
								success = true;
							}
							break;
						}
						case MessageType::BenchmarkEnd:
						{
							handled = true;

							if (!m_IsPeerBenchmarking)
							{
								LogErr(L"There was no peer benchmark running");
							}
							else
							{
								m_IsPeerBenchmarking = false;
								auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_PeerBenchmarkStart);

								LogSys(L"Peer %s benchmark result: %dms", GetName().c_str(), ms.count());
								success = true;
							}
							break;
						}
						case MessageType::FileTransferStart:
						{
							handled = true;

							FileTransferID fid{ 0 };
							UInt64 fsize = 0;
							Buffer fhash(64);
							UInt8 autotrf{ 0 };

							if (rdr.Read(fid, fsize))
							{
								auto fsize2 = static_cast<Size>(fsize);

								// This check needed for 32-bit systems that can't support 64-bit file sizes
								if (sizeof(fsize2) != sizeof(fsize) && fsize > (std::numeric_limits<UInt32>::max)())
								{
									LogErr(L"File transfer attempt for unsupported filesize of %llu bytes", fsize);
								}
								else
								{
									String fname;

									if (rdr.Read(WithSize(fname, MaxSize::_1KB), fhash, autotrf))
									{
										Dbg(L"Received FileTransferStart message from %llu", event.GetPeerLUID());

										auto ft = std::make_unique<FileTransfer>(FileTransferType::Incoming, fid,
																				 fsize2, fname, std::move(fhash),
																				 GetFileTransferDataSize(), autotrf);
										ft->SetStatus(FileTransferStatus::NeedAccept);

										IfNotHasFileTransfer(event.GetPeerLUID(), ft->GetID(),
															 [&](FileTransfers& filetransfers)
										{
											auto retval = filetransfers.insert({ fid, std::move(ft) });
											success = true;
											auto error = false;

											if (m_Window != nullptr && autotrf == 0)
											{
												// Must be deallocated in message handler
												FileAccept* fa = new FileAccept();
												fa->PeerLUID = event.GetPeerLUID();
												fa->FileTransferID = fid;

												PostMessage(m_Window, WMQG_PEER_FILEACCEPT,
															reinterpret_cast<WPARAM>(fa), 0);
											}
											else
											{
												if (!m_AutoFileTransferPath.empty())
												{
													if (!AcceptFile(event.GetPeerLUID(),
																	m_AutoFileTransferPath + L"\\" + fname,
																	*retval.first->second))
													{
														error = true;
													}
												}
												else
												{
													LogErr(L"Auto filetransfer path not set for TestExtender");
													error = true;
												}
											}

											if (error)
											{
												LogErr(L"Couldn't accept filetransfer from %llu", event.GetPeerLUID());
											}
										});
									}
								}
							}
							break;
						}
						case MessageType::FileTransferAccept:
						{
							handled = true;

							FileTransferID ftid{ 0 };

							if (rdr.Read(ftid))
							{
								Dbg(L"Received FileTransferAccept message from %llu", event.GetPeerLUID());

								IfHasFileTransfer(event.GetPeerLUID(), ftid, [&](FileTransfer& ft)
								{
									ft.SetStatus(FileTransferStatus::Transfering);

									success = SendFileData(event.GetPeerLUID(), ft);
								});
							}
							break;
						}
						case MessageType::FileTransferCancel:
						{
							handled = true;

							FileTransferID ftid{ 0 };

							if (rdr.Read(ftid))
							{
								Dbg(L"Received FileTransferCancel message from %llu", event.GetPeerLUID());

								IfHasFileTransfer(event.GetPeerLUID(), ftid, [&](FileTransfer& ft) noexcept
								{
									ft.SetStatus(FileTransferStatus::Cancelled);
									success = true;
								});
							}
							break;
						}
						case MessageType::FileTransferData:
						{
							handled = true;

							FileTransferID ftid{ 0 };

							if (rdr.Read(ftid))
							{
								Dbg(L"Received FileTransferData message from %llu", event.GetPeerLUID());

								IfHasFileTransfer(event.GetPeerLUID(), ftid, [&](FileTransfer& ft)
								{
									auto& buffer = ft.GetTransferBuffer();

									if (rdr.Read(WithSize(buffer, GetFileTransferDataSize())))
									{
										if (ft.WriteToFile(buffer.GetBytes(), buffer.GetSize()))
										{
											success = SendFileDataAck(event.GetPeerLUID(), ft);
										}
										else
										{
											success = SendFileTransferCancel(event.GetPeerLUID(), ft);
										}
									}
								});
							}
							break;
						}
						case MessageType::FileTransferDataAck:
						{
							handled = true;

							FileTransferID ftid{ 0 };

							if (rdr.Read(ftid))
							{
								Dbg(L"Received FileTransferDataAck message from %llu", event.GetPeerLUID());

								IfHasFileTransfer(event.GetPeerLUID(), ftid, [&](FileTransfer& ft)
								{
									if (ft.GetNumBytesTransferred() == ft.GetFileSize())
									{
										ft.SetStatus(FileTransferStatus::Succeeded);
										success = true;
									}
									else success = SendFileData(event.GetPeerLUID(), ft);
								});
							}
							break;
						}
						default:
						{
							LogInfo(L"Received unknown msgtype from %llu: %u", event.GetPeerLUID(), type);
							break;
						}
					}
				}
			}
		}
		else
		{
			LogWarn(L"Opened peer event from %llu: %d", event.GetPeerLUID(), event.GetType());
		}

		return std::make_pair(handled, success);
	}

	void Extender::WorkerThreadLoop(Extender* extender)
	{
		LogDbg(L"%s worker thread %u starting", extender->GetName().c_str(), std::this_thread::get_id());

		Util::SetCurrentThreadName(extender->GetName() + L" User Thread");

		// If the shutdown event is set quit the loop
		while (!extender->m_ShutdownEvent.IsSet())
		{
			extender->m_Peers.IfSharedLock([&](auto& peers)
			{
				for (auto it = peers.begin(); it != peers.end() && !extender->m_ShutdownEvent.IsSet(); ++it)
				{
					it->second->FileTransfers.IfUniqueLock([&](FileTransfers& filetransfers)
					{
						auto fit = filetransfers.begin();

						while (fit != filetransfers.end() && !extender->m_ShutdownEvent.IsSet())
						{
							if ((Util::GetCurrentSteadyTime() - fit->second->GetLastActiveSteadyTime()) > extender->m_MaxFileTransferInactivePeriod)
							{
								LogErr(L"Filetransfer for %s inactive for too long; will remove", fit->second->GetFileName().c_str());

								fit->second->SetStatus(FileTransferStatus::Error);
							}

							if (fit->second->GetStatus() == FileTransferStatus::Succeeded)
							{
								SLogInfo(SLogFmt(FGBrightCyan) << L"File transfer for " <<
										 fit->second->GetFileName() << L" was successful" << SLogFmt(Default));

								fit = filetransfers.erase(fit);
							}
							else if (fit->second->GetStatus() == FileTransferStatus::Error ||
									 fit->second->GetStatus() == FileTransferStatus::Cancelled)
							{
								LogErr(L"File transfer for %s was unsuccessful", fit->second->GetFileName().c_str());

								fit = filetransfers.erase(fit);
							}
							else ++fit;
						}
					});
				}
			});

			// Sleep for a while or until we have to shut down
			extender->m_ShutdownEvent.Wait(10ms);
		}

		LogDbg(L"%s worker thread %u exiting", extender->GetName().c_str(), std::this_thread::get_id());
	}

	template<typename Func>
	bool Extender::IfHasFileTransfer(const PeerLUID pluid, const FileTransferID ftid, Func&& func)
	{
		auto success = false;

		m_Peers.WithSharedLock([&](auto& peers)
		{
			const auto it = peers.find(pluid);
			if (it != peers.end())
			{
				auto filetransfers = it->second->FileTransfers.WithSharedLock();
				const auto fit = filetransfers->find(ftid);
				if (fit != filetransfers->end())
				{
					func(*fit->second);
					success = true;
				}
				else LogErr(L"File transfer not found");
			}
		});

		return success;
	}

	template<typename Func>
	bool Extender::IfNotHasFileTransfer(const PeerLUID pluid, const FileTransferID ftid, Func&& func)
	{
		auto success = false;

		m_Peers.WithSharedLock([&](auto& peers)
		{
			const auto it = peers.find(pluid);
			if (it != peers.end())
			{
				it->second->FileTransfers.WithUniqueLock([&](auto& filetransfers)
				{
					const auto fit = filetransfers.find(ftid);
					if (fit == filetransfers.end())
					{
						func(filetransfers);
						success = true;
					}
					else LogErr(L"File transfer already active");
				});
			}
		});

		return success;
	}

	const bool Extender::SendBenchmarkStart(const PeerLUID pluid)
	{
		if (m_IsLocalBenchmarking)
		{
			LogErr(L"There's already a benchmark running");
			return false;
		}

		const UInt16 msgtype = static_cast<UInt16>(MessageType::BenchmarkStart);

		BufferWriter writer(true);
		if (writer.Write(msgtype))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
			{
				m_IsLocalBenchmarking = true;
				m_LocalBenchmarkStart = std::chrono::high_resolution_clock::now();

				return true;
			}

			LogErr(L"Could not send benchmark start message to peer");
		}

		return false;
	}

	const bool Extender::SendBenchmarkEnd(const PeerLUID pluid)
	{
		if (!m_IsLocalBenchmarking)
		{
			LogErr(L"There is no benchmark running");
			return false;
		}
		else
		{
			m_IsLocalBenchmarking = false;
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_LocalBenchmarkStart);

			LogSys(L"Local %s benchmark result: %dms", GetName().c_str(), ms.count());
		}

		const UInt16 msgtype = static_cast<UInt16>(MessageType::BenchmarkEnd);

		BufferWriter writer(true);
		if (writer.Write(msgtype))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded()) return true;

			LogErr(L"Could not send benchmark end message to peer");
		}

		return false;
	}

	const bool Extender::SendMessage(const PeerLUID pluid, const std::wstring& msg) const
	{
		const UInt16 msgtype = static_cast<UInt16>(MessageType::MessageString);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, msg))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded()) return true;

			LogErr(L"Could not send message to peer");
		}

		return false;
	}

	const bool Extender::SendFile(const PeerLUID pluid, const String filename, const bool autotrf)
	{
		auto success = false;

		auto it = m_Peers.WithSharedLock()->find(pluid);
		if (it != m_Peers.WithSharedLock()->end())
		{
			auto ft = std::make_unique<FileTransfer>(FileTransferType::Outgoing, GetFileTransferDataSize(), autotrf);

			if (ft->OpenSourceFile(filename))
			{
				ft->SetStatus(FileTransferStatus::WaitingForAccept);

				IfNotHasFileTransfer(pluid, ft->GetID(), [&](FileTransfers& filetransfers)
				{
					if (SendFileTransferStart(pluid, *ft))
					{
						filetransfers.insert({ ft->GetID(), std::move(ft) });

						SLogInfo(SLogFmt(FGBrightCyan) << L"Starting file transfer for file " <<
								 filename << SLogFmt(Default));

						success = true;
					}
				});
			}
			else LogErr(L"Could not open file %s", filename.c_str());
		}

		return success;
	}

	const bool Extender::AcceptFile(const PeerLUID pluid, const FileTransferID ftid, const String& filename)
	{
		auto success = false;

		IfHasFileTransfer(pluid, ftid, [&](FileTransfer& ft)
		{
			success = AcceptFile(pluid, filename, ft);
		});

		return success;
	}

	const bool Extender::AcceptFile(const PeerLUID pluid, const String& filename, FileTransfer& ft)
	{
		auto success = false;

		if (!filename.empty())
		{
			if (ft.OpenDestinationFile(filename))
			{
				const UInt16 msgtype = static_cast<UInt16>(MessageType::FileTransferAccept);

				BufferWriter writer(true);
				if (writer.WriteWithPreallocation(msgtype, ft.GetID()))
				{
					if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
					{
						ft.SetStatus(FileTransferStatus::Transfering);

						SLogInfo(SLogFmt(FGBrightCyan) << L"Starting file transfer for file " <<
								 filename << SLogFmt(Default));

						success = true;
					}
					else LogErr(L"Could not send FileTransferAccept message to peer");
				}
				else LogErr(L"Could not prepare FileTransferAccept message for peer");
			}
			else LogErr(L"Could not open file %s", filename.c_str());

			if (!success) ft.SetStatus(FileTransferStatus::Error);
		}
		else SendFileTransferCancel(pluid, ft);

		return success;
	}

	const Peers_ThS* Extender::GetPeers() const noexcept
	{
		return &m_Peers;
	}

	const bool Extender::SendFileTransferStart(const PeerLUID pluid, FileTransfer& ft)
	{
		Path fp(ft.GetFileName());
		String filename = fp.filename().wstring();

		// Filename may not be longer than 256 bytes (128 wide characters)
		filename = filename.substr(0, 128);

		const UInt16 msgtype = static_cast<const UInt16>(MessageType::FileTransferStart);
		const UInt64 filesize = static_cast<UInt64>(ft.GetFileSize());
		const UInt8 autotrf = [&]()
		{
			if (ft.IsAuto()) return 1;
			else return 0;
		}();

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, ft.GetID(), filesize,
										  WithSize(filename, MaxSize::_1KB), *ft.GetFileHash(), autotrf))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded()) return true;
			else LogErr(L"Could not send FileTransferStart message to peer");
		}
		else LogErr(L"Could not prepare FileTransferStart message for peer");

		ft.SetStatus(FileTransferStatus::Error);

		return false;
	}

	const bool Extender::SendFileTransferCancel(const PeerLUID pluid, FileTransfer& ft)
	{
		const UInt16 msgtype = static_cast<UInt16>(MessageType::FileTransferCancel);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, ft.GetID()))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded())
			{
				ft.SetStatus(FileTransferStatus::Cancelled);
				return true;
			}
			else LogErr(L"Could not send FileTransferCancel message to peer");
		}
		else LogErr(L"Could not send prepare FileTransferCancel message for peer");

		ft.SetStatus(FileTransferStatus::Error);

		return false;
	}

	const Size Extender::GetFileTransferDataSize() const noexcept
	{
		// 15 bytes for MessageType::FileTransferData message header
		return (GetMaximumMessageDataSize() - 15);
	}

	const bool Extender::SendFileData(const PeerLUID pluid, FileTransfer& ft)
	{
		auto& buffer = ft.GetTransferBuffer();
		const auto numread = ft.ReadFromFile(buffer.GetBytes(), buffer.GetSize());

		if (ft.GetStatus() != FileTransferStatus::Error)
		{
			buffer.Resize(numread);

			const UInt16 msgtype = static_cast<UInt16>(MessageType::FileTransferData);

			BufferWriter writer(true);
			if (writer.WriteWithPreallocation(msgtype, ft.GetID(), WithSize(buffer, GetFileTransferDataSize())))
			{
				if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded()) return true;
				else LogErr(L"Could not send FileTransferData message to peer");
			}
			else LogErr(L"Could not prepare FileTransferData message for peer");
		}

		ft.SetStatus(FileTransferStatus::Error);

		return false;
	}

	const bool Extender::SendFileDataAck(const PeerLUID pluid, FileTransfer& ft)
	{
		const UInt16 msgtype = static_cast<UInt16>(MessageType::FileTransferDataAck);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, ft.GetID()))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), m_UseCompression).Succeeded()) return true;
			else LogErr(L"Could not send FileTransferDataAck message to peer");
		}
		else LogErr(L"Could not prepare FileTransferDataAck message for peer");

		ft.SetStatus(FileTransferStatus::Error);

		return false;
	}
}