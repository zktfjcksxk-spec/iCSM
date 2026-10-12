//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SNOWBALL_PILE_H
#define SNOWBALL_PILE_H
#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#include "c_props.h"
#else
#include "props.h"
#endif


#if defined( CLIENT_DLL )
#define CSnowballPile C_SnowballPile
#endif

class CSnowballPile : public CBaseAnimating
{
	public:
	DECLARE_CLASS( CSnowballPile, CBaseAnimating );
	DECLARE_NETWORKCLASS();

#if defined( CLIENT_DLL )

	public:
	virtual void OnDataChanged( DataUpdateType_t updateType );

	//C_SnowballPile( const C_SnowballPile & );
	~C_SnowballPile();

	CUtlReference<CNewParticleEffect> m_baseParticleEffect;
#else
	
	// Initialization
	CSnowballPile();

	DECLARE_DATADESC();

	virtual void		Precache();
	virtual void		Spawn();
	virtual int			ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; };

	void		SnowPileUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	private:

	void		EquipPlayer( CBaseEntity *pPlayer, const char *szWeapon = NULL );
#endif
};


#endif // SNOWBALL_PILE_H
