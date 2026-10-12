//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_MOLOTOV_H
#define WEAPON_MOLOTOV_H

#ifdef _WIN32
#pragma once
#endif

#include "weapon_basecsgrenade.h"

#if defined( CLIENT_DLL )
	#define CMolotovGrenade C_MolotovGrenade
	#define CIncendiaryGrenade C_IncendiaryGrenade
#endif

//-----------------------------------------------------------------------------
// Molotov grenades
//-----------------------------------------------------------------------------
class CMolotovGrenade : public CBaseCSGrenade
{
public:
	DECLARE_CLASS( CMolotovGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CMolotovGrenade() {}

#ifdef CLIENT_DLL
	virtual bool	Simulate( void );
	virtual void	UpdateParticles( void );
	virtual void	OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );
#else
	DECLARE_DATADESC();

	virtual void 	EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer );
	virtual void 	Precache( void );
#endif

	virtual void	Drop( const Vector& vecVelocity );

private:
	CMolotovGrenade( const CMolotovGrenade& );

private:
#if defined( CLIENT_DLL )
	CUtlReference<CNewParticleEffect> m_molotovParticleEffect;
#endif
};

//-----------------------------------------------------------------------------
// Incendiary grenades
//-----------------------------------------------------------------------------
class CIncendiaryGrenade : public CMolotovGrenade
{
public:
	DECLARE_CLASS( CIncendiaryGrenade, CMolotovGrenade );
	DECLARE_NETWORKCLASS();
	//DECLARE_PREDICTABLE();

	CIncendiaryGrenade() {}

#ifdef CLIENT_DLL
	//virtual bool	Simulate( void );
	//virtual void	UpdateParticles( void );
	//virtual void	OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );
#else
	//DECLARE_DATADESC();

	virtual void	EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer );
	virtual void 	Precache( void );
#endif

private:
	CIncendiaryGrenade( const CIncendiaryGrenade& );

	/*
private:
#if defined( CLIENT_DLL )
	CUtlReference<CNewParticleEffect> m_molotovParticleEffect;
#endif
	*/
};

#endif // WEAPON_MOLOTOV_H
