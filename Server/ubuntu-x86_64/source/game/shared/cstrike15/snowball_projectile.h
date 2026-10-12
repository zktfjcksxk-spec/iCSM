//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SNOWBALL_PROJECTILE_H
#define SNOWBALL_PROJECTILE_H
#ifdef _WIN32
#pragma once
#endif


#include "basecsgrenade_projectile.h"

#if defined( CLIENT_DLL )

class C_SnowballProjectile : public C_BaseCSGrenadeProjectile
{
	public:
	DECLARE_CLASS( C_SnowballProjectile, C_BaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	virtual bool Simulate( void );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_FIRE; }

	bool IsIncGrenade() const { return m_bIsIncGrenade; }

	protected:
	CNetworkVar( bool, m_bIsIncGrenade );

	private:
	CUtlReference<CNewParticleEffect> m_snowballParticleEffect;

};

#else // GAME_DLL

class CSnowballProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CSnowballProjectile, CBaseCSGrenadeProjectile );

	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

// Overrides.
public:
	CSnowballProjectile();

	virtual void Spawn();
	virtual void Precache();
	virtual void BounceTouch( CBaseEntity *other );
	virtual void BounceSound( void );
	virtual void Detonate();
	
	void	InputSetTimer( inputdata_t &inputdata );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_SNOWBALL; }

// Grenade stuff.
	static CSnowballProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		item_definition_index_t weaponItem );	
	
public:
	float m_flTimeToDetonate;

	// Count of players effected by the flash
	uint8 m_numOpponentsHit; // note: opponents are considered to be anybody not on the flasher's team.
	uint8 m_numTeammatesHit;
};

#endif // GAME_DLL

#endif // SNOWBALL_PROJECTILE_H
