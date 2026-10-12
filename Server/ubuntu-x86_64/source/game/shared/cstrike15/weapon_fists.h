//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_FISTS_H
#define WEAPON_FISTS_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_csbase.h"

#if defined( CLIENT_DLL )
	#define CFists C_Fists
#endif

// ----------------------------------------------------------------------------- //
// CFists class definition.
// ----------------------------------------------------------------------------- //

class CFists : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CFists, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CFists();

	virtual bool HasPrimaryAmmo();
	virtual bool CanBeSelected();
	
	virtual bool CanHolster( void );

	virtual void Precache();

	void Spawn();

	void PrimaryAttack();
	
	void SecondaryAttack();

	virtual bool Deploy();

	bool CanDrop();

	void WeaponIdle();

	virtual CSWeaponType GetWeaponType( void ) const { return WEAPONTYPE_FISTS; }

	virtual bool SendWeaponAnim( int iActivity );

#ifndef CLIENT_DLL
	virtual void Touch( CBaseEntity *pOther );
#endif

#ifdef CLIENT_DLL
	virtual void WeaponPreRender( void ) OVERRIDE;
#endif

	void PlayUninterruptableActivity( Activity nAct );

private:
	
	void FistsPunch( bool bHard = false );

	bool IsUninterruptableAct( Activity nAct );
	CNetworkVar( bool, m_bPlayingUninterruptableAct );

#ifndef CLIENT_DLL
	bool m_bRestorePrevWep;
	CBaseCombatWeaponHandle m_hWeaponBeforePrevious;
	CBaseCombatWeaponHandle m_hWeaponPrevious;

	float m_flNextHardPunchHitTime;
	void FistsAttack( bool bHard = false );

	virtual void ItemPostFrame();
#endif

	void UpdatePoseParameter( void );

	CFists( const CFists & ) {}

};

#endif // WEAPON_FISTS_H
