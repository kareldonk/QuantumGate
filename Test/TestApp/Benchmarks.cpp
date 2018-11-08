// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Benchmarks.h"

#include <thread>
#include <chrono>

#include "Console.h"
#include "Common\Util.h"
#include "Common\Callback.h"
#include "Settings.h"
#include "Concurrency\ThreadLocalCache.h"
#include "Concurrency\RecursiveSharedMutex.h"
#include "Concurrency\SpinMutex.h"
#include "Concurrency\SharedSpinMutex.h"
#include "Compression\Compression.h"

using namespace QuantumGate::Implementation;
using namespace QuantumGate::Implementation::Concurrency;
using namespace std::literals;

int TestFunction(int n)
{
	if (n > 1) return n * TestFunction(n - 1);
	else return 1;
}

int Benchmarks::TestMemberFunction(int n)
{
	if (n > 1) return n * TestFunction(n - 1);
	else return 1;
}

template <typename Func>
std::chrono::microseconds Benchmarks::DoBenchmark(const std::wstring& desc, unsigned int numtries, Func&& func)
{
	const auto begin = std::chrono::high_resolution_clock::now();

	for (auto x = 0u; x < numtries; ++x)
	{
		func();
	}

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);
	LogSys(L"Benchmark '%s' result: %dms", desc.c_str(), ms.count());

	return ms;
}

void Benchmarks::BenchmarkThreadLocalCache()
{
	const auto maxtr = 50000000u;

	LogSys(L"---");
	LogSys(L"Starting ThreadLocalCache benchmark for %u iterations", maxtr);

	Settings settings;

	DoBenchmark(std::wstring(L"Settings as normal variable"), maxtr, [&]()
	{
		const auto& settingsv = settings;

		volatile const auto val = settings.Local.Concurrency.MinThreadPools;
		volatile const auto val2 = settings.Local.Concurrency.WorkerThreadsMaxBurst;
		volatile auto val3 = val * val2;
	});

	ThreadLocalSettings<1> settingstl;

	DoBenchmark(std::wstring(L"Settings thread local cache"), maxtr, [&]()
	{
		const auto& settingstlv = settingstl.GetCache();

		volatile const auto val = settingstlv.Local.Concurrency.MinThreadPools;
		volatile const auto val2 = settingstlv.Local.Concurrency.WorkerThreadsMaxBurst;
		volatile auto val3 = val * val2;
	});
}

void Benchmarks::BenchmarkCallbacks()
{
	const auto maxtr = 50000000u;

	LogSys(L"---");
	LogSys(L"Starting Callbacks benchmark for %u iterations", maxtr);
	
	auto dg1 = Callback<int(int)>(&TestFunction);
	DoBenchmark(std::wstring(L"Callback free function"), maxtr, [&]()
	{
		dg1(10);
	});

	auto fu1 = std::function<int(int)>(&TestFunction);
	DoBenchmark(std::wstring(L"std::function free function"), maxtr, [&]()
	{
		fu1(10);
	});

	LogWarn(L"Callback size: %d bytes / std::function size: %d bytes", sizeof(dg1), sizeof(fu1));

	auto dg2 = Callback<int(int)>([&](int ab) mutable -> int {
		return TestFunction(ab);
	});
	DoBenchmark(std::wstring(L"Callback lambda function"), maxtr, [&]()
	{
		dg2(10);
	});

	auto fu2 = std::function<int(int)>([&](int ab) -> int {
		return TestFunction(ab);
	});
	DoBenchmark(std::wstring(L"std::function lambda function"), maxtr, [&]()
	{
		fu2(10);
	});

	LogWarn(L"Callback size: %d bytes / std::function size: %d bytes", sizeof(dg2), sizeof(fu2));

	Benchmarks bm;
	auto dg3 = Callback<int(int)>(&bm, &Benchmarks::TestMemberFunction);
	DoBenchmark(std::wstring(L"Callback member function"), maxtr, [&]()
	{
		dg3(10);
	});

	std::function<int(int)> fu3 = std::bind(&Benchmarks::TestMemberFunction, &bm, std::placeholders::_1);
	DoBenchmark(std::wstring(L"std::function member function"), maxtr, [&]()
	{
		fu3(10);
	});

	LogWarn(L"Callback size: %d bytes / std::function size: %d bytes", sizeof(dg3), sizeof(fu3));

	DoBenchmark(std::wstring(L"Callback free function (create and execute)"), maxtr, [&]()
	{
		auto dg4 = Callback<int(int)>(&TestFunction);
		dg4(10);
	});

	DoBenchmark(std::wstring(L"std::function free function (create and execute)"), maxtr, [&]()
	{
		auto fu4 = std::function<int(int)>(&TestFunction);
		fu4(10);
	});

	DoBenchmark(std::wstring(L"Callback lambda function (create and execute)"), maxtr, [&]()
	{
		auto dg5 = Callback<int(int)>([&](int ab) mutable -> int {
			return TestFunction(ab);
		});
		dg5(10);
	});

	DoBenchmark(std::wstring(L"std::function lambda function (create and execute)"), maxtr, [&]()
	{
		auto fu5 = std::function<int(int)>([&](int ab) -> int {
			return TestFunction(ab);
		});
		fu5(10);
	});
}

