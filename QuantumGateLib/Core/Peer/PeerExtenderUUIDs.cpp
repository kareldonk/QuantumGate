// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PeerExtenderUUIDs.h"

#include <algorithm>

namespace QuantumGate::Implementation::Core::Peer
{
	bool ExtenderUUIDs::Set(Vector<ExtenderUUID>&& uuids) noexcept
	{
		if (SortAndEnsureUnique(uuids))
		{
			m_ExtenderUUIDs = std::move(uuids);
			return true;
		}

		return false;
	}

	bool ExtenderUUIDs::Copy(const ExtenderUUIDs& uuids) noexcept
	{
		try
		{
			m_ExtenderUUIDs = uuids.m_ExtenderUUIDs;
			return true;
		}
		catch (...) {}

		return false;
	}

	Result<std::pair<Vector<ExtenderUUID>,
		Vector<ExtenderUUID>>> ExtenderUUIDs::Update(Vector<ExtenderUUID>&& update_uuids) noexcept
	{
		try
		{
			if (SortAndEnsureUnique(update_uuids))
			{
				// Get all extenders that were removed
				Vector<ExtenderUUID> removed;
				std::remove_copy_if(m_ExtenderUUIDs.begin(), m_ExtenderUUIDs.end(), std::back_inserter(removed),
									[&](const ExtenderUUID& arg)
				{
					return (std::binary_search(update_uuids.begin(), update_uuids.end(), arg));
				});

				// Get all extenders that were added
				Vector<ExtenderUUID> added;
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

	bool ExtenderUUIDs::HasExtender(const ExtenderUUID& uuid) const noexcept
	{
		return (std::binary_search(m_ExtenderUUIDs.begin(), m_ExtenderUUIDs.end(), uuid));
	}

	bool ExtenderUUIDs::SortAndEnsureUnique(Vector<ExtenderUUID>& uuids) const noexcept
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
