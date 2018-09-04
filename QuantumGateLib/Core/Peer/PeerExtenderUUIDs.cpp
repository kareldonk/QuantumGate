// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerExtenderUUIDs.h"

#include <algorithm>

namespace QuantumGate::Implementation::Core::Peer
{
	const bool ExtenderUUIDs::Set(std::vector<ExtenderUUID>&& uuids) noexcept
	{
		if (SortAndEnsureUnique(uuids))
		{
			m_ExtenderUUIDs = std::move(uuids);
			return true;
		}

		return false;
	}

	const bool ExtenderUUIDs::Copy(const ExtenderUUIDs& uuids) noexcept
	{
		try
		{
			m_ExtenderUUIDs = uuids.m_ExtenderUUIDs;
			return true;
		}
		catch (...) {}

		return false;
	}

	Result<std::pair<std::vector<ExtenderUUID>,
		std::vector<ExtenderUUID>>> ExtenderUUIDs::Update(std::vector<ExtenderUUID>&& update_uuids) noexcept
	{
		try
		{
			if (SortAndEnsureUnique(update_uuids))
			{
				// Get all extenders that were removed
				std::vector<ExtenderUUID> removed;
				std::remove_copy_if(m_ExtenderUUIDs.begin(), m_ExtenderUUIDs.end(), std::back_inserter(removed),
									[&](const ExtenderUUID& arg)
				{
					return (std::binary_search(update_uuids.begin(), update_uuids.end(), arg));
				});

				// Get all extenders that were added
				std::vector<ExtenderUUID> added;
				std::remove_copy_if(update_uuids.begin(), update_uuids.end(), std::back_inserter(added),
									[&](const ExtenderUUID& arg)
				{
					return (std::binary_search(m_ExtenderUUIDs.begin(), m_ExtenderUUIDs.end(), arg));
				});

				// Update current extender list with updates
				m_ExtenderUUIDs = std::move(update_uuids);

				return { std::make_pair(std::move(added), std::move(removed)) };
			}
		}
		catch (...) {}

		return ResultCode::Failed;
	}

	const bool ExtenderUUIDs::HasExtender(const ExtenderUUID& uuid) const noexcept
	{
		return (std::binary_search(m_ExtenderUUIDs.begin(), m_ExtenderUUIDs.end(), uuid));
	}

	const bool ExtenderUUIDs::SortAndEnsureUnique(std::vector<ExtenderUUID>& uuids) const noexcept
	{
		if (uuids.size() <= 1) return true;

		std::sort(uuids.begin(), uuids.end());

		for (std::size_t x = 1; x < uuids.size(); ++x)
		{
			// If we find at least one duplicate
			if (uuids[x] == uuids[x - 1])
			{
				return false;
			}
		}

		return true;
	}
}
