//============ Copyright (c) Valve Corporation, All rights reserved. ============

#include "tier0/basetypes.h"
#include "dbg.h"
#include "kv3lib/blockcompress.h"

#define BLOCK_COMPRESS_HASH_BITS 11
#define BLOCK_COMPRESS_HASH_SIZE ( 1 << BLOCK_COMPRESS_HASH_BITS )
#define BLOCK_COMPRESS_MATCH_BITS 4
#define BLOCK_COMPRESS_DIST_BITS 12
#define BLOCK_COMPRESS_MIN_MATCH_LEN 3
#define BLOCK_COMPRESS_MAX_MATCH_LEN ( BLOCK_COMPRESS_MIN_MATCH_LEN + ( ( 1 << BLOCK_COMPRESS_MATCH_BITS ) - 1 ) )
#define BLOCK_COMPRESS_MAX_MATCH_DIST ( 1 + ( ( 1 << BLOCK_COMPRESS_DIST_BITS ) - 1 ) )

//-----------------------------------------------------------------------------------------------------------------------
// 4-byte integer hash, full avalanche, no muls or constants. TODO: This is most likely still overkill.
//-----------------------------------------------------------------------------------------------------------------------
static inline uint32 BitMix32( uint32 a )
{
	a -= ( a << 6 );
	a ^= ( a >> 17 );
	a -= ( a << 9 );
	a ^= ( a << 4 );
	a -= ( a << 3 );
	a ^= ( a << 10 );
	a ^= ( a >> 15 );
	return a;
}

//-----------------------------------------------------------------------------------------------------------------------
uint32 CBlockCompress::FastCompress( const uint8 *pSrcBuf, uint32 nSrcBufSize, uint8 *pDstBuf, uint32 nDstBufSize )
{
	if ( nSrcBufSize >= 0x7FFFFFFF )
	{
		// Input too large.
		return 0;
	}
	if ( nDstBufSize < ( nSrcBufSize + BLOCK_COMPRESS_MAX_EXPANSION_SIZE ) )
	{
		// Not enough bytes to write the source+header if it can't be compressed.
		return 0;
	}

	uint8 *pDst = pDstBuf;
	pDst[0] = static_cast< uint8 >( nSrcBufSize );
	pDst[1] = static_cast< uint8 >( nSrcBufSize >> 8 );
	pDst[2] = static_cast< uint8 >( nSrcBufSize >> 16 );
	pDst[3] = static_cast< uint8 >( nSrcBufSize >> 24 );
	pDst += sizeof( uint32 );

	if ( !nSrcBufSize )
	{
		// Zero length source.
		pDstBuf[3] |= 0x80;
		return sizeof( uint32 );
	}

	uint8 *pBitBufDst = pDst;
	pDst += sizeof( uint16 );

	uint8 *pDstEnd = pDstBuf + nSrcBufSize - sizeof( uint32 );

	uint nBitBuf = 0;
	uint nBitBufRemaining = 16;

	const uint8 *pSrc = pSrcBuf;
	const uint8 *pSrcEnd = pSrcBuf + nSrcBufSize;

	const uint8 *phraseHash[BLOCK_COMPRESS_HASH_SIZE] = {0}; // NOTE: Setting this to 0 in order to get deterministic compression
	while ( pSrc < pSrcEnd )
	{
		const uint nMaxMatchLen = MIN( static_cast< uint >( pSrcEnd - pSrc ), BLOCK_COMPRESS_MAX_MATCH_LEN );

		uint c0 = pSrc[0];
		uint nMatchLen = 0;
		uint nMatchDist = 0;

		if ( nMaxMatchLen >= BLOCK_COMPRESS_MIN_MATCH_LEN )
		{
			uint c1 = pSrc[1];
			uint c2 = pSrc[2];

			uint32 nHashIndex = BitMix32( c0 | ( c1 << 8 ) | ( c2 << 16 ) ) & ( BLOCK_COMPRESS_HASH_SIZE - 1 );
			const uint8 *pPossibleMatch = phraseHash[nHashIndex];
			phraseHash[nHashIndex] = pSrc;

			nMatchDist = static_cast< uint >( pSrc - pPossibleMatch );

			if ( ( pPossibleMatch >= pSrcBuf ) && ( pPossibleMatch < pSrc ) && ( nMatchDist <= BLOCK_COMPRESS_MAX_MATCH_DIST ) )
			{
				do
				{
					if ( pSrc[nMatchLen] != pPossibleMatch[nMatchLen] )
						break;
					nMatchLen++;
				} while ( nMatchLen < nMaxMatchLen );
			}
		}

		if ( nMatchLen < BLOCK_COMPRESS_MIN_MATCH_LEN )
		{
			nBitBuf >>= 1;

			*pDst++ = static_cast< uint8 >( c0 );
			pSrc++;
		}
		else
		{
			nBitBuf = ( nBitBuf >> 1 ) | 0x8000;

			Assert( ( nMatchLen - BLOCK_COMPRESS_MIN_MATCH_LEN ) < ( 1 << BLOCK_COMPRESS_MATCH_BITS ) );
			Assert( ( nMatchDist - 1 ) < ( 1 << BLOCK_COMPRESS_DIST_BITS ) );

			uint nMatchCode = ( nMatchLen - BLOCK_COMPRESS_MIN_MATCH_LEN ) | ( ( nMatchDist - 1 ) << BLOCK_COMPRESS_MATCH_BITS );
			Assert( nMatchCode <= UINT16_MAX );

			pDst[0] = static_cast< uint8 >( nMatchCode );
			pDst[1] = static_cast< uint8 >( nMatchCode >> 8 );
			pDst += 2;

			pSrc += nMatchLen;
		}

		nBitBufRemaining--;
		if ( !nBitBufRemaining )
		{
			nBitBufRemaining = 16;
			pBitBufDst[0] = static_cast< uint8 >( nBitBuf );
			pBitBufDst[1] = static_cast< uint8 >( nBitBuf >> 8 );
			nBitBuf = 0;
			pBitBufDst = pDst;
			pDst += sizeof( uint16 );
		}

		if ( pDst >= pDstEnd )
		{
			Assert( pDst <= ( pDstBuf + nDstBufSize ) );
			// Source is uncompressible.
			pDstBuf[3] |= 0x80;
#ifdef PLATFORM_X360
			XMemCpy( pDstBuf + sizeof( uint32 ), pSrcBuf, nSrcBufSize );
#else
			memcpy( pDstBuf + sizeof( uint32 ), pSrcBuf, nSrcBufSize );
#endif
			return nSrcBufSize + sizeof( uint32 );
		}
	}

	if ( nBitBufRemaining != 16 )
	{
		nBitBuf >>= nBitBufRemaining;
		pBitBufDst[0] = static_cast< uint8 >( nBitBuf );
		pBitBufDst[1] = static_cast< uint8 >( nBitBuf >> 8 );
	}
	else
	{
		pDst -= sizeof( uint16 );
		Assert( pDst == pBitBufDst );
	}

	uint32 nBytesWritten = static_cast< uint >( pDst - pDstBuf );
	Assert( nBytesWritten <= nDstBufSize );

	return nBytesWritten;
}

