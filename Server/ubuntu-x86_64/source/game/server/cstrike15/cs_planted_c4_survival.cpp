//========= Copyright  2018, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "cs_planted_c4_survival.h"
#include "cs_gamerules.h"
#include "func_bomb_target.h"
#include "mapinfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

///////////////////////////////////////////////////////////
// Survival version of C4 - doesn't end round
///////////////////////////////////////////////////////////

LINK_ENTITY_TO_CLASS( planted_c4_survival, CPlantedC4Survival );
PRECACHE_REGISTER( planted_c4_survival );

BEGIN_PREDICTION_DATA( CPlantedC4Survival )
END_PREDICTION_DATA()

BEGIN_DATADESC( CPlantedC4Survival )
END_DATADESC()

void CPlantedC4Survival::OnDefuse( CCSPlayer* pDefuser )
{
	// TODO: What if there are multiple c4s?
	CSGameRules()->m_bBombDropped = false;
	CSGameRules()->m_bBombPlanted = false;

	// survival c4's disappear when they are defused too
	UTIL_Remove( this );
}

void CPlantedC4Survival::OnExplode( trace_t *pGroundTrace )
{
	// Output to the bomb target ent
	if ( CBombTarget* pBombTarget = GetBombTarget() )
	{
		pBombTarget->AcceptInput( "BombExplode", this, this, variant_t(), 0 );
	}

	DoExplosionEffects( pGroundTrace );

	// Do the Damage
	const float kSurvivalBombDamage = 700; // in our implementation radius is proportional to damage, but you take less damage the further you are from the center
	DoExplosionDamage( g_pMapInfo ? g_pMapInfo->m_flBombRadius : kSurvivalBombDamage );

	// survival c4's disappear when they explode
	UTIL_Remove( this );
}


