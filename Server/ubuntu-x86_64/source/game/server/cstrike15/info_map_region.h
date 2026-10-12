//============ Copyright © Valve Corporation, All rights reserved. ============//
//
// Point entity with a radius to name regions of a map 
//
//=============================================================================//
#pragma once

#include "baseentity.h"

class CInfoMapRegion : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoMapRegion, CPointEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual int  UpdateTransmitState( void ) OVERRIDE;
	virtual void Spawn( void ) OVERRIDE;
private:
	enum { k_eMaxLocTokenLen = 128 };
	CNetworkVar( float, m_flRadius );
	string_t m_strLocToken; // string id for loc token from map 
	CNetworkString( m_szLocToken, k_eMaxLocTokenLen ); // string to be networked to client
};