//-----------------------------------------------------------------------------------------------------------------------
uint32 CBlockCompress::FastDecompress( const uint8 *pSrcBuf, uint32 nSrcBufSize, uint8 *pDstBuf, uint32 nDstBufSize )
{
	if ( nSrcBufSize < sizeof( uint32 ) )
		return 0;

	uint nHeaderBits = pSrcBuf[0] | ( pSrcBuf[1] << 8 ) | ( pSrcBuf[2] << 16 ) | ( pSrcBuf[3] << 24 );
	uint nDecompLen = nHeaderBits & 0x7FFFFFFF;
	if ( nDstBufSize < nDecompLen )
		return 0;

	if ( nHeaderBits & 0x80000000 )
	{
		if ( ( nSrcBufSize - sizeof( uint32 ) ) < nDecompLen )
			return 0;
#ifdef PLATFORM_X360
		XMemCpy( pDstBuf, pSrcBuf + sizeof( uint32 ), nDecompLen );
#else
		memcpy( pDstBuf, pSrcBuf + sizeof( uint32 ), nDecompLen );
#endif
		return nDecompLen;
	}

	if ( nSrcBufSize < 6 )
		return 0;

	uint16 nBitBuf = pSrcBuf[4] | ( pSrcBuf[5] << 8 );
	uint nBitBufRemaining = 16;

	const uint8 *pSrc = pSrcBuf + 6;
	const uint8 *pSrcEnd = pSrcBuf + nSrcBufSize;

	uint8 *pDst = pDstBuf;
	uint8 *pDstEnd = pDst + nDecompLen;

	while ( pDst < pDstEnd )
	{
		if ( !nBitBufRemaining )
		{
			if ( ( pSrc += 2 ) > pSrcEnd )
				return 0;

			nBitBuf = pSrc[-2] | ( pSrc[-1] << 8 );
			nBitBufRemaining = 16;
		}

		if ( nBitBuf & 1 )
		{
			if ( ( pSrc += 2 ) > pSrcEnd )
				return 0;

			uint nMatchCode = pSrc[-2] | ( pSrc[-1] << 8 );
			uint nMatchLen = BLOCK_COMPRESS_MIN_MATCH_LEN + ( nMatchCode & ( ( 1 << BLOCK_COMPRESS_MATCH_BITS ) - 1 ) );
			uint nMatchDist = 1 + ( nMatchCode >> BLOCK_COMPRESS_MATCH_BITS );

			const uint8 *pMatchSrc = pDst - nMatchDist;
			if ( ( pMatchSrc < pDstBuf ) || ( ( pDst + nMatchLen ) > pDstEnd ) )
				return 0;

			if ( nMatchDist == 1 )
			{
				// Single byte repeat match - avoid LHS on each iteration.
				uint c = *pMatchSrc;
				for ( uint i = 0; i < nMatchLen; i++ )
				{
					pDst[i] = static_cast< uint8 >( c );
				}
				pDst += nMatchLen;
			}
			else
			{
				for ( uint i = 0; i < nMatchLen; i++ )
					*pDst++ = *pMatchSrc++;
			}
		}
		else
		{
			if ( ( pSrc += 1 ) > pSrcEnd )
				return 0;

			*pDst++ = pSrc[-1];
		}

		nBitBuf >>= 1;
		nBitBufRemaining--;
	}

	return nDecompLen;
}