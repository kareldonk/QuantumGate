// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

// If NO_PCG_RANDOM is defined during compile time then PCG won't be used;
// instead the Mersenne Twister engine from std will be used (see further below)
#ifndef NO_PCG_RANDOM
#pragma message("Using PCG as random number generator")
#include <pcg_random.hpp>
#elif
#pragma message("Using Mersenne Twister 19937 as random number generator (slower!)")
#include <random>
#endif

namespace QuantumGate::Implementation
{
	class Obfuscate final
	{
	private:
#ifndef NO_PCG_RANDOM
		using Rng64Alg = pcg64_fast;
#else
		using Rng64Alg = std::mt19937_64;
#endif

		Obfuscate() noexcept = default;

	public:
		inline static void Do(BufferSpan& data, const BufferView& key, const UInt32 iv) noexcept
		{
			assert(key.GetSize() == sizeof(UInt64));

			const auto key64 = reinterpret_cast<const UInt64*>(key.GetBytes());
			const auto iv64 = static_cast<UInt64>(iv) | (static_cast<UInt64>(iv) << 32);
			const auto keyiv64 = *key64 ^ iv64;

			Rng64Alg rng64(keyiv64);

			std::array<UInt64, 8> keys{ 0 };
			std::generate(keys.begin(), keys.end(),
						  [&]() -> UInt64
			{
				return (rng64() ^ keyiv64);
			});

			const auto rlen = data.GetSize() % sizeof(UInt64);
			const auto len = (data.GetSize() - rlen) / sizeof(UInt64);

			auto data64 = reinterpret_cast<UInt64*>(data.GetBytes());

			for (Size i = 0; i < len; ++i)
			{
				data64[i] = data64[i] ^ keys[i % keys.size()];
			}

			const auto idx = len % keys.size();
			const auto rkey64 = keys[idx];
			const auto rkey = reinterpret_cast<const Byte*>(&rkey64);

			for (Size i = data.GetSize() - rlen; i < data.GetSize(); ++i)
			{
				data[i] = data[i] ^ rkey[i % sizeof(rkey64)];
			}
		}

		inline static void Undo(BufferSpan& data, const BufferView& key, const UInt32 iv) noexcept
		{
			Do(data, key, iv);
		}
	};
}