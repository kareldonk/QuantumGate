// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <shared_mutex>

namespace QuantumGate::Implementation::Core::Peer
{
	class ExtenderUUIDs
	{
	public:
		ExtenderUUIDs() = default;
		ExtenderUUIDs(const ExtenderUUIDs&) = delete;
		ExtenderUUIDs(ExtenderUUIDs&&) = default;
		~ExtenderUUIDs() = default;
		ExtenderUUIDs& operator=(const ExtenderUUIDs&) = delete;
		ExtenderUUIDs& operator=(ExtenderUUIDs&&) = default;

		[[nodiscard]] const bool HasExtender(const ExtenderUUID& uuid) const noexcept;

		inline const std::vector<ExtenderUUID>& Current() const noexcept { return m_ExtenderUUIDs; }

		[[nodiscard]] const bool Set(std::vector<ExtenderUUID>&& uuids) noexcept;
		[[nodiscard]] const bool Copy(const ExtenderUUIDs& uuids) noexcept;
		Result<std::pair<std::vector<ExtenderUUID>,
			std::vector<ExtenderUUID>>> Update(std::vector<ExtenderUUID>&& update_uuids) noexcept;

	private:
		[[nodiscard]] const bool SortAndEnsureUnique(std::vector<ExtenderUUID>& uuids) const noexcept;

	private:
		std::vector<ExtenderUUID> m_ExtenderUUIDs;
	};
}