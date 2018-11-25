// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Network\IPAddress.h"
#include "..\..\Concurrency\ThreadSafe.h"

#include <unordered_map>

namespace QuantumGate::Implementation::Core::Access
{
	struct IPFilterImpl final
	{
		IPFilterID ID{ 0 };
		IPFilterType Type{ IPFilterType::Blocked };
		IPAddress Address;
		IPAddress Mask;
		Network::BinaryIPAddress StartAddress; // Network byte order (big endian)
		Network::BinaryIPAddress EndAddress; // Network byte order (big endian)
	};

	using IPFilterMap = std::unordered_map<IPFilterID, IPFilterImpl>;

	class Export IPFilters final
	{
	public:
		IPFilters() noexcept {}
		IPFilters(const IPFilters&) = delete;
		IPFilters(IPFilters&&) = default;
		~IPFilters() = default;
		IPFilters& operator=(const IPFilters&) = delete;
		IPFilters& operator=(IPFilters&&) = default;

		Result<IPFilterID> AddFilter(const WChar* ip_cidr,
									 const IPFilterType type) noexcept;
		Result<IPFilterID> AddFilter(const WChar* ip_str, const WChar* mask_str,
									 const IPFilterType type) noexcept;
		Result<IPFilterID> AddFilter(const String& ip_str, const String& mask_str,
									 const IPFilterType type) noexcept;
		Result<IPFilterID> AddFilter(const IPAddress& ip, const IPAddress& mask,
									 const IPFilterType type) noexcept;

		Result<> RemoveFilter(const IPFilterID filterid, const IPFilterType type) noexcept;

		void Clear() noexcept;

		[[nodiscard]] const bool HasFilter(const IPFilterID filterid, const IPFilterType type) const noexcept;

		Result<Vector<IPFilter>> GetFilters() const noexcept;

		Result<bool> IsAllowed(const WChar* ip) const noexcept;
		Result<bool> IsAllowed(const IPAddress& ipaddr) const noexcept;

	private:
		Result<IPFilterID> AddFilterImpl(const IPAddress& ip, const IPAddress& mask,
										 const IPFilterType type) noexcept;

		[[nodiscard]] const bool IsIPInFilterMap(const IPAddress& address, const IPFilterMap& filtermap) const noexcept;

		const IPFilterID GetFilterID(const IPAddress& ip, const IPAddress& mask) const noexcept;

	private:
		IPFilterMap m_IPAllowFilters;
		IPFilterMap m_IPBlockFilters;
	};

	using IPFilters_ThS = Concurrency::ThreadSafe<IPFilters, std::shared_mutex>;
}
