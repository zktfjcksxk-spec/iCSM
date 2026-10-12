//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "quakedef.h"
#include "networkstringtableitem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableItem::CNetworkStringTableItem( void )
{
	m_Item.data = NULL;
	m_Item.length = 0;
	m_Item.tick = 0;

#ifndef SHARED_NET_STRING_TABLES
	m_nTickCreated = 0;
	m_pChangeList = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableItem::~CNetworkStringTableItem( void )
{
#ifndef SHARED_NET_STRING_TABLES
	if ( m_pChangeList )
	{
		// free changelist and elements

		for ( int i=0; i < m_pChangeList->Count(); i++ )
		{
			itemchange_s item = m_pChangeList->Element( i );

			if ( item.data )
				delete[] item.data;
		}

		delete m_pChangeList; // destructor calls Purge()

		m_Item.data = NULL;
	}
#endif
		
	if ( m_Item.data )
	{
		delete[] m_Item.data;
	}
}

#ifndef SHARED_NET_STRING_TABLES
void CNetworkStringTableItem::EnableChangeHistory( bool bEnable )
{
	if ( m_pChangeList )
	{
		if ( !bEnable )
		{
			if ( !m_pChangeList->IsEmpty() )
			{
				V_swap( m_Item, m_pChangeList->Tail() ); // bake the latest version of the data item into it 
			}
			for ( int i = 0; i < m_pChangeList->Count(); ++i )
				if ( unsigned char *pData = m_pChangeList->Element( i ).data )
					delete[] pData;
			// get rid of history
			delete m_pChangeList;
			m_pChangeList = NULL;
		}
	}
	else
	{
		if ( bEnable )
		{
			m_pChangeList = new CUtlVector<itemchange_s>;
			m_pChangeList->AddToTail( m_Item );
			m_Item.data = NULL;
			m_Item.length = 0;
		}
	}
}

bool CNetworkStringTableItem::UpdateChangeList( int tick, int length, const void *userData )
{
	int count = m_pChangeList->Count();
	itemchange_s item;

	if ( count > 0 )
	{	
		// check if different from last change in list
		item = m_pChangeList->Element( count-1 );

		if ( !item.data && !userData )
			return false; // both NULL data, no changes affected

		if ( item.length == length )
		{
			if ( item.data && userData )
			{
				if ( Q_memcmp( (void*)userData, (void*)item.data, length ) == 0 )
				{
					return false; // no data or size change, no changes affected
				}
			}
		}

		if ( item.tick == tick )
		{
			// two changes within same tick frame, remove last change from list
			if ( item.data )
			{
				delete[] item.data;
			}

			m_pChangeList->Remove( count-1 );
		}
				
	}

	item.tick = tick;
	m_Item.tick = tick; // make sure GetTickChanged() returns a consistent tick

	// add new user data and time stamp

	if ( userData && length )
	{
		item.data = new unsigned char[length];
		item.length = length;
		Q_memcpy( item.data, userData, length );
	}
	else
	{
		item.data = NULL;
		item.length = 0;
	}

	m_pChangeList->AddToTail( item );

	return true;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *string - 
//-----------------------------------------------------------------------------
bool CNetworkStringTableItem::SetUserData( int tick, int length, const void *userData )
{

#ifndef SHARED_NET_STRING_TABLES
	if ( m_pChangeList )
	{
		return UpdateChangeList( tick, length, userData );
	}
	Assert ( m_nTickCreated > 0 && m_nTickCreated <= tick );
#endif

	Assert ( GetTickChanged() > 0 && GetTickChanged() <= tick );
	Assert ( length < CNetworkStringTableItem::MAX_USERDATA_SIZE );

	// no old or new data
	if ( !userData && !m_Item.data )
		return false;

	if ( m_Item.data &&
		length == m_Item.length &&
		!Q_memcmp( m_Item.data, (void*)userData, length ) )
	{
		return false; // old & new data are equal
	}

	if ( m_Item.data )
		delete[] m_Item.data;

	m_Item.length = length;

	if ( length > 0 )
	{
		m_Item.data = new unsigned char[ length ];
		Q_memcpy( m_Item.data, userData, length );
	}
	else
	{
		m_Item.data = NULL; 
	}

	SetTickChanged( tick );

	return true;
}



const void* CNetworkStringTableItem::GetUserDataAtTick( int *length, int nAtTick )
{
	if ( m_pChangeList && m_pChangeList->Count() )
	{
		for ( int i = m_pChangeList->Count(); i-- > 0; )
		{
			const itemchange_s &item = m_pChangeList->Element( i );
			if ( item.tick <= nAtTick )
			{
				if ( length )
					*length = item.length;
				return item.data;
			}
		}
	}
	else if ( nAtTick >= m_nTickCreated )
	{
		if ( length )
			*length = m_Item.length;

		return ( const void * ) m_Item.data;
	}

	// couldn't find valid user data at the given tick
	if ( length )
		*length = 0;
	return NULL;
}

static CNetworkStringTableItem::itemchange_s s_NullItem = { 0,0,NULL };

// Given the last-ack'ed tick, and the tick at which we're interested in the last change, return that change
// return NULL if there was no change between nAckTick and nAt tick, inclusive
const CNetworkStringTableItem::itemchange_s* CNetworkStringTableItem::GetChangeAtTick( int nAtTick, int nAckTick ) const
{
	AssertDbg( nAckTick != nAtTick );

	if ( !m_pChangeList )
	{
		if ( m_nTickCreated <= nAtTick ) // is this item created yet? 
		{
			// this item exists at nAtTick
			if ( nAckTick >= m_Item.tick )
			{
				// the client already ack'ed this item, so there are no changes to report.
				// unless we're going backwards in time, but then there's a bad scenario :
				//	client jumps back and forward in time, receives the past version but its ack doesn't reach the server.
				//  then, server will think that client still thinks client's stringtable is in the future; but it'll be in the past. Server won't update the client string table, and it will stay outdated
				//  it's probably less risky to just send no update in this case
				return NULL;
			}

			// if it's changed after nAtTick, then we don't know what its state was before
			// we lost the actual item data before the last change, so it's unclear what we should return here
			AssertDbg( m_Item.tick <= nAtTick ); // "History request from table item without changelist (history)"
			// just return the last known state
			return &m_Item;
		}
		else
		{
			// Speculative change: ( nAckTick >= m_nTickCreated ) denotes the replay case when time flows backwards
			// for validity's sake, we could network an empty data item, but then (see scenario above) the client could jump forward in time, sever could miss its ack from the past, and think client still has the future(present) data
			// returning NULL is safer here because we don't destroy information from the future that may become ack'ed information from the past if the client quickly cancels replay.

			return NULL; // if not created yet, its state in the past is undefined and we don't care to send updates
		}
	}
	else
	{
		for ( int nChangeCount = m_pChangeList->Count(), i = nChangeCount; i-- > 0; )
		{
			const itemchange_s *pChange = &m_pChangeList->Element( i );
			if ( pChange->tick <= nAtTick )
			{
				// this is the last change before client's tick
				if ( nAckTick < nAtTick )
				{
					// normal timeline: from past to future. The change should happen after ack to be interesting.
					if ( pChange->tick > nAckTick )
					{
						// the change was not ack'ed yet, so it's an interesting change
						return pChange;
					}
				}
				else
				{
					// reverse timeline: a change was ack'ed in the future. If there "will have been"/"was" another change before/at the ack tick, then there's an interesting change to return
					if ( i + 1 < nChangeCount && m_pChangeList->Element( i + 1 ).tick <= nAckTick )
					{
						// the ack'ed change happened after the change we need
						return pChange;
					}
				}
				// the change was ack'ed already and there was no interesting change after the last ack - we found no changes at this tick to return
				return NULL;
			}
		}
		// all the changes happened after the tick requested; the protocol ("it was before me(tm)") dictates returning NULL
		return &s_NullItem;
	}
	// unreachable code:
	return NULL;
}





