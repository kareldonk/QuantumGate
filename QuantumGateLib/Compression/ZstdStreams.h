// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <zstd.h>

namespace QuantumGate::Implementation::Compression
{
	// One static thread_local ZstdStreams object will manage
	// resources for zstd de/compression for efficiency (not
	// having to allocate context memory constantly); for more
	// info see manual: https://facebook.github.io/zstd/zstd_manual.html
	class ZstdStreams final
	{
	private:
		ZstdStreams() noexcept
		{
			// One time allocation of streams
			m_CompressionStream = ZSTD_createCStream();
			m_DecompressionStream = ZSTD_createDStream();
		}

		ZstdStreams(const ZstdStreams&) = delete;
		ZstdStreams(ZstdStreams&&) = delete;

		~ZstdStreams()
		{
			// Free resources
			if (m_CompressionStream) ZSTD_freeCStream(m_CompressionStream);
			if (m_DecompressionStream) ZSTD_freeDStream(m_DecompressionStream);
		}

		ZstdStreams& operator=(const ZstdStreams&) = delete;
		ZstdStreams& operator=(ZstdStreams&&) = delete;

		ForceInline static ZstdStreams& GetZstdStreams() noexcept
		{
			// Static object for use by the current thread
			// allocated one time for efficiency
			static thread_local ZstdStreams zstd;
			return zstd;
		}

		inline static ZSTD_CStream* GetCompressionStream() noexcept { return GetZstdStreams().m_CompressionStream; }
		inline static ZSTD_DStream* GetDecompressionStream() noexcept { return GetZstdStreams().m_DecompressionStream; }
		inline static Int GetCompressionLevel() noexcept { return GetZstdStreams().m_CompressionLevel; }

	public:
		[[nodiscard]] static Size GetCompressBound(const Size input_size) noexcept
		{
			return ZSTD_compressBound(input_size);
		}

		[[nodiscard]] static bool Compress(Byte* outbuffer, Size& outlen, const BufferView& inbuffer) noexcept
		{
			assert(outbuffer != nullptr);
			assert(outlen > inbuffer.GetSize());

			auto zstream = GetCompressionStream();
			if (zstream != nullptr)
			{
				// Begin new compression session
				auto result = ZSTD_initCStream(zstream, GetCompressionLevel());
				if (!ZSTD_isError(result))
				{
					// Required to reduce resources used by zstd for this use case, since
					// we provide all input data to be compressed in one round
					result = ZSTD_CCtx_setPledgedSrcSize(zstream, inbuffer.GetSize());
					if (!ZSTD_isError(result))
					{
						ZSTD_inBuffer input{ inbuffer.GetBytes(), inbuffer.GetSize(), 0 };
						ZSTD_outBuffer output{ outbuffer, outlen, 0 };

						// Loop for as long as there's input left to compress
						while (input.pos < input.size)
						{
							const auto size = ZSTD_compressStream(zstream, &output, &input);
							if (ZSTD_isError(size))
							{
								return false;
							}
						}

						if (ZSTD_endStream(zstream, &output) == 0)
						{
							// Save final output size
							outlen = output.pos;
							return true;
						}
					}
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

			auto zstream = GetDecompressionStream();
			if (zstream != nullptr)
			{
				// Begin new decompression session
				const auto result = ZSTD_initDStream(zstream);
				if (!ZSTD_isError(result))
				{
					ZSTD_inBuffer input{ inbuffer.GetBytes(), inbuffer.GetSize(), 0 };
					ZSTD_outBuffer output{ outbuffer, outlen, 0 };

					// Loop for as long as there's input left to decompress
					while (input.pos < input.size)
					{
						const auto size = ZSTD_decompressStream(zstream, &output, &input);
						if (ZSTD_isError(size))
						{
							return false;
						}
					}

					// Save final output size
					outlen = output.pos;
					return true;
				}
			}

			return false;
		}

	private:
		const Int m_CompressionLevel{ 8 };

		ZSTD_CStream* m_CompressionStream{ nullptr };
		ZSTD_DStream* m_DecompressionStream{ nullptr };
	};
}