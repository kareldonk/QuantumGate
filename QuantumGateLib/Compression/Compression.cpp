// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Compression.h"
#include "ZlibStreams.h"
#include "ZstdStreams.h"
#include "..\Common\Endian.h"

namespace QuantumGate::Implementation::Compression
{
	Export bool Compress(const BufferView& inbuffer, Buffer& outbuffer,
						 const Algorithm::Compression ca) noexcept
	{
		auto success = false;

		try
		{
			const auto hdrlen = sizeof(UInt32);
			const Size sizeuncompr{ inbuffer.GetSize() };
			Size sizecompr{ 0 };

			// Compress the buffer; note that we begin compression at
			// buffer beginning plus the size of the header 32-bit integer
			switch (ca)
			{
				case Algorithm::Compression::DEFLATE:
				{
					sizecompr = compressBound(static_cast<uLong>(sizeuncompr));

					outbuffer.Allocate(hdrlen + sizecompr);

					success = ZlibStreams::Compress(outbuffer.GetBytes() + hdrlen, sizecompr, inbuffer);

					break;
				}
				case Algorithm::Compression::ZSTANDARD:
				{
					sizecompr = ZSTD_compressBound(sizeuncompr);

					outbuffer.Allocate(hdrlen + sizecompr);

					success = ZstdStreams::Compress(outbuffer.GetBytes() + hdrlen, sizecompr, inbuffer);

					break;
				}
				default:
				{
					break;
				}
			}

			if (success)
			{
				// Remove unused space from the output buffer if any is left
				outbuffer.Resize(hdrlen + sizecompr);

				// Store the uncompressed size of the buffer at the beginning
				// of the output buffer in a header 32-bit integer
				const UInt32 suhdr = Endian::ToNetworkByteOrder(static_cast<UInt32>(sizeuncompr));
				memcpy(outbuffer.GetBytes(), &suhdr, hdrlen);
			}
		}
		catch (...) {}

		return success;
	}

	Export bool Decompress(BufferView inbuffer, Buffer& outbuffer,
						   const Algorithm::Compression ca, const std::optional<Size> maxsize) noexcept
	{
		try
		{
			const auto hdrlen = sizeof(UInt32);

			// Input buffer should at least have the header size integer
			if (inbuffer.GetSize() < hdrlen) return false;

			UInt32 suhdr{ 0 };

			// First get the size of uncompressed data from the buffer in the header 32-bit integer
			memcpy(&suhdr, inbuffer.GetBytes(), hdrlen);
			Size sizeuncompr{ Endian::FromNetworkByteOrder(suhdr) };

			// Check if the uncompressed data would be larger than the maximum allowed size
			// to protect against decompression bomb attack or bad data
			if (maxsize && sizeuncompr > *maxsize) return false;

			// We begin decompression at buffer beginning 
			// plus the size of the header 32-bit integer
			inbuffer.RemoveFirst(hdrlen);

			outbuffer.Allocate(sizeuncompr);

			switch (ca)
			{
				case Algorithm::Compression::DEFLATE:
				{
					if (!ZlibStreams::Decompress(outbuffer.GetBytes(), sizeuncompr, inbuffer)) return false;

					break;
				}
				case Algorithm::Compression::ZSTANDARD:
				{
					if (!ZstdStreams::Decompress(outbuffer.GetBytes(), sizeuncompr, inbuffer)) return false;

					break;
				}
				default:
				{
					return false;
				}
			}

			assert(outbuffer.GetSize() == sizeuncompr);

			// Final uncompressed size should match what we expected,
			// otherwise something is wrong
			return (outbuffer.GetSize() == sizeuncompr);
		}
		catch (...) {}

		return false;
	}
}