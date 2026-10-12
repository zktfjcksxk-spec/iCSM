//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "gamerules.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "weapon_snowball.h"


#ifdef CLIENT_DLL


#else

	#include "cs_player.h"
	#include "items.h"
	#include "snowball_projectile.h"

#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define GRENADE_TIMER	6.0f //Seconds



IMPLEMENT_NETWORKCLASS_ALIASED( Snowball, DT_Snowball )

BEGIN_NETWORK_TABLE(CSnowball, DT_Snowball)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CSnowball )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_snowball, Snowball );
// PRECACHE_REGISTER( weapon_flashbang ); // No Precache override, so not needed.

CSnowball::CSnowball()
{
}

#ifndef CLIENT_DLL

	BEGIN_DATADESC( CSnowball )
	END_DATADESC()

	void CSnowball::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer )
	{
		CSnowballProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, GetItemDefinitionIndex() );
	}


#endif


