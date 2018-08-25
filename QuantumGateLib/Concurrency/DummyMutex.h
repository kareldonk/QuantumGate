// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Concurrency
{
	class DummyMutex
	{
	public:
		constexpr DummyMutex() noexcept {}
		DummyMutex(const DummyMutex&) = delete;
		DummyMutex(DummyMutex&&) = delete;
		~DummyMutex() = default;
		DummyMutex& operator=(const DummyMutex&) = delete;
		DummyMutex& operator=(DummyMutex&&) = delete;

		constexpr void lock() noexcept {}
		constexpr void unlock() noexcept {}
	};
}