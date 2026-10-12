//========= Copyright (c) Valve Corporation, All rights reserved. ============//
#pragma once

#include "mathlib/ssemath.h"
#include "tier1/strtools.h"

#define SUBSTRING_BITS	5
union StringHistoryEntry
{
	char string[ ( 1 << SUBSTRING_BITS ) ];
	shortx8 xmm[ ( 1 << ( SUBSTRING_BITS - 4 ) ) ]; // 16 bytes per xmm register
};

class CStringHistory
{
public:
	// limit string history to 32 entries
	enum EntryCountEnum_t { MAX_ENTRIES = 32 };

	CStringHistory()
	{
		m_nEnd = 0; 
		m_nCount = 0;
	}

	const StringHistoryEntry & operator []( int i ) const { return m_Entry[ ( i + MAX_ENTRIES + m_nEnd - m_nCount ) % MAX_ENTRIES ]; }
	StringHistoryEntry & operator []( int i ) { return m_Entry[ ( i + MAX_ENTRIES + m_nEnd - m_nCount ) % MAX_ENTRIES ]; }
	void Add( const char *pString )
	{
		V_strncpy( m_Entry[ m_nEnd ].string, pString, 1 << SUBSTRING_BITS );
		m_nCount = Min<int>( m_nCount + 1, MAX_ENTRIES );
		m_nEnd = ( m_nEnd + 1 ) % MAX_ENTRIES;
	}
	int Count()const
	{
		return m_nCount;
	}
protected:
	StringHistoryEntry m_Entry[ MAX_ENTRIES ];

	int m_nEnd; // end of ring buffer
	int m_nCount; // count of elements in the ring buffer
};

inline int CountSimilarCharacters( char const *str1, char const *str2 )
{
	int c = 0;
	while ( *str1 && *str2 &&
		*str1 == *str2 && c < ((1<<SUBSTRING_BITS) -1 ))
	{
		str1++;
		str2++;
		c++;
	}

	return c;
}

