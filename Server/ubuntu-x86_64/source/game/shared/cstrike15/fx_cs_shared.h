//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FX_CS_SHARED_H
#define FX_CS_SHARED_H
#ifdef _WIN32
#pragma once
#endif


#ifdef CLIENT_DLL
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
#endif


// This runs on both the client and the server.
// On the server, it only does the damage calculations.
// On the client, it does all the effects.
void FX_FireBullets( 
	int iEntIndex,						// Not necessarily a player; some map entities also fire bullets.
	CWeaponCSBase* pWeapon,				// Can be null, but if so, need to specify a weapon item definition
	const CEconItemView* pWeaponView,	// (Only required if pWeapon is null): weapon data for the weapon being fired
	const Vector &vOrigin,				// Where to fire from
	const QAngle &vAngles,				// Angle to fire at
	int	iMode,							// For weapons with multiple firing modes, which mode is firing (e.g. sniper rifle scoped is secondary)
	int iSeed,							// Random seed to use for inaccuracy calculations
	float fInaccuracy,					// Inaccuracy amount given current weapon user's state
	float fSpread,						// Spread amount given current weapon user's state
	float fAccuracyFishtail,			// Fishtail inaccuracy amount (if enabled)
	float flSoundTime,					// Time to start the weapon sound (if 0, instantly)
	WeaponSound_t sound_type,			// Type of weapon sound to emit
	float flRecoilIndex
	// $$$REI Revert as many changes to FX_FireBullets as possible, doesn't handle PVS properly on client
	);

// This runs on both the client and the server.
// On the server, it dispatches a TE_PlantBomb to visible clients.
// On the client, it plays the planting animation.
enum PlantBombOption_t
{
	PLANTBOMB_PLANT, // play the planting animation
	PLANTBOMB_ABORT, // abort the planting animation
	// NOTE: If you add additional items to this enum then m_option in CTEPlantBomb will need to have its SendPropInt setting changed to have more than one bit.
};
void FX_PlantBomb( int iPlayer, const Vector &vOrigin, PlantBombOption_t option );

#endif // FX_CS_SHARED_H
