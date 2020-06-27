// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestExtender.h"

extern "C"
{
#include <monocypher.h>
#include <monocypher.c>
}

#include <cassert>
#include <functional>

#include <Console.h>
#include <Common\Util.h>
#include <Memory\BufferWriter.h>
#include <Memory\BufferReader.h>

using namespace QuantumGate::Implementation;
using namespace QuantumGate::Implementation::Memory;
using namespace std::literals;

namespace TestExtender
{
	FileTransfer::FileTransfer(QuantumGate::Peer& peer, const FileTransferType type, const Size trfbuf_size,
							   const bool autotrf, const bool benchmark, const Size benchmark_size) noexcept :
		m_Peer(peer), m_LastActiveSteadyTime(Util::GetCurrentSteadyTime()),	m_Type(type), m_Auto(autotrf),
		m_Benchmark(benchmark), m_BenchmarkSize(benchmark_size), m_TransferBuffer(trfbuf_size)
	{}

	FileTransfer::FileTransfer(QuantumGate::Peer& peer, const FileTransferType type, const FileTransferID id,
							   const Size filesize, const String& filename, Buffer&& filehash,
							   const Size trfbuf_size, const bool autotrf, const bool benchmark) noexcept :
		m_Peer(peer), m_LastActiveSteadyTime(Util::GetCurrentSteadyTime()), m_Type(type), m_Auto(autotrf),
		m_Benchmark(benchmark), m_ID(id), m_FileSize(filesize), m_FileName(filename), m_FileHash(std::move(filehash)),
		m_TransferBuffer(trfbuf_size)
	{}

	FileTransfer::~FileTransfer()
	{
		if (m_File != nullptr)
		{
			fclose(m_File);
			m_File = nullptr;
		}

		if (((m_Status != FileTransferStatus::Succeeded || IsAuto()) &&
			 m_Type == FileTransferType::Incoming) && !m_FileName.empty() && !m_Benchmark)
		{
			DeleteFile(m_FileName.c_str());
		}
	}

	bool FileTransfer::OpenSourceFile(const String& filename)
	{
		auto success = false;

		if (m_Benchmark)
		{
			m_FileSize = m_BenchmarkSize;
			m_FileName = filename;
			m_FileHash = Util::GetPseudoRandomBytes(64);
			m_BenchmarkBuffer = Util::GetPseudoRandomBytes(Extender::GetMaximumMessageDataSize());

			m_ID = Util::GetPersistentHash(*Util::ToBase64(m_FileHash));
			m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();

			success = true;
		}
		else
		{
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
							m_ID = Util::GetPersistentHash(*Util::ToBase64(m_FileHash));
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
		}

		return success;
	}

	Size FileTransfer::ReadFromFile(Byte* buffer, Size size) noexcept
	{
		if (m_NumBytesTransferred == 0) m_TransferStartSteadyTime = Util::GetCurrentSteadyTime();

		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();

		Size numread{ 0 };

		if (m_Benchmark)
		{
			assert(size <= m_BenchmarkBuffer.GetSize());

			numread = size;
			if (numread > (m_FileSize - m_NumBytesTransferred))
			{
				numread = m_FileSize - m_NumBytesTransferred;
			}

			std::memcpy(buffer, m_BenchmarkBuffer.GetBytes(), numread);

			if (numread < size)
			{
				// Transfer end
				TransferEndStats();
			}
		}
		else
		{
			numread = fread(buffer, sizeof(Byte), size, m_File);

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
		}

		m_NumBytesTransferred += numread;

		return numread;
	}

	bool FileTransfer::OpenDestinationFile(const String& filename)
	{
		if (!m_Benchmark)
		{
			if (fopen_s(&m_File, Util::ToStringA(filename).c_str(), "w+b") != 0)
			{
				return false;
			}
		}

		m_FileName = filename;
		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();

		return true;
	}

