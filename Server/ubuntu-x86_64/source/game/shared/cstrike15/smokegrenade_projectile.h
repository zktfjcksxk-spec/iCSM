//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SMOKEGRENADE_PROJECTILE_H
#define SMOKEGRENADE_PROJECTILE_H
#ifdef _WIN32
#pragma once
#endif

#include "basecsgrenade_projectile.h"


#define SMOKE_GRENADE_FULL_OPACITY_DURATION 12.5f
// Other timings start counting *after* full opacity duration completes
// additionally the entity adds +1.0 sec before it is UTIL_Remove'd
#define SMOKE_GRENADE_FADE_TIMING_DURATION 4.55f
#define SMOKE_GRENADE_FADE_FPV_OVERLAY_DURATION 4.25f
#define SMOKE_GRENADE_EXTINGUISHFIRE_TIMING_UNTIL 4.80f


#if defined( CLIENT_DLL )

class C_SmokeGrenadeProjectile : public C_BaseCSGrenadeProjectile
{
	public:
	DECLARE_CLASS( C_SmokeGrenadeProjectile, C_BaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	C_SmokeGrenadeProjectile() {}
	virtual ~C_SmokeGrenadeProjectile();

	virtual void PostDataUpdate( DataUpdateType_t updateType ) OVERRIDE;
	virtual void OnDataChanged( DataUpdateType_t updateType ) OVERRIDE;

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_SMOKE; }
	void SpawnSmokeEffect();

	CNetworkVar( int, m_nSmokeEffectTickBegin );
	CNetworkVar( bool, m_bDidSmokeEffect );

	bool m_bSmokeEffectSpawned;
};

#else // GAME_DLL
class CSmokeGrenadeProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CSmokeGrenadeProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

public:
	DECLARE_DATADESC();
// Overrides.
public:
	virtual void Spawn();
	virtual void Precache();
	virtual void Detonate();
	virtual void OnBounced( void );
	virtual void BounceSound( void );

	void Think_Detonate();
	void Think_Fade();
	void Think_Remove();

	void SmokeDetonate( void );
	void RemoveGrenadeFromLists( void );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_SMOKE; }

	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }

// Grenade stuff.
public:

	static CSmokeGrenadeProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		item_definition_index_t weaponItem );

	void SetTimer( float timer );

	CNetworkVar( int, m_nSmokeEffectTickBegin );
	CNetworkVar( bool, m_bDidSmokeEffect );
	Vector m_vSmokeColor;
	float m_flLastBounce;
};
#endif // GAME_DLL

#endif // SMOKEGRENADE_PROJECTILE_H
