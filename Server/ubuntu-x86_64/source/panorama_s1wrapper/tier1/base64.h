//============ Copyright (c) Valve Corporation, All rights reserved. ============
#pragma once

#include <ctype.h>
#include "tier0/platform.h"


unsigned int base64_encode( char *pDst, unsigned int nDstLength, const byte *pSrc, unsigned int nSrcLength );
unsigned int base64_decode( byte *pDst, unsigned int nDstLength, const char *pSrc, unsigned int nSrcLength );
inline uint base64_decode_maxsize( uint32 cubData ) { return ( ( ( cubData + 3 ) / 4 ) * 3 ) + 1; }
inline uint base64_encode_maxsize( uint32 cubData ) { return ( ( ( cubData + 2 ) / 3 ) * 4 ) + 1; }


char base64_encode_table[] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };
unsigned char base64_decode_table[ 128 ] =
{
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x3E, 0x80, 0x80, 0x80, 0x3F,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
	0x3C, 0x3D, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
	0x17, 0x18, 0x19, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
	0x31, 0x32, 0x33, 0x80, 0x80, 0x80, 0x80, 0x80,
};

unsigned int base64_encode( char *pDst, unsigned int nDstLength, const byte *pSrc, unsigned int nSrcLength )
{
	unsigned int x;
	unsigned int nOutLength = 0;
	int n = 3;
	byte triple[ 3 ], quad[ 4 ];

	for ( x = 0; x < nSrcLength && nOutLength < nDstLength; x = x + 3 )
	{
		if ( ( nSrcLength - x ) / 3 == 0 )
		{
			n = ( nSrcLength - x ) % 3;
		}

		for ( int i = 0; i < 3; i++ )
		{
			triple[ i ] = '0';
		}

		for ( int i = 0; i < n; i++ )
		{
			triple[ i ] = pSrc[ x + i ];
		}

		quad[ 0 ] = base64_encode_table[ ( triple[ 0 ] & 0xFC ) >> 2 ];
		quad[ 1 ] = base64_encode_table[ ( ( triple[ 0 ] & 0x03 ) << 4 ) | ( ( triple[ 1 ] & 0xF0 ) >> 4 ) ];
		quad[ 2 ] = base64_encode_table[ ( ( triple[ 1 ] & 0x0F ) << 2 ) | ( ( triple[ 2 ] & 0xC0 ) >> 6 ) ];
		quad[ 3 ] = base64_encode_table[ triple[ 2 ] & 0x3F ];

		if ( n < 3 )
		{
			quad[ 3 ] = '=';
		}

		if ( n < 2 )
		{
			quad[ 2 ] = '=';
		}

		for ( int i = 0; i < 4 && nOutLength + i < nDstLength; i++ )
		{
			pDst[ nOutLength + i ] = quad[ i ];
		}

		nOutLength = nOutLength + 4;
	}

	if ( nOutLength < nDstLength )
	{
		pDst[ nOutLength ] = '\0';
	}

	return nOutLength;
}

static inline bool is_base64( unsigned char c )
{
	return ( V_isalnum( c ) || ( c == '+' ) || ( c == '/' ) );
}

unsigned int base64_decode( byte *pDst, unsigned int nDstLength, const char *pSrc, unsigned int nSrcLength )
{
	unsigned int nOutLength = 0;
	byte quad[ 4 ];
	byte triple[ 3 ];

	int i = 0;
	while ( nSrcLength-- && ( pSrc[ i ] != '=' ) && is_base64( pSrc[ i ] ) )
	{
		quad[ i & 0x03 ] = base64_decode_table[ pSrc[ i ] & 0x7F ];
		++i;
		if ( ( i & 0x03 ) == 0 )
		{
			// process 4 bytes into 3
			triple[ 0 ] = ( quad[ 0 ] << 2 ) + ( ( quad[ 1 ] & 0x30 ) >> 4 );
			triple[ 1 ] = ( ( quad[ 1 ] & 0xf ) << 4 ) + ( ( quad[ 2 ] & 0x3c ) >> 2 );
			triple[ 2 ] = ( ( quad[ 2 ] & 0x3 ) << 6 ) + quad[ 3 ];

			for ( int j = 0; j < 3 && nOutLength < nDstLength; j++ )
			{
				pDst[ nOutLength++ ] = triple[ j ];
			}

			if ( nOutLength >= nDstLength )
				break;
		}
	}

	// process remainders
	if ( i & 0x03 )
	{
		for ( int j = ( i & 0x03 ); j < 4; j++ )
		{
			// fill remainder
			quad[ j ] = 0;
		}

		triple[ 0 ] = ( quad[ 0 ] << 2 ) + ( ( quad[ 1 ] & 0x30 ) >> 4 );
		triple[ 1 ] = ( ( quad[ 1 ] & 0xf ) << 4 ) + ( ( quad[ 2 ] & 0x3c ) >> 2 );
		triple[ 2 ] = ( ( quad[ 2 ] & 0x3 ) << 6 ) + quad[ 3 ];

		for ( int j = 0; j < ( i & 0x03 ) - 1 && nOutLength < nDstLength; j++ )
		{
			pDst[ nOutLength++ ] = triple[ j ];
		}
	}

	return nOutLength;
}

