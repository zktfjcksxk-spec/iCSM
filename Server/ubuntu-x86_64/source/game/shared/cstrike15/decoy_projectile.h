//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DECOY_PROJECTILE_H
#define DECOY_PROJECTILE_H


#if CSTRIKE_TRUNK_BUILD
#define DECOY_ALWAYS_NEGEV 1
#else
#define DECOY_ALWAYS_NEGEV 0
#endif


#ifdef _WIN32
#pragma once
#endif


#include "basecsgrenade_projectile.h"


#if defined( CLIENT_DLL )

class C_DecoyProjectile : public C_BaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( C_DecoyProjectile, C_BaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	virtual bool Simulate( void );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_DECOY; }

	float GetParticleEffectTimeSinceSpawn() const;

private:
	CUtlReference<CNewParticleEffect> m_decoyParticleEffect;
	float m_flTimeParticleEffectSpawn;

};

#else // GAME_DLL

struct DecoyWeaponProfile;

DECLARE_AUTO_LIST( IDecoyProjectile )
class CDecoyProjectile : public CBaseCSGrenadeProjectile, public IDecoyProjectile
{
public:
	DECLARE_CLASS( CDecoyProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();
	IMPLEMENT_AUTO_LIST_GET();

// Overrides.
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Detonate( void );
	virtual void BounceSound( void );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_DECOY; }

// Grenade stuff.
	static CDecoyProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		item_definition_index_t weaponItem );

private:
	void Think_Detonate( void );
	void GunfireThink( void );
	void SetTimer( float timer );

	int m_shotsRemaining;
	float m_fExpireTime;
	DecoyWeaponProfile*	m_pProfile;
	item_definition_index_t m_decoyWeaponDefIndex;
	WeaponSound_t m_decoyWeaponSoundType;
};

#endif // GAME_DLL

#endif // DECOY_PROJECTILE_H