	bool FileTransfer::WriteToFile(const Byte* buffer, const Size size) noexcept
	{
		if (m_NumBytesTransferred == 0) m_TransferStartSteadyTime = Util::GetCurrentSteadyTime();

		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();

		if (m_Benchmark)
		{
			m_NumBytesTransferred += size;

			if (m_NumBytesTransferred == m_FileSize)
			{
				// Transfer end
				TransferEndStats();

				SetStatus(FileTransferStatus::Succeeded);
			}

			return true;
		}
		else
		{
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
	}

	void FileTransfer::SetStatus(const FileTransferStatus status) noexcept
	{
		m_Status = status;
		m_LastActiveSteadyTime = Util::GetCurrentSteadyTime();
	}

	const WChar* FileTransfer::GetStatusString() const noexcept
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
		try
		{
			constexpr Size bufsize{ 1024 * 1000 };
			Buffer buffer(bufsize);
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
					const auto numread = fread(buffer.GetBytes(), sizeof(Byte), buffer.GetSize(), m_File);

					bytesread += static_cast<Size>(numread);

					crypto_blake2b_update(&ctx, reinterpret_cast<const UChar*>(buffer.GetBytes()), numread);

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
		}
		catch (...) {}

		return false;
	}

	void FileTransfer::TransferEndStats() const noexcept
	{
		const auto msecs = std::chrono::duration_cast<std::chrono::milliseconds>(Util::GetCurrentSteadyTime() -
																				 m_TransferStartSteadyTime);
		double kbsecs = static_cast<double>(m_FileSize) / 1024.0;
		double mbitsecs = ((static_cast<double>(m_FileSize) / 1024.0) / 1024.0) * 8.0;

		if (msecs.count() > 0)
		{
			kbsecs = kbsecs / (static_cast<double>(msecs.count()) / 1000.0);
			mbitsecs = mbitsecs / (static_cast<double>(msecs.count()) / 1000.0);
		}

		SLogInfo(SLogFmt(FGBrightCyan) << L"Stats for filetransfer " << m_FileName << L": " <<
				 SLogFmt(FGBrightWhite) << m_FileSize << L" bytes" << SLogFmt(FGBrightCyan) << " in " <<
				 SLogFmt(FGBrightYellow) << msecs.count() << L" ms" << SLogFmt(FGBrightCyan) << ", " <<
				 SLogFmt(FGBrightGreen) << kbsecs << L" KB/s " << SLogFmt(FGBrightCyan) << "(" <<
				 SLogFmt(FGBrightMagenta) << mbitsecs << L" Mb/s" << SLogFmt(FGBrightCyan) << ")" << SLogFmt(Default));
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
			LogErr(L"%s: couldn't set one or more extender callbacks", GetName().c_str());
		}
	}

	Extender::~Extender()
	{}

	bool Extender::OnStartup()
	{
		LogDbg(L"Extender '%s' starting...", GetName().c_str());

		m_ShutdownEvent.Reset();

		m_Thread = std::thread(Extender::WorkerThreadLoop, this);

		if (m_Window != nullptr)
		{
			PostMessage(m_Window, static_cast<UINT>(WindowsMessage::ExtenderInit), 0, 0);
		}

		// Return true if initialization was successful, otherwise return false and
		// QuantumGate won't be sending this extender any notifications
		return true;
	}

	void Extender::OnPostStartup()
	{
		LogDbg(L"Extender '%s' running...", GetName().c_str());
	}

	void Extender::OnShutdown()
	{
		LogDbg(L"Extender '%s' shutting down...", GetName().c_str());

		// Set the shutdown event to notify thread that we're shutting down
		m_ShutdownEvent.Set();

		if (m_Thread.joinable())
		{
			// Wait for the thread to shut down
			m_Thread.join();
		}

		m_Peers.WithUniqueLock()->clear();

		if (m_Window != nullptr)
		{
			PostMessage(m_Window, static_cast<UINT>(WindowsMessage::ExtenderDeinit), 0, 0);
		}
	}

	void Extender::OnPeerEvent(PeerEvent&& event)
	{
		String ev(L"Unknown");

		if (event.GetType() == PeerEvent::Type::Connected)
		{
			ev = L"Connect";

			if (auto result = GetPeer(event.GetPeerLUID()); result.Succeeded())
			{
				auto peer = std::make_unique<PeerData>(std::move(*result));

				m_Peers.WithUniqueLock()->insert({ event.GetPeerLUID(), std::move(peer) });
			}
		}
		else if (event.GetType() == PeerEvent::Type::Disconnected)
		{
			ev = L"Disconnect";

			m_Peers.WithUniqueLock()->erase(event.GetPeerLUID());
		}

		LogInfo(L"Extender '%s' got peer event: %s, Peer LUID: %llu",
				GetName().c_str(), ev.c_str(), event.GetPeerLUID());

		if (m_Window != nullptr)
		{
			// Must be deallocated in message handler
			Event* ev = new Event({ event.GetType(), event.GetPeerLUID() });

			// Using PostMessage because the current QuantumGate worker thread should NOT be calling directly to the UI;
			// only the thread that created the Window should do that, to avoid deadlocks
			if (!PostMessage(m_Window, static_cast<UINT>(WindowsMessage::PeerEvent), reinterpret_cast<WPARAM>(ev), 0))
			{
				delete ev;
			}
		}
	}

	QuantumGate::Extender::PeerEvent::Result Extender::OnPeerMessage(PeerEvent&& event)
	{
		PeerEvent::Result result;

		if (event.GetType() == PeerEvent::Type::Message)
		{
			auto msgdata = event.GetMessageData();
			if (msgdata != nullptr)
			{
				UInt16 mtype = 0;
				BufferReader rdr(*msgdata, true);

				// Get message type
				if (rdr.Read(mtype))
				{
					const auto type = static_cast<MessageType>(mtype);
					switch (type)
					{
						case MessageType::MessageString:
						{
							result.Handled = true;

							String str;
							str.resize((msgdata->GetSize() - sizeof(UInt16)) / sizeof(String::value_type));

							if (rdr.Read(str))
							{
								SLogInfo(L"Message from " << event.GetPeerLUID() << L": " <<
										 SLogFmt(FGBrightGreen) << str << SLogFmt(Default));

								result.Success = true;
							}
							break;
						}
						case MessageType::BenchmarkStart:
						{
							result.Handled = true;

							if (m_IsPeerBenchmarking)
							{
								LogErr(L"There's already a peer benchmark running");
							}
							else
							{
								m_IsPeerBenchmarking = true;
								m_PeerBenchmarkStart = std::chrono::high_resolution_clock::now();
								result.Success = true;
							}
							break;
						}
						case MessageType::BenchmarkEnd:
						{
							result.Handled = true;

							if (!m_IsPeerBenchmarking)
							{
								LogErr(L"There was no peer benchmark running");
							}
							else
							{
								m_IsPeerBenchmarking = false;
								auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_PeerBenchmarkStart.load());

								LogSys(L"Peer %s benchmark result: %dms", GetName().c_str(), ms.count());
								result.Success = true;
							}
							break;
						}
						case MessageType::FileTransferStart:
						{
							result.Handled = true;

							FileTransferID fid{ 0 };
							UInt64 fsize = 0;
							Buffer fhash(64);
							UInt8 autotrf{ 0 };
							UInt8 benchmark{ 0 };

							if (rdr.Read(fid, fsize))
							{
								const auto fsize2 = static_cast<Size>(fsize);

								// This check needed for 32-bit systems that can't support 64-bit file sizes
								if (sizeof(fsize2) != sizeof(fsize) && fsize > (std::numeric_limits<UInt32>::max)())
								{
									LogErr(L"File transfer attempt for unsupported filesize of %llu bytes", fsize);
								}
								else
								{
									String fname;

									if (rdr.Read(WithSize(fname, MaxSize::_1KB), fhash, autotrf, benchmark))
									{
										Dbg(L"Received FileTransferStart message from %llu", event.GetPeerLUID());

										IfNotHasFileTransfer(event.GetPeerLUID(), fid,
															 [&](QuantumGate::Peer& peer, FileTransfers& filetransfers)
										{
											auto ft = std::make_unique<FileTransfer>(peer, FileTransferType::Incoming, fid,
																					 fsize2, fname, std::move(fhash),
																					 GetFileTransferDataSize(), autotrf,
																					 benchmark);
											ft->SetStatus(FileTransferStatus::NeedAccept);

											const auto retval = filetransfers.insert({ fid, std::move(ft) });
											result.Success = true;
											auto error = false;

											if (m_Window != nullptr && autotrf == 0 && benchmark == 0)
											{
												// Must be deallocated in message handler
												FileAccept* fa = new FileAccept();
												fa->PeerLUID = peer.GetLUID();
												fa->FileTransferID = fid;

												if (!PostMessage(m_Window, static_cast<UINT>(WindowsMessage::FileAccept),
																 reinterpret_cast<WPARAM>(fa), 0))
												{
													delete fa;
												}
											}
											else if (autotrf == 1)
											{
												auto filepath = m_AutoFileTransferPath.WithSharedLock();
												if (!filepath->empty())
												{
													if (benchmark == 0)
													{
														// Random temporary filename because the file
														// will get deleted after completion anyway and
														// to reduce conflicts with multiple transfers
														fname = *filepath + Util::FormatString(L"%llu.tmp",
																							   Util::GetPseudoRandomNumber());
													}

													if (!AcceptFile(fname, *retval.first->second))
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
							result.Handled = true;

							FileTransferID ftid{ 0 };

							if (rdr.Read(ftid))
							{
								Dbg(L"Received FileTransferAccept message from %llu", event.GetPeerLUID());

								IfHasFileTransfer(event.GetPeerLUID(), ftid, [&](FileTransfer& ft)
								{
									ft.SetStatus(FileTransferStatus::Transfering);

									result.Success = SendFileData(ft);
								});
							}
							break;
						}
						case MessageType::FileTransferCancel:
						{
							result.Handled = true;

							FileTransferID ftid{ 0 };

							if (rdr.Read(ftid))
							{
								Dbg(L"Received FileTransferCancel message from %llu", event.GetPeerLUID());

								IfHasFileTransfer(event.GetPeerLUID(), ftid, [&](FileTransfer& ft) noexcept
								{
									ft.SetStatus(FileTransferStatus::Cancelled);
									result.Success = true;
								});
							}
							break;
						}
						case MessageType::FileTransferData:
						{
							result.Handled = true;

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
											result.Success = SendFileDataAck(ft);
										}
										else
										{
											result.Success = SendFileTransferCancel(ft);
										}
									}
								});
							}
							break;
						}
						case MessageType::FileTransferDataAck:
						{
							result.Handled = true;

							FileTransferID ftid{ 0 };

							if (rdr.Read(ftid))
							{
								Dbg(L"Received FileTransferDataAck message from %llu", event.GetPeerLUID());

								IfHasFileTransfer(event.GetPeerLUID(), ftid, [&](FileTransfer& ft)
								{
									if (ft.GetNumBytesTransferred() == ft.GetFileSize())
									{
										ft.SetStatus(FileTransferStatus::Succeeded);
										result.Success = true;
									}
									else result.Success = SendFileData(ft);
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
			LogWarn(L"Unknown peer event from %llu: %d", event.GetPeerLUID(), event.GetType());
		}

		return result;
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
						func(it->second->Peer, filetransfers);
						success = true;
					}
					else LogErr(L"File transfer already active");
				});
			}
		});

		return success;
	}

	bool Extender::SendBenchmarkStart(const PeerLUID pluid) noexcept
	{
		if (m_IsLocalBenchmarking)
		{
			LogErr(L"There's already a benchmark running");
			return false;
		}

		constexpr UInt16 msgtype = static_cast<UInt16>(MessageType::BenchmarkStart);

		BufferWriter writer(true);
		if (writer.Write(msgtype))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(), QuantumGate::SendParameters{ .Compress = m_UseCompression }).Succeeded())
			{
				m_IsLocalBenchmarking = true;
				m_LocalBenchmarkStart = std::chrono::high_resolution_clock::now();

				return true;
			}

			LogErr(L"Could not send benchmark start message to peer");
		}

		return false;
	}

	bool Extender::SendBenchmarkEnd(const PeerLUID pluid) noexcept
	{
		if (!m_IsLocalBenchmarking)
		{
			LogErr(L"There is no benchmark running");
			return false;
		}
		else
		{
			m_IsLocalBenchmarking = false;
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_LocalBenchmarkStart.load());

			LogSys(L"Local %s benchmark result: %dms", GetName().c_str(), ms.count());
		}

		constexpr UInt16 msgtype = static_cast<UInt16>(MessageType::BenchmarkEnd);

		BufferWriter writer(true);
		if (writer.Write(msgtype))
		{
			if (SendMessageTo(pluid, writer.MoveWrittenBytes(),
							  QuantumGate::SendParameters{ .Compress = m_UseCompression }).Succeeded()) return true;

			LogErr(L"Could not send benchmark end message to peer");
		}

		return false;
	}

	bool Extender::SendMessage(const PeerLUID pluid, const String& msg, const SendParameters::PriorityOption priority,
							   const std::chrono::milliseconds delay) const
	{
		constexpr UInt16 msgtype = static_cast<UInt16>(MessageType::MessageString);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, msg))
		{
			SendParameters params;
			params.Compress = m_UseCompression;
			params.Priority = priority;
			params.Delay = delay;

			const auto result = SendMessageTo(pluid, writer.MoveWrittenBytes(), params);
			if (result.Succeeded()) return true;
			else
			{
				LogErr(L"Could not send message to peer: %s", result.GetErrorDescription().c_str());
			}
		}

		return false;
	}

	bool Extender::SendFile(const PeerLUID pluid, const String filename, const bool autotrf,
							const bool benchmark, const Size benchmark_size)
	{
		auto success = false;

		auto peers = m_Peers.WithSharedLock();
		const auto it = peers->find(pluid);
		if (it != peers->end())
		{
			auto ft = std::make_unique<FileTransfer>(it->second->Peer, FileTransferType::Outgoing,
													 GetFileTransferDataSize(), autotrf, benchmark, benchmark_size);

			// Need to unlock because IfNotHasFileTransfer will
			// lock again later
			peers.UnlockShared();

			if (ft->OpenSourceFile(filename))
			{
				ft->SetStatus(FileTransferStatus::WaitingForAccept);

				IfNotHasFileTransfer(pluid, ft->GetID(), [&](QuantumGate::Peer& ftpeer, FileTransfers& filetransfers)
				{
					if (SendFileTransferStart(*ft))
					{
						filetransfers.insert({ ft->GetID(), std::move(ft) });

						SLogInfo(SLogFmt(FGBrightCyan) << L"Starting file transfer for " <<
								 filename << SLogFmt(Default));

						success = true;
					}
				});
			}
			else LogErr(L"Could not open file %s", filename.c_str());
		}

		return success;
	}

	bool Extender::AcceptFile(const PeerLUID pluid, const FileTransferID ftid, const String& filename)
	{
		auto success = false;

		IfHasFileTransfer(pluid, ftid, [&](FileTransfer& ft)
		{
			success = AcceptFile(filename, ft);
		});

		return success;
	}

	bool Extender::AcceptFile(const String& filename, FileTransfer& ft)
	{
		auto success = false;

		if (!filename.empty())
		{
			if (ft.OpenDestinationFile(filename))
			{
				constexpr UInt16 msgtype = static_cast<UInt16>(MessageType::FileTransferAccept);

				BufferWriter writer(true);
				if (writer.WriteWithPreallocation(msgtype, ft.GetID()))
				{
					if (SendMessageTo(ft.GetPeer(), writer.MoveWrittenBytes(),
									  QuantumGate::SendParameters{ .Compress = m_UseCompression }).Succeeded())
					{
						ft.SetStatus(FileTransferStatus::Transfering);

						SLogInfo(SLogFmt(FGBrightCyan) << L"Starting file transfer for " <<
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
		else SendFileTransferCancel(ft);

		return success;
	}

	const Peers_ThS* Extender::GetPeers() const noexcept
	{
		return &m_Peers;
	}

	bool Extender::SendFileTransferStart(FileTransfer& ft)
	{
		Path fp{ ft.GetFileName() };
		String filename{ fp.filename().wstring().c_str() };

		// Filename may not be longer than 256 bytes (128 wide characters)
		filename = filename.substr(0, 128);

		constexpr UInt16 msgtype = static_cast<const UInt16>(MessageType::FileTransferStart);
		const UInt64 filesize = static_cast<UInt64>(ft.GetFileSize());

		const UInt8 autotrf = [&]()
		{
			if (ft.IsAuto()) return 1;
			else return 0;
		}();
		
		const UInt8 benchmark = [&]()
		{
			if (ft.IsBenchmark()) return 1;
			else return 0;
		}();

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, ft.GetID(), filesize, WithSize(filename, MaxSize::_1KB),
										  *ft.GetFileHash(), autotrf, benchmark))
		{
			if (SendMessageTo(ft.GetPeer(), writer.MoveWrittenBytes(),
							  QuantumGate::SendParameters{ .Compress = m_UseCompression }).Succeeded()) return true;
			else LogErr(L"Could not send FileTransferStart message to peer");
		}
		else LogErr(L"Could not prepare FileTransferStart message for peer");

		ft.SetStatus(FileTransferStatus::Error);

		return false;
	}

	bool Extender::SendFileTransferCancel(FileTransfer& ft) noexcept
	{
		constexpr UInt16 msgtype = static_cast<UInt16>(MessageType::FileTransferCancel);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, ft.GetID()))
		{
			if (SendMessageTo(ft.GetPeer(), writer.MoveWrittenBytes(),
							  QuantumGate::SendParameters{ .Compress = m_UseCompression }).Succeeded())
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

	Size Extender::GetFileTransferDataSize() const noexcept
	{
		// 15 bytes for MessageType::FileTransferData message header
		return (GetMaximumMessageDataSize() - 15);
	}

	bool Extender::SendFileData(FileTransfer& ft)
	{
		auto& buffer = ft.GetTransferBuffer();
		const auto numread = ft.ReadFromFile(buffer.GetBytes(), buffer.GetSize());

		if (ft.GetStatus() != FileTransferStatus::Error)
		{
			buffer.Resize(numread);

			constexpr UInt16 msgtype = static_cast<UInt16>(MessageType::FileTransferData);

			BufferWriter writer(true);
			if (writer.WriteWithPreallocation(msgtype, ft.GetID(), WithSize(buffer, GetFileTransferDataSize())))
			{
				if (SendMessageTo(ft.GetPeer(), writer.MoveWrittenBytes(),
								  QuantumGate::SendParameters{ .Compress = m_UseCompression }).Succeeded()) return true;
				else LogErr(L"Could not send FileTransferData message to peer");
			}
			else LogErr(L"Could not prepare FileTransferData message for peer");
		}

		ft.SetStatus(FileTransferStatus::Error);

		return false;
	}

	bool Extender::SendFileDataAck(FileTransfer& ft) noexcept
	{
		constexpr UInt16 msgtype = static_cast<UInt16>(MessageType::FileTransferDataAck);

		BufferWriter writer(true);
		if (writer.WriteWithPreallocation(msgtype, ft.GetID()))
		{
			if (SendMessageTo(ft.GetPeer(), writer.MoveWrittenBytes(),
							  QuantumGate::SendParameters{ .Compress = m_UseCompression }).Succeeded()) return true;
			else LogErr(L"Could not send FileTransferDataAck message to peer");
		}
		else LogErr(L"Could not prepare FileTransferDataAck message for peer");

		ft.SetStatus(FileTransferStatus::Error);

		return false;
	}
}