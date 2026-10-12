//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "weapon_csbasegun.h"
#include "cs_gamerules.h"

#if defined( CLIENT_DLL )
	#define CWeaponTaser C_WeaponTaser
	#define CWeaponPartyPopper  C_WeaponPartyPopper 
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define TASER_BIRTHDAY_PARTICLES	"weapon_confetti"
#define TASER_BIRTHDAY_SOUND		"Weapon_PartyHorn.Single"

ConVar mp_taser_recharge_time( "mp_taser_recharge_time", "-1", FCVAR_RELEASE|FCVAR_REPLICATED, "Determines recharge time for taser. -1 = disabled."  );

class CWeaponTaser : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponTaser, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponTaser();

	virtual void Spawn( void ) OVERRIDE;

	virtual void Precache() OVERRIDE;
	virtual void PrimaryAttack( void ) OVERRIDE;

	bool DoesRecharge( void ) const { return ( mp_taser_recharge_time.GetFloat() >= 0.0f ); }

#if defined( CLIENT_DLL )
	virtual const char* GetMuzzleFlashEffectName_1stPerson( void ) OVERRIDE;
	virtual const char* GetMuzzleFlashEffectName_3rdPerson( void ) OVERRIDE;
#endif

#if defined( GAME_DLL )
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void ItemPostFrame();
#endif

	virtual CSWeaponType GetWeaponType( void ) const OVERRIDE { return WEAPONTYPE_TASER; }
	float GetCharge( void ) OVERRIDE;

private:
	CWeaponTaser( const CWeaponTaser& );

	CNetworkVar( float, m_fFireTime );
	void Think_Recharge( void );
};




IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTaser, DT_WeaponTaser )

#ifdef GAME_DLL
BEGIN_NETWORK_TABLE( CWeaponTaser, DT_WeaponTaser )
SendPropFloat( SENDINFO( m_fFireTime ) ),
END_NETWORK_TABLE()
#else
BEGIN_NETWORK_TABLE( CWeaponTaser, DT_WeaponTaser )
RecvPropFloat( RECVINFO( m_fFireTime ) ),
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponTaser )
DEFINE_PRED_FIELD( m_fFireTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_taser, WeaponTaser );
PRECACHE_REGISTER( weapon_taser );

CWeaponTaser::CWeaponTaser() :
	m_fFireTime(0.0f)
{
}

float CWeaponTaser::GetCharge()
{
	if ( !DoesRecharge() )
		return 1.0f;

	if ( m_iClip1 > 0 )
		return 1.0f;

	return RemapValClamped( gpGlobals->curtime - m_fFireTime, 0, mp_taser_recharge_time.GetFloat(), 0, 0.97f );
}

void CWeaponTaser::Spawn()
{
	BaseClass::Spawn();

	Think_Recharge();
}

void CWeaponTaser::Precache()
{
	BaseClass::Precache();

	PrecacheParticleSystem( TASER_BIRTHDAY_PARTICLES );
	PrecacheScriptSound( TASER_BIRTHDAY_SOUND );
}

void CWeaponTaser::PrimaryAttack( void )
{
	if ( !CSBaseGunFire( GetCycleTime(), Primary_Mode ) )
		return;

	m_fFireTime = gpGlobals->curtime;

	CCSPlayer* pOwner = GetPlayerOwner();
	if ( pOwner && pOwner->IsPlayerGhost() )
	{
		CPASAttenuationFilter filter( this );
		filter.UsePredictionRules();
		EmitSound( filter, entindex(), "Player.GhostTaserShoot" );
	}
	else if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		//CPASAttenuationFilter filter( this, params.soundlevel );
		//EmitSound( filter, entindex(), TASER_BIRTHDAY_SOUND, &GetLocalOrigin(), 0.0f );

		CPASAttenuationFilter filter( this );
		filter.UsePredictionRules();
		EmitSound( filter, entindex(), TASER_BIRTHDAY_SOUND );
	}
}

#if defined( GAME_DLL )

bool CWeaponTaser::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( !DoesRecharge() )
	{
		if ( HasAmmo() == false )
		{
			// just drop it if it's out of ammo and we're trying to switch away
			GetPlayerOwner()->CSWeaponDrop( this );
		}
	}

	return BaseClass::Holster(pSwitchingTo);
}

void CWeaponTaser::ItemPostFrame()
{
	const float kTaserDropDelay = 0.5f;
	BaseClass::ItemPostFrame();

	if ( !DoesRecharge() )
	{
		if ( !HasAmmo() && gpGlobals->curtime >= m_fFireTime + kTaserDropDelay )
		{
			GetPlayerOwner()->Weapon_Drop( this, NULL, NULL );
		}
	}
}

#endif

#if defined( CLIENT_DLL )
const char* CWeaponTaser::GetMuzzleFlashEffectName_1stPerson( void )
{
	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		return TASER_BIRTHDAY_PARTICLES;
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPerson( GetEconItemView() );
	}
}

const char* CWeaponTaser::GetMuzzleFlashEffectName_3rdPerson( void )
{
	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		return TASER_BIRTHDAY_PARTICLES;
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPerson( GetEconItemView() );
	}
}
#endif


void CWeaponTaser::Think_Recharge( void )
{
#if defined( GAME_DLL )

	if ( DoesRecharge() )
	{
		if ( ( m_iClip1 == 0 ) && ( gpGlobals->curtime - m_fFireTime > mp_taser_recharge_time.GetFloat() ) )
		{
			m_iClip1++;
		}
	}

#endif

	SetContextThink( &CWeaponTaser::Think_Recharge, gpGlobals->curtime + 1.0f, "TASERTHINK" );
}



/*
class CWeaponPartyPopper : public CWeaponTaser
{
	public:
	DECLARE_CLASS( CWeaponPartyPopper, CWeaponTaser );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	//CWeaponPartyPopper();

	//virtual void PrimaryAttack( void );

	private:
	//CWeaponPartyPopper( const CWeaponPartyPopper& );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPartyPopper, DT_WeaponPartyPopper )

BEGIN_NETWORK_TABLE( CWeaponPartyPopper, DT_WeaponPartyPopper )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponPartyPopper )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_partypopper, WeaponPartyPopper );

void CWeaponPartyPopper::PrimaryAttack( void )
{
	BaseClass::PrimaryAttack();

	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		CPASAttenuationFilter filter( this );
		filter.UsePredictionRules();
		EmitSound( filter, entindex(), TASER_BIRTHDAY_SOUND );
		//EmitSound( filter, entindex(), "Weapon_MAC10.Single" );
	}
}
*/