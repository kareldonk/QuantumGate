// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BufferIO.h"

namespace QuantumGate::Implementation::Memory::BufferIO
{
	template<> Size GetDataSize(const BufferSpan& data) noexcept
	{
		return data.GetSize();
	}

	template<> Size GetDataSize(const BufferView& data) noexcept
	{
		return data.GetSize();
	}

	template<> Size GetDataSize(const String& data) noexcept
	{
		return (data.size() * sizeof(String::value_type));
	}

	template<> Size GetDataSize(const Network::SerializedBinaryIPAddress& data) noexcept
	{
		static_assert(sizeof(Network::SerializedBinaryIPAddress) == 17,
					  "Unexpected size of SerializedBinaryIPAddress; check padding or alignment.");
		return sizeof(Network::SerializedBinaryIPAddress);
	}

	template<> Size GetDataSize(const Network::SerializedBinaryBTHAddress& data) noexcept
	{
		static_assert(sizeof(Network::SerializedBinaryBTHAddress) == 9,
					  "Unexpected size of SerializedBinaryBTHAddress; check padding or alignment.");
		return sizeof(Network::SerializedBinaryBTHAddress);
	}

	template<> Size GetDataSize(const Network::SerializedIPEndpoint& data) noexcept
	{
		static_assert(sizeof(Network::SerializedIPEndpoint) == 20,
					  "Unexpected size of SerializedIPEndpoint; check padding or alignment.");
		return sizeof(Network::SerializedIPEndpoint);
	}

	template<> Size GetDataSize(const Network::SerializedBTHEndpoint& data) noexcept
	{
		static_assert(sizeof(Network::SerializedBTHEndpoint) == 28,
					  "Unexpected size of SerializedBTHEndpoint; check padding or alignment.");
		return sizeof(Network::SerializedBTHEndpoint);
	}

	template<> Size GetDataSize(const SerializedUUID& data) noexcept
	{
		static_assert(sizeof(SerializedUUID) == 16, "Unexpected size of SerializedUUID; check padding or alignment.");
		return sizeof(SerializedUUID);
	}

	template<> Size GetDataSize(const Buffer& data) noexcept
	{
		return data.GetSize();
	}

	template<> Size GetDataSize(const ProtectedBuffer& data) noexcept
	{
		return data.GetSize();
	}
}