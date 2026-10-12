//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "LocalNetworkBackdoor.h"
#include "server_class.h"
#include "client_class.h"
#include "server.h"
#include "eiface.h"
#include "cdll_engine_int.h"
#include "dt_localtransfer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CLocalNetworkBackdoor *g_pLocalNetworkBackdoor = NULL;

#ifndef DEDICATED
// This is called 
void CLocalNetworkBackdoor::InitFastCopy()
{
	if ( !GetBaseLocalClient().m_NetChannel->IsLoopback() )
		return;


	const CStandardSendProxies *pSendProxies = serverGameDLL->GetStandardSendProxies();
	const CStandardRecvProxies *pRecvProxies = g_ClientDLL->GetStandardRecvProxies();

	int nFastCopyProps = 0;
	int nSlowCopyProps = 0;

	for ( int iClass=0; iClass < GetBaseLocalClient().m_nServerClasses; iClass++ )
	{
		ClientClass *pClientClass = GetBaseLocalClient().GetClientClass(iClass);
		if ( !pClientClass ) 
			Error( "InitFastCopy - missing client class %d (Should be equivelent of server class: %s)", iClass, GetBaseLocalClient().m_pServerClasses[iClass].m_ClassName );

		ServerClass *pServerClass = SV_FindServerClass( pClientClass->GetName() );
		if ( !pServerClass )
			Error( "InitFastCopy - missing server class %s", pClientClass->GetName() );

		LocalTransfer_InitFastCopy(
			pServerClass->m_pTable,
			pSendProxies,
			pClientClass->m_pRecvTable,
			pRecvProxies,
			nSlowCopyProps,
			nFastCopyProps
			);
	}

	int percentFast = (nFastCopyProps * 100 ) / (nSlowCopyProps + nFastCopyProps + 1);
	if ( percentFast <= 45 )
	{
		// This may not be a real problem, but at the time this code was added, 67% of the
		// properties were able to be copied without proxies. If percentFast goes to 0 or some
		// really low number suddenly, then something probably got screwed up.
		Assert( false );
		Warning( "InitFastCopy: only %d%% fast props. Bug?\n", percentFast );
	}
} 
#endif //DEDICATED

void CLocalNetworkBackdoor::StartEntityStateUpdate()
{
	m_EntsAlive.ClearAll();
	m_nEntsCreated = 0;
	m_nEntsChanged = 0;

	// signal client that we start updating entities
	ClientDLL_FrameStageNotify( FRAME_NET_UPDATE_START );
}

void CLocalNetworkBackdoor::EndEntityStateUpdate()
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
	ClientDLL_FrameStageNotify( FRAME_NET_UPDATE_POSTDATAUPDATE_START );

	// Handle entities created.
	int i;
	for ( i=0; i < m_nEntsCreated; i++ )
	{
		int iEdict = m_EntsCreatedIndices[i];
		CCachedEntState *pCached = &m_CachedEntState[iEdict];
		IClientNetworkable *pNet = pCached->m_pNetworkable;

		pNet->PostDataUpdate( DATA_UPDATE_CREATED );
		pNet->NotifyShouldTransmit( SHOULDTRANSMIT_START );
		pCached->m_bDormant = false;
	}

	// Handle entities changed.
	for ( i=0; i < m_nEntsChanged; i++ )
	{
		int iEdict = m_EntsChangedIndices[i];
		m_CachedEntState[iEdict].m_pNetworkable->PostDataUpdate( DATA_UPDATE_DATATABLE_CHANGED );
	}

	ClientDLL_FrameStageNotify( FRAME_NET_UPDATE_POSTDATAUPDATE_END );

	// Handle entities removed (= SV_WriteDeletions() in normal mode)
	int nDWords = m_PrevEntsAlive.GetNumDWords();

	// Handle entities removed.
	for ( i=0; i < nDWords; i++ )
	{
		unsigned long prevEntsAlive = m_PrevEntsAlive.GetDWord( i );
		unsigned long entsAlive = m_EntsAlive.GetDWord( i );
		unsigned long toDelete = (prevEntsAlive ^ entsAlive) & prevEntsAlive;

		if ( toDelete )
		{
			for ( int iBit=0; iBit < 32; iBit++ )
			{
				if ( toDelete & (1 << iBit) )
				{
					int iEdict = (i<<5) + iBit;
					if ( iEdict < MAX_EDICTS )
					{
						if ( m_CachedEntState[iEdict].m_pNetworkable )
						{
							m_CachedEntState[iEdict].m_pNetworkable->Release();
							m_CachedEntState[iEdict].m_pNetworkable = NULL;
						}
						else
						{
							AssertOnce( !"EndEntityStateUpdate:  Would have crashed with NULL m_pNetworkable\n" );
						}
					}
					else
					{
						AssertOnce( !"EndEntityStateUpdate:  Would have crashed with entity out of range\n" );
					}
				}
			}
		}
	}

	// Remember the previous state of which entities were around.
	m_PrevEntsAlive = m_EntsAlive;

	// end of all entity update activity
	ClientDLL_FrameStageNotify( FRAME_NET_UPDATE_END );

	/*
	#ifdef _DEBUG
	for ( i=0; i <= highest_index; i++ )
	{
	if ( !( m_EntsAlive[i>>5] & (1 << (i & 31)) ) )
	Assert( !m_CachedEntState[i].m_pNetworkable );

	if ( ( m_EntsAlive[i>>5] & (1 << (i & 31)) ) &&
	( m_EntsCreated[i>>5] & (1 << (i & 31)) ) )
	{
	Assert( FindInList( m_EntsCreatedIndices, m_nEntsCreated, i ) );
	}

	if ( (m_EntsAlive[i>>5] & (1 << (i & 31))) && 
	!(m_EntsCreated[i>>5] & (1 << (i & 31))) &&
	(m_EntsChanged[i>>5] & (1 << (i & 31)))
	)
	{
	Assert( FindInList( m_EntsChangedIndices, m_nEntsChanged, i ) );
	}
	}
	#endif
	*/
}

