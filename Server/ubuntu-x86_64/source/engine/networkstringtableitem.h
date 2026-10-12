//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NETWORKSTRINGTABLEITEM_H
#define NETWORKSTRINGTABLEITEM_H
#ifdef _WIN32
#pragma once
#endif

#include "utlsymbol.h"
#include "utlvector.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNetworkStringTableItem
{
public:
	enum
	{
		MAX_USERDATA_BITS = 14,
		MAX_USERDATA_SIZE = (1 << MAX_USERDATA_BITS)
	};

	struct itemchange_s {
		int				tick;
		int				length;
		unsigned char	*data;
	};

	CNetworkStringTableItem( void );
	~CNetworkStringTableItem( void );

#ifndef SHARED_NET_STRING_TABLES
	void			EnableChangeHistory( bool bEnable );
	bool 			UpdateChangeList( int tick, int length, const void *userData );
	inline int		GetTickCreated( void ) const { return m_nTickCreated; }
#endif
	
	bool			SetUserData( int tick, int length, const void *userdata );
	const void		*GetUserData( int *length = NULL );
	const void		*GetUserDataAtTick( int *length, int nAtTick );
	
	// Used by server only
	void			SetTickChanged( int nTickChanged ) { Assert( !m_pChangeList );  m_Item.tick = nTickChanged; } // this call makes no sense for items with history enabled
	inline int		GetTickChanged( void ) const 
	{// we can ask when item changed even if there's history on it, but then the result must be consistent with querying the last item in history
		Assert( !m_pChangeList || m_pChangeList->IsEmpty() || m_pChangeList->Tail().tick == m_Item.tick );
		return m_Item.tick; 
	}
	const itemchange_s*	GetChangeAtTick( int nAtTick, int nAckTick ) const;

protected:
	itemchange_s	m_Item; // TODO: make it just changelist, which always has at least 1 element
public:
#ifndef SHARED_NET_STRING_TABLES
	int				m_nTickCreated;
	CUtlVector< itemchange_s > *m_pChangeList;	
#endif
};


inline const void *CNetworkStringTableItem::GetUserData( int *length )
{
	const itemchange_s *pOutItem = &m_Item;
	if ( m_pChangeList && !m_pChangeList->IsEmpty() )
	{
		pOutItem = &m_pChangeList->Tail();
	}

	if ( length )
		*length = pOutItem->length;

	return ( const void * ) pOutItem->data;
}



#endif // NETWORKSTRINGTABLEITEM_H
