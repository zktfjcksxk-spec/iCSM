//============ Copyright © Valve Corporation, All rights reserved. ============//
//
// Point entity with a radius to name regions of a map 
//
//=============================================================================//
#include "cbase.h"
#include "c_info_map_region.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_InfoMapRegion, DT_InfoMapRegion, CInfoMapRegion )
RecvPropFloat( RECVINFO( m_flRadius ) ),
RecvPropString( RECVINFO( m_szLocToken ) ),
END_RECV_TABLE()

C_EntityClassList< C_InfoMapRegion > g_InfoRegionList;
template<> C_InfoMapRegion *C_EntityClassList<C_InfoMapRegion>::m_pClassList = nullptr;

C_InfoMapRegion* GetRegionNameList()
{
	return g_InfoRegionList.m_pClassList;
}

C_InfoMapRegion::C_InfoMapRegion()
{
	g_InfoRegionList.Insert( this );
}

C_InfoMapRegion::~C_InfoMapRegion()
{
	g_InfoRegionList.Remove( this );
}
