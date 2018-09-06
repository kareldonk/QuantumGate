// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "BufferIO.h"

namespace QuantumGate::Implementation::Memory
{
	template<> const Size BufferIO::GetDataSize(const String& data) noexcept
	{
		return (data.size() * sizeof(String::value_type));
	}

	template<> const Size BufferIO::GetDataSize(const Network::SerializedBinaryIPAddress& data) noexcept
	{
		static_assert(sizeof(Network::SerializedBinaryIPAddress) == 17,
					  "Unexpected size of SerializedBinaryIPAddress; check padding or alignment.");
		return sizeof(Network::SerializedBinaryIPAddress);
	}

	template<> const Size BufferIO::GetDataSize(const Network::SerializedIPEndpoint& data) noexcept
	{
		static_assert(sizeof(Network::SerializedIPEndpoint) == 19,
					  "Unexpected size of SerializedIPEndpoint; check padding or alignment.");
		return sizeof(Network::SerializedIPEndpoint);
	}

	template<> const Size BufferIO::GetDataSize(const SerializedUUID& data) noexcept
	{
		static_assert(sizeof(SerializedUUID) == 16, "Unexpected size of SerializedUUID; check padding or alignment.");
		return sizeof(SerializedUUID);
	}

	template<> const Size BufferIO::GetDataSize(const Buffer& data) noexcept
	{
		return data.GetSize();
	}

	template<> const Size BufferIO::GetDataSize(const BufferView& data) noexcept
	{
		return data.GetSize();
	}

	template<> const Size BufferIO::GetDataSize(const ProtectedBuffer& data) noexcept
	{
		return data.GetSize();
	}
}