void CLocalNetworkBackdoor::EntityDormant( int iEnt, int iSerialNum )
{
	CCachedEntState *pCached = &m_CachedEntState[iEnt];

	IClientNetworkable *pNet = pCached->m_pNetworkable;
	Assert( pNet == entitylist->GetClientNetworkable( iEnt ) );
	if ( pNet )
	{
		Assert( pCached->m_iSerialNumber == pNet->GetIClientUnknown()->GetRefEHandle().GetSerialNumber() );
		if ( pCached->m_iSerialNumber == iSerialNum )
		{
			m_EntsAlive.Set( iEnt );

			// Tell the game code that this guy is now dormant.
			Assert( pCached->m_bDormant == pNet->IsDormant() );
			if ( !pCached->m_bDormant )
			{
				pNet->NotifyShouldTransmit( SHOULDTRANSMIT_END );
				pCached->m_bDormant = true;
			}
		}
		else
		{
			pNet->Release();
			pCached->m_pNetworkable = NULL;
			m_PrevEntsAlive.Clear( iEnt ); 
		}
	}
}


void CLocalNetworkBackdoor::AddToPendingDormantEntityList( unsigned short iEdict )
{
	edict_t *e = &sv.edicts[iEdict];
	if ( !( e->m_fStateFlags & FL_EDICT_PENDING_DORMANT_CHECK ) )
	{
		e->m_fStateFlags |= FL_EDICT_PENDING_DORMANT_CHECK;
		m_PendingDormantEntities.AddToTail( iEdict );
	}
}		

void CLocalNetworkBackdoor::ProcessDormantEntities()
{
	FOR_EACH_LL( m_PendingDormantEntities, i )
	{
		int iEdict = m_PendingDormantEntities[i];
		edict_t *e = &sv.edicts[iEdict];

		// Make sure the entity still exists and stil has the dontsend flag set.
		if ( e->IsFree() || !(e->m_fStateFlags & FL_EDICT_DONTSEND) )
		{
			e->m_fStateFlags &= ~FL_EDICT_PENDING_DORMANT_CHECK;
			continue;
		}

		EntityDormant( iEdict, e->m_NetworkSerialNumber );
		e->m_fStateFlags &= ~FL_EDICT_PENDING_DORMANT_CHECK;
	}
	m_PendingDormantEntities.Purge();
}


	

void CLocalNetworkBackdoor::ClearState()
{
	// Clear the cache for all the entities.
	for ( int i=0; i < MAX_EDICTS; i++ )
	{
		CCachedEntState &ces = m_CachedEntState[i];

		ces.m_pNetworkable = NULL;
		ces.m_iSerialNumber = -1;
		ces.m_bDormant = false;
		ces.m_pDataPointer = NULL;
	}

	m_PrevEntsAlive.ClearAll();
}

void CLocalNetworkBackdoor::StartBackdoorMode() 
{ 
	ClearState();

	for ( int i=0; i < MAX_EDICTS; i++ )
	{
		IClientNetworkable *pNet = entitylist->GetClientNetworkable( i );

		CCachedEntState &ces = m_CachedEntState[i];

		if ( pNet )
		{
			ces.m_pNetworkable = pNet;
			ces.m_iSerialNumber = pNet->GetIClientUnknown()->GetRefEHandle().GetSerialNumber();
			ces.m_bDormant = pNet->IsDormant();
			ces.m_pDataPointer = pNet->GetDataTableBasePtr();
			m_PrevEntsAlive.Set( i );
		}
	}
}

void CLocalNetworkBackdoor::StopBackdoorMode()
{
	ClearState();
}


void CLocalNetworkBackdoor::ForceFlushEntity( int iEntity )
{
	CCachedEntState *pCached = &m_CachedEntState[iEntity];
	
	if ( pCached->m_pNetworkable )
		pCached->m_pNetworkable->Release();

	pCached->m_pNetworkable = NULL;
	pCached->m_iSerialNumber = -1;
	pCached->m_bDormant = false;
	pCached->m_pDataPointer = NULL;
}

