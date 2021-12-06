// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Endpoint.h"
#include "..\Memory\BufferReader.h"
#include "..\Memory\BufferWriter.h"

namespace QuantumGate::Implementation::Network
{
	struct SerializedEndpoint final
	{
	public:
		Endpoint::Type Type{ Endpoint::Type::Unspecified };
		union
		{
			Byte Dummy[(std::max)(sizeof(SerializedIPEndpoint), sizeof(SerializedBTHEndpoint))]{ Byte{ 0 } };
			SerializedIPEndpoint IPEndpoint;
			SerializedBTHEndpoint BTHEndpoint;
		};

		SerializedEndpoint() noexcept {}
		SerializedEndpoint(const Endpoint& endpoint) noexcept { *this = endpoint; }

		SerializedEndpoint& operator=(const Endpoint& endpoint) noexcept
		{
			Type = endpoint.GetType();
			switch (Type)
			{
				case Endpoint::Type::IP:
					IPEndpoint = endpoint.GetIPEndpoint();
					break;
				case Endpoint::Type::BTH:
					BTHEndpoint = endpoint.GetBTHEndpoint();
					break;
				case Endpoint::Type::Unspecified:
					std::memset(&Dummy, 0, sizeof(Dummy));
					break;
				default:
					assert(false);
					break;
			}
			return *this;
		}

		operator Endpoint() const noexcept
		{
			switch (Type)
			{
				case Endpoint::Type::IP:
					return Network::IPEndpoint(IPEndpoint);
				case Endpoint::Type::BTH:
					return Network::BTHEndpoint(BTHEndpoint);
				case Endpoint::Type::Unspecified:
					break;
				default:
					assert(false);
					break;
			}
			return {};
		}

		bool operator==(const SerializedEndpoint& other) const noexcept
		{
			if (Type == other.Type)
			{
				switch (Type)
				{
					case Endpoint::Type::IP:
						return (IPEndpoint == other.IPEndpoint);
					case Endpoint::Type::BTH:
						return (BTHEndpoint == other.BTHEndpoint);
					case Endpoint::Type::Unspecified:
						return (std::memcmp(Dummy, other.Dummy, sizeof(Dummy)) == 0);
					default:
						assert(false);
						break;
				}
			}
			return false;
		}

		bool operator!=(const SerializedEndpoint& other) const noexcept
		{
			return !(*this == other);
		}

		[[nodiscard]] Size GetDataSize() const noexcept
		{
			switch (Type)
			{
				case Endpoint::Type::IP:
					return Memory::BufferIO::GetDataSizes(Type, IPEndpoint);
				case Endpoint::Type::BTH:
					return Memory::BufferIO::GetDataSizes(Type, BTHEndpoint);
				case Endpoint::Type::Unspecified:
					return Memory::BufferIO::GetDataSizes(Type);
				default:
					assert(false);
					break;
			}
			return 0;
		}

		[[nodiscard]] bool Read(Memory::BufferReader& reader) noexcept
		{
			if (reader.Read(Type))
			{
				switch (Type)
				{
					case Endpoint::Type::IP:
						return reader.Read(IPEndpoint);
					case Endpoint::Type::BTH:
						return reader.Read(BTHEndpoint);
					case Endpoint::Type::Unspecified:
						return true;
					default:
						assert(false);
						break;
				}
			}

			return false;
		}

		[[nodiscard]] bool Write(Memory::BufferWriter& writer) const noexcept
		{
			switch (Type)
			{
				case Endpoint::Type::IP:
					return writer.Write(Type, IPEndpoint);
				case Endpoint::Type::BTH:
					return writer.Write(Type, BTHEndpoint);
				case Endpoint::Type::Unspecified:
					return writer.Write(Type);
				default:
					assert(false);
					break;
			}

			return false;
		}
	};
}