void Benchmarks::BenchmarkMutexes()
{
	const auto maxtr = 10000000u;

	LogSys(L"---");
	LogSys(L"Starting mutexes benchmark for %u iterations", maxtr);

	std::atomic_int atint = 0;
	volatile int num = 0;
	DoBenchmark(std::wstring(L"atomic int store"), maxtr, [&]()
	{
		atint.store(num);
	});

	std::atomic_uint64_t atint64 = 0u;
	volatile UInt64 num64 = 0u;
	DoBenchmark(std::wstring(L"atomic UInt64 store"), maxtr, [&]()
	{
		atint64.store(num64);
	});

	DoBenchmark(std::wstring(L"atomic UInt64 load and store"), maxtr, [&]()
	{
		num64 = atint64.load();
		atint64.store(num64);
	});

	std::atomic_bool atbool = true;
	volatile bool flag = true;
	DoBenchmark(std::wstring(L"atomic bool load"), maxtr, [&]()
	{
		flag = atbool.load();
	});

	std::mutex mtx;
	DoBenchmark(std::wstring(L"std::mutex unique"), maxtr, [&]()
	{
		mtx.lock();
		mtx.unlock();
	});

	std::shared_mutex smtx;
	DoBenchmark(std::wstring(L"std::shared_mutex unique"), maxtr, [&]()
	{
		smtx.lock();
		smtx.unlock();
	});

	DoBenchmark(std::wstring(L"std::shared_mutex shared"), maxtr, [&]()
	{
		smtx.lock_shared();
		smtx.unlock_shared();
	});

	std::recursive_mutex rmtx;
	DoBenchmark(std::wstring(L"std::recursive_mutex unique"), maxtr, [&]()
	{
		rmtx.lock();
		rmtx.unlock();
	});

	RecursiveSharedMutex qgmtx;
	DoBenchmark(std::wstring(L"RecursiveSharedMutex unique"), maxtr, [&]()
	{
		qgmtx.lock();
		qgmtx.unlock();
	});

	DoBenchmark(std::wstring(L"RecursiveSharedMutex shared"), maxtr, [&]()
	{
		qgmtx.lock_shared();
		qgmtx.unlock_shared();
	});

	SpinMutex spmtx;
	DoBenchmark(std::wstring(L"SpinMutex unique"), maxtr, [&]()
	{
		spmtx.lock();
		spmtx.unlock();
	});

	SharedSpinMutex sqgmtx;
	DoBenchmark(std::wstring(L"SharedSpinMutex unique"), maxtr, [&]()
	{
		sqgmtx.lock();
		sqgmtx.unlock();
	});

	DoBenchmark(std::wstring(L"SharedSpinMutex shared"), maxtr, [&]()
	{
		sqgmtx.lock_shared();
		sqgmtx.unlock_shared();
	});
}

