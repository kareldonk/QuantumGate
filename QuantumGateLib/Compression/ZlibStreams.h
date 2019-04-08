// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Common\ScopeGuard.h"

#define ZLIB_WINAPI
#include <zlib.h>

namespace QuantumGate::Implementation::Compression
{
	// One static thread_local ZlibStreams object will manage
	// resources for zlib de/compression for efficiency (not
	// having to allocate context memory constantly); for more
	// info see manual: https://www.zlib.net/manual.html
	class ZlibStreams final
	{
	private:
		struct MemoryAllocation final
		{
			Memory::FreeBuffer Buffer;
			bool IsUsed{ false };
		};

		ZlibStreams() noexcept {}
		ZlibStreams(const ZlibStreams&) = delete;
		ZlibStreams(ZlibStreams&&) = delete;
		~ZlibStreams() {}
		ZlibStreams& operator=(const ZlibStreams&) = delete;
		ZlibStreams& operator=(ZlibStreams&&) = delete;

		static voidpf Alloc(voidpf opaque, uInt items, uInt size) noexcept
		{
			try
			{
				auto& memallocs = GetMemoryAllocations();

				// Find unused (freed) previous allocation
				const auto it = std::find_if(memallocs.begin(), memallocs.end(),
											 [&](const MemoryAllocation& alloc) noexcept
				{
					return !alloc.IsUsed;
				});

				if (it != memallocs.end())
				{
					// If we have an unused allocation, use it
					it->IsUsed = true;
					it->Buffer.Resize(static_cast<Size>(items) * static_cast<Size>(size));
					return it->Buffer.GetBytes();
				}
				else
				{
					// Make another allocation
					const auto nit = memallocs.insert(memallocs.end(),
													  { Memory::FreeBuffer(static_cast<Size>(items) *
																		   static_cast<Size>(size)), true });
					return nit->Buffer.GetBytes();
				}
			}
			catch (...) {}

			return nullptr;
		}

		static void Free(voidpf opaque, voidpf address) noexcept
		{
			auto& memallocs = GetMemoryAllocations();
			const auto it = std::find_if(memallocs.begin(), memallocs.end(),
										 [&](const MemoryAllocation& alloc) noexcept
			{
				if (alloc.Buffer.GetBytes() == address) return true;
				return false;
			});

			if (it != memallocs.end())
			{
				// We don't actually free memory;
				// just mark it as unused so it
				// will be reused later
				it->IsUsed = false;
			}
			else
			{
				// Trying to free address that
				// was not allocated by us
				assert(false);
			}
		}

		ForceInline static ZlibStreams& GetZlibStreams() noexcept
		{
			// Static object for use by the current thread
			// allocated one time for efficiency
			static thread_local ZlibStreams zstd;
			return zstd;
		}

		inline static Int GetCompressionLevel() noexcept { return GetZlibStreams().m_CompressionLevel; }

		inline static std::vector<MemoryAllocation>& GetMemoryAllocations() noexcept
		{
			return GetZlibStreams().m_MemoryAllocations;
		}

	public:
		[[nodiscard]] static bool Compress(Byte* outbuffer, Size& outlen, const BufferView& inbuffer) noexcept
		{
			assert(outbuffer != nullptr);
			assert(outlen > inbuffer.GetSize());

			// Code below largely taken from zlib library (uncompr.c) but the allocation
			// functions have been customized for some more efficiency

			z_stream zstream{ 0 };
			zstream.zalloc = &Alloc;
			zstream.zfree = &Free;

			auto ret = deflateInit(&zstream, GetCompressionLevel());
			if (ret == Z_OK)
			{
				// End zstream when we exit
				const auto sg = MakeScopeGuard([&]() noexcept { deflateEnd(&zstream); });

				const uInt max = std::numeric_limits<uInt>::max();
				Size left = outlen;
				Size sourcelen = inbuffer.GetSize();

				zstream.next_in = (Bytef*)inbuffer.GetBytes();
				zstream.avail_in = 0;
				zstream.next_out = reinterpret_cast<Bytef*>(outbuffer);
				zstream.avail_out = 0;

				do
				{
					if (zstream.avail_out == 0)
					{
						zstream.avail_out = (left > static_cast<Size>(max)) ? max : static_cast<uInt>(left);
						left -= zstream.avail_out;
					}

					if (zstream.avail_in == 0)
					{
						zstream.avail_in = (sourcelen > static_cast<Size>(max)) ? max : static_cast<uInt>(sourcelen);
						sourcelen -= zstream.avail_in;
					}

					ret = deflate(&zstream, sourcelen ? Z_NO_FLUSH : Z_FINISH);
				} while (ret == Z_OK);

				if (ret == Z_STREAM_END)
				{
					// Save final output size
					outlen = zstream.total_out;
					return true;
				}
			}

			return false;
		}

		[[nodiscard]] static bool Decompress(Byte* outbuffer, Size& outlen, const BufferView& inbuffer) noexcept
		{
			DbgInvoke([&]()
			{
				if (outlen > 0) assert(outbuffer != nullptr);
			});

			// Code below largely taken from zlib library but the allocation
			// functions have been customized for some more efficiency

			z_stream zstream{ 0 };
			zstream.zalloc = &Alloc;
			zstream.zfree = &Free;

			auto ret = inflateInit(&zstream);
			if (ret == Z_OK)
			{
				// End zstream when we exit
				const auto sg = MakeScopeGuard([&]() noexcept { inflateEnd(&zstream); });

				const uInt max = std::numeric_limits<uInt>::max();
				Byte buf{ 0 };
				Size left = outlen;
				Size sourcelen = inbuffer.GetSize();

				if (outlen == 0)
				{
					// Special case when for detection of
					// incomplete stream when outlen == 0
					left = 1;
					outbuffer = &buf;
				}

				zstream.next_in = (Bytef*)inbuffer.GetBytes();
				zstream.avail_in = 0;
				zstream.next_out = reinterpret_cast<Bytef*>(outbuffer);
				zstream.avail_out = 0;

				do
				{
					if (zstream.avail_out == 0)
					{
						zstream.avail_out = (left > static_cast<Size>(max)) ? max : static_cast<uInt>(left);
						left -= zstream.avail_out;
					}

					if (zstream.avail_in == 0)
					{
						zstream.avail_in = (sourcelen > static_cast<Size>(max)) ? max : static_cast<uInt>(sourcelen);
						sourcelen -= zstream.avail_in;
					}

					ret = inflate(&zstream, Z_NO_FLUSH);
				} while (ret == Z_OK);

				if (ret == Z_STREAM_END)
				{
					// Save final output size
					if (outbuffer != &buf) outlen = zstream.total_out;
					return true;
				}
			}

			return false;
		}

	private:
		const Int m_CompressionLevel{ Z_DEFAULT_COMPRESSION };

		std::vector<MemoryAllocation> m_MemoryAllocations;
	};
}