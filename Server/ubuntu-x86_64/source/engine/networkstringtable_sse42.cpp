//========= Copyright (c) Valve Corporation, All rights reserved. ============//
#include "platform.h"
#include "bitvec.h"
#include "networkstringtable_stringhistory.h"
#include <nmmintrin.h>


int GetBestPreviousString_SSE42_V1( const CStringHistory& history, char const *newstring, int& substringsize )
{
	int bestindex = -1;
	int bestcount = 0;
	int c = history.Count();

	shortx8 ns0 = LoadUnalignedShortSIMD( newstring );
	shortx8 ns1 = LoadUnalignedShortSIMD( newstring + 16 );

	for ( int i = 0; i < c; i++ )
	{
		const StringHistoryEntry &she = history[ i ];

		int nCmpLen = _mm_cmpistri( ns0, she.xmm[ 0 ], _SIDD_NEGATIVE_POLARITY | _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_LEAST_SIGNIFICANT );

		if ( nCmpLen < 3 )
			continue;

		if ( nCmpLen == 16 )
			nCmpLen += _mm_cmpistri( ns1, she.xmm[ 1 ], _SIDD_NEGATIVE_POLARITY | _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_LEAST_SIGNIFICANT );

		if ( nCmpLen > bestcount )
		{
			bestcount = nCmpLen;
			bestindex = i;
		}
	}

	//Assert( GetBestPreviousStringV1( history, newstring, substringsize ) == bestindex &&  substringsize == bestcount );

	substringsize = bestcount;
	return bestindex;
}


int GetBestPreviousString_SSE42( const CStringHistory& history, char const *newstring, int& substringsize )
{
	int bestindex = -1;
	int bestcount = 0;
	int c = history.Count();
	shortx8 zero = _mm_setzero_si128();
	shortx8 ns0 = LoadUnalignedShortSIMD( newstring );
	int nStrLen = _mm_cmpistri( zero, ns0, _SIDD_CMP_EQUAL_EACH );
	shortx8 ns1;
	if ( nStrLen == 16 )
	{
		ns1 = LoadUnalignedShortSIMD( newstring + 16 );
		nStrLen += _mm_cmpistri( zero, ns1, _SIDD_CMP_EQUAL_EACH );
	}
	else
	{
		if ( nStrLen < 3 )
			return -1;

		// This won't be used because we can't get here unless at least one of the 16 bytes in ns0 is zero,
		// which means _mm_cmpistri below can't ever return 16.
		ns1 = zero;
	}

	for ( int i = 0; i < c; i++ )
	{
		const StringHistoryEntry &she = history[ i ];

		int nCmpLen = _mm_cmpistri( ns0, she.xmm[ 0 ], _SIDD_NEGATIVE_POLARITY | _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_LEAST_SIGNIFICANT );

		if ( nCmpLen < 3 )
			continue;

		if ( nCmpLen == 16 )
		{
			nCmpLen += _mm_cmpistri( ns1, she.xmm[ 1 ], _SIDD_NEGATIVE_POLARITY | _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_LEAST_SIGNIFICANT );
		}

		if ( nCmpLen > bestcount )
		{
			bestcount = nCmpLen;
			bestindex = i;
		}
	}
	substringsize = Min( bestcount, nStrLen );
	return bestindex;
}


// 4+ms
int GetBestPreviousString_SSE42_V2( const CStringHistory& history, char const *newstring, int& substringsize )
{
	int nHistoryCount = history.Count();
	if ( !nHistoryCount )
	{
		return -1;
	}

	StringHistoryEntry nse;
	uint nNewStringLength = 0;
	COMPILE_TIME_ASSERT( SUBSTRING_BITS == 5 ); // 32 bytes in an entry
	for ( ; nNewStringLength < 32; ++nNewStringLength )
	{
		char nCharacter = newstring[ nNewStringLength ];
		nse.string[ nNewStringLength ] = nCharacter;
		if ( !nCharacter )
			break;
	}
	if ( nNewStringLength < 3 )
	{
		return -1;
	}

	uint8 nBestIndex[ 17 ]; // for a match of N characters, best[N] will have the index of the match
	V_memset( nBestIndex, 0xFF, sizeof( nBestIndex ) );
	uint nMatchesMask = 0; // for each match i, bit 1<<i will be set
	int nFullMatchesCount = 0;
	uint8 nFullMatches[ CStringHistory::MAX_ENTRIES ];

	for ( int i = nHistoryCount; i-->0; )
	{
		const StringHistoryEntry &she = history[ i ];

		int nCmpLen = _mm_cmpistri( nse.xmm[ 0 ], she.xmm[ 0 ], _SIDD_NEGATIVE_POLARITY | _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_LEAST_SIGNIFICANT );
		
		nFullMatches[ nFullMatchesCount ] = i;
		nFullMatchesCount += nCmpLen >> 4; // only advance count of full matches if full match is found (16 matching characters)
		
		nBestIndex[ nCmpLen ] = i;
		nMatchesMask |= 1 << nCmpLen;
	}
	Assert( nMatchesMask );
	uint nBestCmpLen = Plat_BitScanReverse( nMatchesMask );

	if ( nBestCmpLen >= nNewStringLength )
	{
		// full match
		substringsize = nNewStringLength;
		return nBestIndex[ nBestCmpLen ];
	}

	if ( nBestCmpLen < 16 )
	{
		// main case
		substringsize = nBestCmpLen;
		if ( nBestCmpLen < 3 )
			return -1;
		return nBestIndex[ nBestCmpLen ];
	}

	Assert( nFullMatchesCount );
	nMatchesMask = 0; // for each match i (+16), bit 1<<i will be set

	V_memset( nBestIndex, 0xFF, sizeof( nBestIndex ) );
	for ( int j = 0; j < nFullMatchesCount; j++ )
	{
		uint nHistoryEntry = nFullMatches[ j ];
		const StringHistoryEntry &she = history[ nHistoryEntry ];

		int nCmpLen = _mm_cmpistri( nse.xmm[ 1 ], she.xmm[ 1 ], _SIDD_NEGATIVE_POLARITY | _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_LEAST_SIGNIFICANT );
		nBestIndex[ nCmpLen ] = nHistoryEntry;
		nMatchesMask |= 1 << nCmpLen;
	}
	Assert( nMatchesMask );
	nBestCmpLen = Plat_BitScanReverse( nMatchesMask ); 

	if ( nBestCmpLen + 16 >= nNewStringLength )
	{
		// full match
		substringsize = nNewStringLength;
		return nBestIndex[ nBestCmpLen ];
	}

	substringsize = nBestCmpLen + 16;
	return nBestIndex[ nBestCmpLen ];
}

