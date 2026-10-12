//========= Copyright © 2018, Valve Corporation, All rights reserved. ============//
//
// Purpose: Survival mode planted bomb
//
//=============================================================================//

#ifndef CS_PLANTED_C4_SURVIVAL_H
#define CS_PLANTED_C4_SURVIVAL_H
#ifdef _WIN32
#pragma once
#endif

#include "cstrike15/weapon_c4.h"

class CPlantedC4Survival : public CPlantedC4
{
	DECLARE_CLASS( CPlantedC4Survival, CPlantedC4 );
	DECLARE_DATADESC();
	DECLARE_PREDICTABLE();
	//DECLARE_SERVERCLASS(); // This networks to clients as a CPlantedC4; it has no additional netvars and there is no C_PlantedC4Survival

protected:
	virtual void OnDefuse( CCSPlayer* pDefuser ) OVERRIDE;
	virtual void OnExplode( trace_t *pGroundTrace ) OVERRIDE;

	// In survival mode, player's don't radio when too close to detonation
	virtual void RadioAboutToExplode( int teamNumber ) OVERRIDE {};
};

#endif // CS_PLANTED_C4_SURVIVAL_H