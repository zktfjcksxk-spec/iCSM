//============ Copyright © Valve Corporation, All rights reserved. ============//
//
// Point entity with a radius to name regions of a map 
//
//=============================================================================//

#include "cbase.h"
#include "info_map_region.h"
#include "util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( info_map_region, CInfoMapRegion );

BEGIN_DATADESC( CInfoMapRegion )
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
	DEFINE_KEYFIELD( m_strLocToken, FIELD_STRING, "token" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CInfoMapRegion, DT_InfoMapRegion )
	SendPropFloat(SENDINFO( m_flRadius )),
	SendPropString(SENDINFO( m_szLocToken )),
END_SEND_TABLE()

int CInfoMapRegion::UpdateTransmitState( void )
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CInfoMapRegion::Spawn( void )
{
	V_strncpy( m_szLocToken.GetForModify(), STRING( m_strLocToken ), CInfoMapRegion::k_eMaxLocTokenLen );
}

