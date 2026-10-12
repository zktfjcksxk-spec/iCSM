//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: components of tier0 PLAT_ with (at least mostly) platform independent implementations.
//
// $NoKeywords: $
//===========================================================================//


#include "pch_tier0.h"
#include <time.h>

void GetCurrentDayOfTheWeek( int *pDay )
{
	struct tm *pNewTime;
	time_t long_time;

	time( &long_time );                /* Get time as long integer. */
	pNewTime = localtime( &long_time ); /* Convert to local time. */

	*pDay = pNewTime->tm_wday;
}

void GetCurrentDayOfTheYear( int *pDay )
{
	struct tm *pNewTime;
	time_t long_time;

	time( &long_time );                /* Get time as long integer. */
	pNewTime = localtime( &long_time ); /* Convert to local time. */

	*pDay = pNewTime->tm_yday;
}


//-----------------------------------------------------------------------------
// UUIDs
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE void Plat_CreateUUIDImpl( V_uuid_t *uuid, int( *pfnRand )( int, int ) )
{
	const int MAX_RANDOM_RANGE = 0x7FFFFFFFUL;
	uint32 *pIdRaw = ( uint32 * )uuid;
	pIdRaw[0] = ( *pfnRand )( 0, MAX_RANDOM_RANGE ) ^ ( ( *pfnRand )( 0, MAX_RANDOM_RANGE ) << 1 );
	pIdRaw[1] = ( *pfnRand )( 0, MAX_RANDOM_RANGE ) ^ ( ( *pfnRand )( 0, MAX_RANDOM_RANGE ) << 1 );
	pIdRaw[2] = ( *pfnRand )( 0, MAX_RANDOM_RANGE ) ^ ( ( *pfnRand )( 0, MAX_RANDOM_RANGE ) << 1 );
	pIdRaw[3] = ( *pfnRand )( 0, MAX_RANDOM_RANGE ) ^ ( ( *pfnRand )( 0, MAX_RANDOM_RANGE ) << 1 );

	// put in the variant and version bits.
	uuid->Data3 &= 0x0FFF;
	uuid->Data3 |= ( 4 << 12 ); // version 4, see RFC 4122
	uuid->Data4[0] &= 0x3F;
	uuid->Data4[0] |= 0x80;
}

PLATFORM_INTERFACE void Plat_UUIDToString( const V_uuid_t *pUuidIn, char *pBuf, size_t nBufSize )
{
	const V_uuid_t *pUuid = ( V_uuid_t * )pUuidIn;
	if ( nBufSize >= UUID_STRING_SIZE )
	{
		sprintf( pBuf, "%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x", pUuid->Data1, pUuid->Data2,
			pUuid->Data3, pUuid->Data4[0], pUuid->Data4[1], pUuid->Data4[2], pUuid->Data4[3], pUuid->Data4[4], pUuid->Data4[5], pUuid->Data4[6], pUuid->Data4[7] );
	}
	else if ( nBufSize > 0 )
	{
		*pBuf = 0;
	}
}

PLATFORM_INTERFACE bool Plat_UUIDFromString( V_uuid_t *pIdIn, const char *pBuf )
{
	V_uuid_t *pId = ( V_uuid_t * )pIdIn;
	if ( pBuf && strlen( pBuf ) == UUID_STRING_SIZE - 1 )
	{
		uint32 scanned[11];
		int nScanned = ::sscanf(
			pBuf,
			"%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x",
			&scanned[0],
			&scanned[1],
			&scanned[2],
			&scanned[3],
			&scanned[4],
			&scanned[5],
			&scanned[6],
			&scanned[7],
			&scanned[8],
			&scanned[9],
			&scanned[10] );

		pId->Data1 = scanned[0];
		pId->Data2 = ( uint16 )scanned[1];
		pId->Data3 = ( uint16 )scanned[2];
		pId->Data4[0] = ( uint8 )scanned[3];
		pId->Data4[1] = ( uint8 )scanned[4];
		pId->Data4[2] = ( uint8 )scanned[5];
		pId->Data4[3] = ( uint8 )scanned[6];
		pId->Data4[4] = ( uint8 )scanned[7];
		pId->Data4[5] = ( uint8 )scanned[8];
		pId->Data4[6] = ( uint8 )scanned[9];
		pId->Data4[7] = ( uint8 )scanned[10];

		if ( nScanned == 11 )
		{
			return true;
		}
	}
	memset( pIdIn, 0, sizeof( V_uuid_t ) );
	return false;
}