// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <random>
#include <memory>

// If NO_PCG_RANDOM is defined during compile time then PCG won't be used;
// instead the Mersenne Twister engine from std will be used (see further below)
#ifndef NO_PCG_RANDOM
#pragma warning(push)
#pragma warning(disable:4244)
#include <pcg_random.hpp>
#pragma warning(pop)
#endif

namespace QuantumGate::Implementation
{
#ifndef NO_PCG_RANDOM
	using Rng32Alg = pcg32_unique;
	using Rng64Alg = pcg64_unique;
#else
	using Rng32Alg = std::mt19937;
	using Rng64Alg = std::mt19937_64;
#endif

	struct RngEngine final
	{
		std::random_device Device;
		Rng32Alg Rng32{ Device() };
		UInt64 Rng32Count{ 0 };
		Rng64Alg Rng64{ Device() };
		UInt64 Rng64Count{ 0 };
		static constexpr UInt64 RngEngineReseedLimit{ 2'147'483'648 }; // 2^31

		ForceInline void CheckSeed32(const UInt64 num = 1) noexcept
		{
			if (std::numeric_limits<UInt64>::max() - num >= Rng32Count)
			{
				Rng32Count += num;

				if (Rng32Count > RngEngineReseedLimit)
				{
					Rng32.seed(Device());
					Rng32Count = 0;
				}
			}
			else
			{
				Rng32.seed(Device());
				Rng32Count = 0;
			}
		}

		ForceInline void CheckSeed64(const UInt64 num = 1) noexcept
		{
			if (std::numeric_limits<UInt64>::max() - num >= Rng64Count)
			{
				Rng64Count += num;

				if (Rng64Count > RngEngineReseedLimit)
				{
					Rng64.seed(Device());
					Rng64Count = 0;
				}
			}
			else
			{
				Rng64.seed(Device());
				Rng64Count = 0;
			}
		}
	};

	class Random final
	{
	private:
		Random() noexcept = default;

	public:
		inline static Int64 GetPseudoRandomNumber() noexcept
		{
			GetRngEngine().CheckSeed64();
			return (GetRngEngine().Rng64)();
		}

		inline static Int64 GetPseudoRandomNumber(const Int64 min, const Int64 max) noexcept
		{
			GetRngEngine().CheckSeed64();
			const std::uniform_int_distribution<Int64> dist(min, max);

			return dist(GetRngEngine().Rng64);
		}

		static Buffer GetPseudoRandomBytes(const Size count);

	private:
		ForceInline static RngEngine& GetRngEngine() noexcept
		{
			// Each thread gets its own copy of RngEngine,
			// making random number generation unique to each thread
			static thread_local RngEngine rngen;
			return rngen;
		}
	};
}
