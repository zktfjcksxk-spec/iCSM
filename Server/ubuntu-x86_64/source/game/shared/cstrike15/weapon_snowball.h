//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_SNOWBALL_H
#define WEAPON_SNOWBALL_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_basecsgrenade.h"


#ifdef CLIENT_DLL
	#define CSnowball C_Snowball
#endif


//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CSnowball : public CBaseCSGrenade
{
public:
	DECLARE_CLASS( CSnowball, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSnowball();
	virtual bool DropLiveGrenadeWhenOwnerDies() OVERRIDE{ return false; }

#ifdef CLIENT_DLL

#else
	DECLARE_DATADESC();

		virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer );
#endif

	CSnowball( const CSnowball & ) {}
};


#endif // WEAPON_SNOWBALL_H
