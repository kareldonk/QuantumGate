// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "QuantumGate.h"

using namespace QuantumGate;

class Benchmarks final
{
private:
	Benchmarks() noexcept = default;

public:
	int TestMemberFunction(int number);

	template<typename Func>
	static std::chrono::microseconds DoBenchmark(const std::wstring & desc, unsigned int numtries, Func && func);

	static void BenchmarkThreadLocalCache();
	static void BenchmarkCallbacks();
	static void BenchmarkThreadPause();
	static void BenchmarkMutexes();
	static void BenchmarkCompression();
	static void BenchmarkConsole();
	static void BenchmarkMemory();
};

