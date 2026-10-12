//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "ServerNetworkProperty.h"
#include "tier0/dbg.h"
#include "tier0/spinlock.h"
#include "gameinterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CTimedEventMgr g_NetworkPropertyEventMgr;

//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------
BEGIN_DATADESC_NO_BASE( CServerNetworkProperty )
//	DEFINE_FIELD( m_pOuter, FIELD_CLASSPTR ),
//	DEFINE_FIELD( m_pPev, FIELD_CLASSPTR ),
//	DEFINE_FIELD( m_PVSInfo, PVSInfo_t ),
//	DEFINE_FIELD( m_pServerClass, FIELD_CLASSPTR ),
	DEFINE_GLOBAL_FIELD( m_hParent, FIELD_EHANDLE ),
//	DEFINE_FIELD( m_TimerEvent, CEventRegister ),
//	DEFINE_FIELD( m_bPendingStateChange, FIELD_BOOLEAN ),
END_DATADESC()


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CServerNetworkProperty::CServerNetworkProperty()
{
	Init( NULL );
}


CServerNetworkProperty::~CServerNetworkProperty()
{
	/* Free our transmit proxy.
	if ( m_pTransmitProxy )
	{
		m_pTransmitProxy->Release();
	}*/

	engine->CleanUpEntityClusterList( &m_PVSInfo );

	// remove the attached edict if it exists
	DetachEdict();
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CServerNetworkProperty::Init( CBaseEntity *pEntity )
{
	// NOTE: We're in pEntity's constructor so we can't call virtual methods of pEntity here
	m_pPev = NULL;
	m_pOuter = pEntity;
	m_pServerClass = NULL;
//	m_pTransmitProxy = NULL;
	m_bPendingStateChange = false;
	m_PVSInfo.m_nClusterCount = 0;
	m_TimerEvent.Init( &g_NetworkPropertyEventMgr, this );
}

void CServerNetworkProperty::CacheServerClass()
{
	m_pServerClass = m_pOuter->GetServerClass();
}


//-----------------------------------------------------------------------------
// Connects, disconnects edicts
//-----------------------------------------------------------------------------
void CServerNetworkProperty::AttachEdict( edict_t *pRequiredEdict )
{
	Assert ( !m_pPev );

	// see if there is an edict allocated for it, otherwise get one from the engine
	if ( !pRequiredEdict )
	{
		pRequiredEdict = engine->CreateEdict();
	}

	m_pPev = pRequiredEdict;
	m_pPev->SetEdict( GetBaseEntity(), true );
}

void CServerNetworkProperty::DetachEdict()
{
	if ( m_pPev )
	{
		m_pPev->SetEdict( NULL, false );
		engine->RemoveEdict( m_pPev );
		m_pPev = NULL;
	}
}


//-----------------------------------------------------------------------------
// Entity handles
//-----------------------------------------------------------------------------
IHandleEntity *CServerNetworkProperty::GetEntityHandle( )
{
	return m_pOuter;
}

void CServerNetworkProperty::Release()
{
	delete m_pOuter;
}


//-----------------------------------------------------------------------------
// Returns the network parent
//-----------------------------------------------------------------------------
CServerNetworkProperty* CServerNetworkProperty::GetNetworkParent()
{
	CBaseEntity *pParent = m_hParent.Get();
	return pParent ? pParent->NetworkProp() : NULL;
}


//-----------------------------------------------------------------------------
// Marks for deletion
//-----------------------------------------------------------------------------
void CServerNetworkProperty::MarkForDeletion()
{
	m_pOuter->AddEFlags( EFL_KILLME );
}

bool CServerNetworkProperty::IsMarkedForDeletion() const
{
	return ( m_pOuter->GetEFlags() & EFL_KILLME ) != 0;
}




//////////////////////////////////////////////////////////////////////////
// edict pointer is 16-byte-aligned and bits [15:4] are effectively the significant bits corresponding to entindex with some shift (gpGlobals->pEdicts[0] is 0th edict in server)
// For cache efficiency, it may be better to swizzle the bits so that neighboring edicts occupy different cache lines, but it's not clear it's a win.
// the Lock will work even if above is not true. The main consideration is that the same edict must lock the same bit in the bitmap. 
// Even if multiple edicts share the same bit and some bits are not used, functionally the lock will keep working
//
class CEdictLockBits
{
	enum ConstEnum_t { ENT_COUNT = 1 << 11 }; // it's probably most efficient if the number of bits == MAX_EDICTS, but it's not necessarily so.
	// we could just fit this into 64 bytes, but then all CPUs would fight for the same cache line. 
	// It's a lot of cache pollution to give each entity a cache line (and 128Kbytes isn't trivial amount of memory)
	// 4 bytes per entity, swizzled to avoid sequential entities fighting for the same cache lines, seems like a reasonable compromise 
	// It's 16Kbytes, not quite insignificant, but seems tolerably small. It's not at all obvious that it is optimal though.
	uint32 m_EntLocks[ ENT_COUNT ];
protected:
	uint32 *GetEdictSlot( edict_t *pEdict )
	{
		// the significant bits are [14:4], shifting bits [14:11]->[3:0] will effectively swizzle indices
		// such that neighboring entindices are 16 uint32's = 64 bytes = 1 cache line apart, which is perfect
		// to avoid fighting for the same cache line
		uint nSwizzledIndex = ( ( ( uintp( pEdict ) >> 11 ) & 0xF ) ^ uintp( pEdict ) ) & ( ENT_COUNT - 1 );
		AssertDbg( nSwizzledIndex < ENT_COUNT );
		return &m_EntLocks[ nSwizzledIndex ];
	}
public:
	CEdictLockBits()
	{
		V_memset( m_EntLocks, 0, sizeof( m_EntLocks ) );
	}
	void Lock( edict_t *pEdict )
	{
		volatile uint32 *pEdictSlot = GetEdictSlot( pEdict );
		while ( 0 != ThreadInterlockedCompareExchange( pEdictSlot, 1, 0 ) )
			continue;
	}
	void Unlock( edict_t *pEdict )
	{
		volatile uint32 *pEdictSlot = GetEdictSlot( pEdict );
		while ( 1 != ThreadInterlockedCompareExchange( pEdictSlot, 0, 1 ) )
			continue;
	}
};

CEdictLockBits g_EdictLockBits;




//-----------------------------------------------------------------------------
// PVS information
//-----------------------------------------------------------------------------
void CServerNetworkProperty::RecomputePVSInformation()
{
	if ( m_pPev )
	{
		// Sergiy: on ARM, we'd need acquire semantics to read m_fStateFlags 
		if ( m_pPev->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION )
		{
			g_EdictLockBits.Lock( m_pPev );
			if ( m_pPev->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION ) // still dirty?
			{	
				engine->BuildEntityClusterList( edict(), &m_PVSInfo );
				m_pPev->m_fStateFlags &= ~FL_EDICT_DIRTY_PVS_INFORMATION; // clear the bit while we have the lock
			}
			g_EdictLockBits.Unlock( m_pPev );
		}
	}
}


//-----------------------------------------------------------------------------
// Serverclass
//-----------------------------------------------------------------------------
ServerClass* CServerNetworkProperty::GetServerClass()
{
	return m_pServerClass;
}

const char* CServerNetworkProperty::GetClassName() const
{
	return STRING(m_pOuter->m_iClassname);
}


//-----------------------------------------------------------------------------
// Transmit proxies
/*-----------------------------------------------------------------------------
void CServerNetworkProperty::SetTransmitProxy( CBaseTransmitProxy *pProxy )
{
	if ( m_pTransmitProxy )
	{
		m_pTransmitProxy->Release();
	}

	m_pTransmitProxy = pProxy;
	
	if ( m_pTransmitProxy )
	{
		m_pTransmitProxy->AddRef();
	}
}*/

//-----------------------------------------------------------------------------
// PVS rules
//-----------------------------------------------------------------------------
bool CServerNetworkProperty::IsInPVS( const edict_t *pRecipient, const void *pvs, int pvssize )
{
	RecomputePVSInformation();

	// ignore if not touching a PV leaf
	// negative leaf count is a node number
	// If no pvs, add any entity

	Assert( pvs && ( edict() != pRecipient ) );

	unsigned char *pPVS = ( unsigned char * )pvs;
	
	if ( m_PVSInfo.m_nClusterCount < 0 )   // too many clusters, use headnode
	{
		return ( engine->CheckHeadnodeVisible( m_PVSInfo.m_nHeadNode, pPVS, pvssize ) != 0);
	}
	
	for ( int i = 0; i < m_PVSInfo.m_nClusterCount; i++ )
	{
		if (pPVS[m_PVSInfo.m_pClusters[i] >> 3] & (1 << (m_PVSInfo.m_pClusters[i] & 7) ))
			return true;
	}

	return false;		// not visible
}


//-----------------------------------------------------------------------------
// PVS: this function is called a lot, so it avoids function calls
//-----------------------------------------------------------------------------
bool CServerNetworkProperty::IsInPVS( const CCheckTransmitInfo *pInfo )
{
	// PVS data must be up to date in normal single-threaded mode, but in multithreaded parallel send mode it's possible that PVS wasn't updated yet because it's still queued on another thread.
	// It should be exceedingly rare; if PVS is dirty regularly here, there maybe a perf issue
	RecomputePVSInformation();

	int i;

	// Early out if the areas are connected
	if ( !m_PVSInfo.m_nAreaNum2 )
	{
		for ( i=0; i< pInfo->m_AreasNetworked; i++ )
		{
			int clientArea = pInfo->m_Areas[i];
			if ( clientArea == m_PVSInfo.m_nAreaNum || engine->CheckAreasConnected( pInfo->m_pEnginePVSContext, clientArea, m_PVSInfo.m_nAreaNum ) )
				break;
		}
	}
	else
	{
		// doors can legally straddle two areas, so
		// we may need to check another one
		for ( i=0; i< pInfo->m_AreasNetworked; i++ )
		{
			int clientArea = pInfo->m_Areas[i];
			if ( clientArea == m_PVSInfo.m_nAreaNum || clientArea == m_PVSInfo.m_nAreaNum2 )
				break;

			if ( engine->CheckAreasConnected( pInfo->m_pEnginePVSContext, clientArea, m_PVSInfo.m_nAreaNum ) )
				break;

			if ( engine->CheckAreasConnected( pInfo->m_pEnginePVSContext, clientArea, m_PVSInfo.m_nAreaNum2 ) )
				break;
		}
	}

	if ( i == pInfo->m_AreasNetworked )
	{
		// areas not connected
		return false;
	}

	// ignore if not touching a PV leaf
	// negative leaf count is a node number
	// If no pvs, add any entity

	Assert( edict() != pInfo->m_pClientEnt );

	unsigned char *pPVS = ( unsigned char * )pInfo->m_PVS;
	if ( m_PVSInfo.m_nClusterCount < 0 )   // too many clusters, use headnode
	{
		return (engine->CheckHeadnodeVisible( m_PVSInfo.m_nHeadNode, pPVS, pInfo->m_nPVSSize ) != 0);
	}
	
	for ( i = m_PVSInfo.m_nClusterCount; --i >= 0; )
	{
		int nCluster = m_PVSInfo.m_pClusters[i];
		if ( ((int)(pPVS[nCluster >> 3])) & BitVec_BitInByte( nCluster ) )
			return true;
	}

	return false;		// not visible

}


void CServerNetworkProperty::SetUpdateInterval( float val )
{
	if ( val == 0 )
		m_TimerEvent.StopUpdates();
	else
		m_TimerEvent.SetUpdateInterval( val );
}


void CServerNetworkProperty::FireEvent()
{
	// Our timer went off. If our state has changed in the background, then 
	// trigger a state change in the edict.
	if ( m_bPendingStateChange )
	{
		m_pPev->StateChanged();
		m_bPendingStateChange = false;
	}
}