void Benchmarks::BenchmarkCompression()
{
	const auto maxtr = 50000u;

	LogSys(L"---");
	LogSys(L"Starting Compression benchmark for %u iterations", maxtr);

	std::array<std::wstring, 4> comprbuf = {
		L"Hello world",
		L"Hello world, Hello world",
		L"Nothing is impossible, that is possible. Nothing is possible, that is impossible.",
		L"\"Sexual suppression supports the power of the Church, which has sunk very deep roots \
		into the exploited masses by means of sexual anxiety and guilt. It engenders timidity \
		towards authority and binds children to their parents. This results in adult subservience \
		to state authority and to capitalistic exploitation. It paralyzes the intellectual critical \
		powers of the oppressed masses because it consumes the greater part of biological energy. \
		Finally, it paralyzes the resolute development of creative forces and renders impossible the \
		achievement of all aspirations for human freedom. In this way the prevailing economic system \
		(in which single individuals can easily rule entire masses) becomes rooted in the psychic \
		structures of the oppressed themselves.\" - Wilhelm Reich\r\n\r\n \
		\"Geldings, any farmer will tell you, are easier to control than stallions.The first governments, \
		which were frankly slave - states, inculcated sexual repression for precisely this reason. \
		[...] We are now able to understand the two great mysteries of social behavior : why sexual \
		repression is accepted and why government is accepted, when the first diminishes joy and the \
		second is leading obviously to the destruction of the species. [...] \
		The unrepressed man of the future — if there is a future — will look back at our age and \
		wonder how we survived without all landing in the madhouse.That so many of us do land in \
		madhouses will be accepted as the natural consequence of(sexually) repressed civilization.\" \
		- Robert Anton Wilson" 
	};

	for (const auto& txt : comprbuf)
	{
		Buffer inbuf(txt.size());
		memcpy(inbuf.GetBytes(), txt.data(), txt.size());

		Buffer zloutbuf, zstdoutbuf;

		LogSys(L"---");
		LogSys(L"Input size: %u bytes", inbuf.GetSize());

		DoBenchmark(std::wstring(L"Compression using Zlib"), maxtr, [&]()
		{
			if (!Compression::Compress(inbuf, zloutbuf, Algorithm::Compression::DEFLATE))
			{
				AfxMessageBox(L"Compression failed!");
				throw;
			}
		});

		DoBenchmark(std::wstring(L"Decompression using Zlib"), maxtr, [&]()
		{
			if (!Compression::Decompress(zloutbuf, inbuf, Algorithm::Compression::DEFLATE))
			{
				AfxMessageBox(L"Decompression failed!");
				throw;
			}
		});

		DoBenchmark(std::wstring(L"Compression using Zstd"), maxtr, [&]()
		{
			if (!Compression::Compress(inbuf, zstdoutbuf, Algorithm::Compression::ZSTANDARD))
			{
				AfxMessageBox(L"Compression failed!");
				throw;
			}
		});

		DoBenchmark(std::wstring(L"Decompression using Zstd"), maxtr, [&]()
		{
			if (!Compression::Decompress(zstdoutbuf, inbuf, Algorithm::Compression::ZSTANDARD))
			{
				AfxMessageBox(L"Decompression failed!");
				throw;
			}
		});

		LogSys(L"Zlib compression output size: %u", zloutbuf.GetSize());
		LogSys(L"Zstd compression output size: %u", zstdoutbuf.GetSize());
	}
}

void Benchmarks::BenchmarkConsole()
{
	const auto maxtr = 50000u;

	const auto dur1 = DoBenchmark(std::wstring(L"Adding to console using AddMessage"), maxtr, [&]()
	{
		LogInfo(L"This is a test message");
	});

	const auto dur2 = DoBenchmark(std::wstring(L"Adding to console using Log"), maxtr, [&]()
	{
		SLogInfo(L"This is a test message");
	});

	const auto number = 3000ull;

	const auto dur3 = DoBenchmark(std::wstring(L"Adding to console using AddMessage"), maxtr, [&]()
	{
		LogInfo(L"This is a test message %llu", number);
	});

	const auto dur4 = DoBenchmark(std::wstring(L"Adding to console using Log"), maxtr, [&]()
	{
		SLogInfo(L"This is a test message " << number);
	});

	LogSys(L"Benchmark results: %dms / %dms | %dms / %dms",
		   std::chrono::duration_cast<std::chrono::milliseconds>(dur1).count(),
		   std::chrono::duration_cast<std::chrono::milliseconds>(dur2).count(),
		   std::chrono::duration_cast<std::chrono::milliseconds>(dur3).count(),
		   std::chrono::duration_cast<std::chrono::milliseconds>(dur4).count());
}

void Benchmarks::BenchmarkMemory()
{
	const auto maxtr = 2000u;

	LogSys(L"---");
	LogSys(L"Starting Memory benchmark for %u iterations", maxtr);

	using namespace QuantumGate::Implementation::Memory;

	auto len = 16u;

	while (true)
	{
		LogSys(L"\r\nAllocation of %llu bytes:", len);

		DoBenchmark(std::wstring(L"Free Allocator"), maxtr, [&]()
		{
			using FBuffer = BufferImpl<>;

			FBuffer buf;
			buf.Allocate(len);
			buf.Clear();
			buf.FreeUnused();
		});

		DoBenchmark(std::wstring(L"Pool Allocator"), maxtr, [&]()
		{
			using PBuffer = BufferImpl<PoolAllocator<Byte>>;

			PBuffer buf;
			buf.Allocate(len);
			buf.Clear();
			buf.FreeUnused();
		});

		len *= 2;
		if (len > 3000000) break;
	}
}