//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifndef BLOCKCOMPRESS_H
#define BLOCKCOMPRESS_H
#pragma once

#include "tier0/platform.h"

// The maximum amount the source data can be expanded during compression. The output buffer must always be at least (nSrcBufSize + BLOCK_COMPRESS_MAX_EXPANSION_SIZE).
#define BLOCK_COMPRESS_MAX_EXPANSION_SIZE sizeof( uint32 )

#define BLOCK_COMPRESS_HEADER_SIZE sizeof( uint32 )

class CBlockCompress
{
public:
	// Basic linear time LZRW1-style block compressor.
	// This function is up to ~40x faster than CLZSS on 360, although it compresses 3-4% worse.
	// Compression rate on X360 is ~55MB/sec., or ~55,000 bytes per ms. Decompression rate is several times higher.
	// This function purposely doesn't memset() its hash table, so it's not deterministic given the same input data, but the output is guaranteed to be correct.
	// Guaranteed to succeed even if the source is uncompressible.
	// Destination buffer size must be at least nSrcBufSize+BLOCK_COMPRESS_MAX_EXPANSION_SIZE bytes (big enough to hold an uncompressible input).
	static uint32 FastCompress( const uint8 *pSrcBuf, uint32 nSrcBufSize, uint8 *pDstBuf, uint32 nDstBufSize );
	
	// Returns the decompressed size of the compressed block.
	static uint32 GetDecompressedSize( const uint8 *pSrcBuf, uint32 nSrcBufSize );
	
	// Returns true if the block contains uncompressed data.
	// In this case, the actual raw data occurs BLOCK_COMPRESS_HEADER_SIZE after the beginning of the compressed buffer.
	// IN OTHER WORDS: EVEN IF IsStoredUncompressed() RETURNS true YOU STILL NEED TO CALL FastDecompress() ON THE BUFFER.
	static bool IsStoredUncompressed( const uint8 *pSrcBuf, uint32 nSrcBufSize );

	// Decompresses a block of compressed data, or 0 on failure.
	// The destination buffer must be large enough to hold the decompressed data, and must be in readable (cached) memory. 
	// Full checking is performed, so corrupted compressed data will not cause this function to go off the rails.
	static uint32 FastDecompress( const uint8 *pSrcBuf, uint32 nSrcBufSize, uint8 *pDstBuf, uint32 nDstBufSize );
};

inline uint32 CBlockCompress::GetDecompressedSize( const uint8 *pSrcBuf, uint32 nSrcBufSize )
{
	if ( nSrcBufSize < sizeof( uint32 ) )
		return 0;
	uint nHeaderBits = pSrcBuf[0] | ( pSrcBuf[1] << 8 ) | ( pSrcBuf[2] << 16 ) | ( pSrcBuf[3] << 24 );
	return nHeaderBits & 0x7FFFFFFF;
}

inline bool CBlockCompress::IsStoredUncompressed( const uint8 *pSrcBuf, uint32 nSrcBufSize )
{
	if ( nSrcBufSize < sizeof( uint32 ) )
		return false;
	return static_cast< int8 >( pSrcBuf[3] ) < 0;
}

#endif // BLOCKCOMPRESS_H

