// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <chrono>
#include <atomic>

namespace QuantumGate::Implementation
{
	template<Size MaxNumMeasurements = 100, bool Alloc = false>
	class DiffTimer final
	{
		static_assert(MaxNumMeasurements > 0, "The maximum number of measurements should be greater than zero.");

		using IDType = UInt32;

		struct MeasurementData
		{
			IDType ID{ 0 };
			std::chrono::time_point<std::chrono::steady_clock> Start;
			std::chrono::time_point<std::chrono::steady_clock> End;
		};

		using MeasurementContainer = std::conditional_t<Alloc, std::vector<MeasurementData>,
			std::array<MeasurementData, MaxNumMeasurements>>;

	public:
		class Measurement final
		{
		public:
			Measurement(MeasurementData& data) noexcept : m_Data(data) {}
			Measurement(const Measurement&) = delete;
			Measurement(Measurement&&) = default;
			~Measurement() = default;
			Measurement& operator=(const Measurement&) = delete;
			Measurement& operator=(Measurement&&) = default;

			void Start() noexcept
			{
				m_Data.Start = std::chrono::steady_clock::now();
			}

			std::chrono::nanoseconds End() noexcept
			{
				m_Data.End = std::chrono::steady_clock::now();
				return GetElapsedTime();
			}

			[[nodiscard]] std::chrono::nanoseconds GetElapsedTime() noexcept
			{
				return std::chrono::duration_cast<std::chrono::nanoseconds>(m_Data.End - m_Data.Start);
			}

		private:
			MeasurementData& m_Data;
		};

		static constexpr IDType AllIDs{ 0 };

		DiffTimer() noexcept(!Alloc)
		{
			if constexpr (Alloc)
			{
				m_Measurements.reserve(MaxNumMeasurements);
			}
		}

		DiffTimer(const DiffTimer&) = delete;
		DiffTimer(DiffTimer&&) = delete;
		~DiffTimer() = default;
		DiffTimer& operator=(const DiffTimer&) = delete;
		DiffTimer& operator=(DiffTimer&&) = delete;

		Measurement GetNewMeasurement(const IDType id)
		{
			assert(id != AllIDs);

			auto entry = m_NextEntry.load(std::memory_order::memory_order_relaxed);
			auto next_entry = entry + 1;
			while (entry < MaxNumMeasurements && !m_NextEntry.compare_exchange_weak(entry, next_entry,
																					std::memory_order_release,
																					std::memory_order_relaxed))
			{
				next_entry = entry + 1;
			}

			if (entry >= MaxNumMeasurements)
			{
				throw std::system_error(std::make_error_code(std::errc::value_too_large),
										"There are no more available entries.");
			}

			m_Measurements[entry].ID = id;

			return Measurement(m_Measurements[entry]);
		}

		[[nodiscard]] std::chrono::nanoseconds GetTotalElapsedTime(const IDType id = AllIDs) noexcept
		{
			std::chrono::nanoseconds total{ 0 };

			for (const auto& measurement : m_Measurements)
			{
				if (id == AllIDs || measurement.ID == id) total += measurement.End - measurement.Start;
			}

			return total;
		}

	private:
		MeasurementContainer m_Measurements;
		std::atomic<Size> m_NextEntry{ 0 };
	};
}