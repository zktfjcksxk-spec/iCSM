//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_MELEE_H
#define WEAPON_MELEE_H

#ifdef _WIN32
#pragma once
#endif

#include "weapon_csbase.h"

#if defined( CLIENT_DLL )
	#define CMelee C_Melee
#endif


// ----------------------------------------------------------------------------- //
// CMelee class definition.
// ----------------------------------------------------------------------------- //

class CMelee : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CMelee, CWeaponCSBase );
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif // GAME_DLL
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CMelee();

	// We say yes to this so the weapon system lets us switch to it.
	virtual bool HasPrimaryAmmo() { return true; }
	virtual bool CanBeSelected() { return true; }

	virtual void Precache();

	void Spawn();

	void PrimaryAttack();
	void SecondaryAttack();
	virtual bool Deploy();
	virtual void UpdateShieldState( void );

	virtual CSWeaponType GetWeaponType( void ) const { return WEAPONTYPE_MELEE; }

#ifndef CLIENT_DLL
	bool DidThrowHitPlayer( CCSPlayer* pHitPlayer );
#endif

private:
	CMelee( const CMelee & ) {}

	CNetworkVar( float, m_flThrowAt );

#ifndef CLIENT_DLL
	void ThrowWeapon();

	bool m_swingLeft;
	EHANDLE m_hThrower;
#endif

};

#endif // WEAPON_MELEE_H
