//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_BREACHCHARGE_H
#define WEAPON_BREACHCHARGE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_csbase.h"

#include "basegrenade_shared.h"

#if defined( CLIENT_DLL )
	#define CBreachCharge C_BreachCharge
#endif

// ----------------------------------------------------------------------------- //
// CBreachCharge class definition.
// ----------------------------------------------------------------------------- //

class CBreachCharge : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CBreachCharge, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CBreachCharge();

	virtual bool HasPrimaryAmmo();
	virtual bool CanBeSelected();
	void Spawn();
	void PrimaryAttack();
	void SecondaryAttack();
	virtual bool Deploy();
	void WeaponIdle();

	virtual CSWeaponType GetWeaponType( void ) const { return WEAPONTYPE_BREACHCHARGE; }

	CBreachCharge( const CBreachCharge & ) {}

	virtual void SetModel( const char *szModelName );

#ifndef CLIENT_DLL
	void RemoveThink( void );
	bool ExtractAmmoFromOther( CBreachCharge* pOther );
	bool HasAnyLiveCharges( void );
#else

	virtual void WeaponPreRender( void );
private:
	IMaterial *m_pIconMaterial; 

#endif


};



#ifdef CLIENT_DLL
	#define CBreachChargeProjectile C_BreachChargeProjectile
#endif

class CBreachChargeProjectile : public CBaseGrenade
{
public:
	DECLARE_CLASS( CBreachChargeProjectile, CBaseGrenade );
	
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CBreachChargeProjectile();

	virtual void Spawn();
	virtual void Precache();

	CNetworkVar( bool, m_bShouldExplode );
	CNetworkVar( EHANDLE, m_weaponThatThrewMe );
	CNetworkVar( int, m_nParentBoneIndex );
	CNetworkVar( Vector, m_vecParentBonePos );
	
	const bool DoesDetonatorStillExist( void ) { return m_weaponThatThrewMe.Get() != NULL; }
	const bool GetCanBeDetonatedBy( CBreachCharge* pBreachChargeWeapon ) { return m_weaponThatThrewMe.Get() == pBreachChargeWeapon; }
	const void SetCanBeDetonatedBy( CBreachCharge* pBreachChargeWeapon ) { m_weaponThatThrewMe = pBreachChargeWeapon; }

	inline void GiveAnyKillCreditTo( CBaseCombatCharacter* pPlayer ) { SetThrower( pPlayer ); }

	//CBreachChargeProjectile( const CBreachChargeProjectile& ) {}
	//virtual ~CBreachChargeProjectile();

#ifndef CLIENT_DLL
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool m_bDefused;
	bool m_bUnstuckFromPlayer;

	virtual int OnTakeDamage( const CTakeDamageInfo &info );
#endif

#ifdef CLIENT_DLL
	
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual void OnDataChanged( DataUpdateType_t updateType );

#else

	static CBreachChargeProjectile* Create( const Vector &position, const Vector &velocity, CBaseCombatCharacter *pOwner, CBreachCharge *pDetonator );

	void AlignAnglesTo( const Vector vecTargetDir );

	void StuckToSurfaceThink( void );
	void ExplodeThink( void );

	virtual bool ShouldRemoveOnParentRemoval( void );

	virtual void BounceTouch( CBaseEntity *other );

	virtual void Detonate( void );
	void SetArmed( void );

	bool m_bResolvedParent;

	Vector m_vecLastKnownValidPos;

	CBaseEntity* m_pDesiredParent;

	int m_iParentClass;

#endif
};

#endif // WEAPON_BREACHCHARGE_H
