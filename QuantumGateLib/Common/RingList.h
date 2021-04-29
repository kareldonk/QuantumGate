// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Containers.h"

namespace QuantumGate::Implementation
{
	template<typename T, Size MaxSize>
	class RingList final
	{
		using ListType = Containers::List<T>;

	public:
		bool Add(T&& value) noexcept
		{
			try
			{
				m_List.emplace_front(std::move(value));

				if (m_List.size() > MaxSize)
				{
					m_List.pop_back();
				}

				m_Updated = true;
			}
			catch (...) { return false; }

			return true;
		}

		[[nodiscard]] inline bool IsUpdated() const noexcept { return m_Updated; }
		inline void Expire() noexcept { m_Updated = false; }

		[[nodiscard]] inline bool IsEmpty() const noexcept { return m_List.empty(); }
		[[nodiscard]] inline Size GetSize() const noexcept { return m_List.size(); }
		[[nodiscard]] inline bool IsMaxSize() const noexcept { return m_List.size() == MaxSize; }

		[[nodiscard]] inline ListType& GetList() noexcept { return m_List; }
		[[nodiscard]] inline const ListType& GetList() const noexcept { return m_List; }
		
		inline void Clear() noexcept
		{
			m_List.clear();
			m_Updated = false;
		}

	private:
		bool m_Updated{ false };
		ListType m_List;
	};
}