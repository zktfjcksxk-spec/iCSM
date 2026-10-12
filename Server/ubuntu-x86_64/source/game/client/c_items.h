//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ITEMS_H
#define C_ITEMS_H

#ifdef _WIN32
#pragma once
#endif

#include "c_baseanimating.h"
#include "glow_outline_effect.h"
#include "econ_entity.h"
// #include "entityoutput.h"
// #include "player_pickup.h"
// #include "vphysics/constraints.h"


class C_Item : public C_EconEntity
{
public:
	DECLARE_CLASS( C_Item, C_EconEntity );
	DECLARE_CLIENTCLASS();
	//DECLARE_PREDICTABLE();

	C_Item();
	virtual ~C_Item();

 	virtual void	Spawn( void );
	virtual void	OnDataChanged( DataUpdateType_t type );
	virtual void	ClientThink( void );

	virtual wchar_t* GetReticleHintText( void ) { return L""; }
	void UpdateOutlineGlow( void );
	CGlowObject m_GlowObject;

protected:
	CNetworkVar( bool, m_bShouldGlow );

	wchar_t			m_pReticleHintTextName[256];
};


class C_ItemAssaultSuitUseable : public C_Item
{
	public:
	DECLARE_CLASS( C_ItemAssaultSuitUseable, C_Item );
	DECLARE_CLIENTCLASS();

	virtual void	Spawn( void );
	virtual wchar_t* GetReticleHintText( void );

	CNetworkVar( int, m_nArmorValue );
	CNetworkVar( bool, m_bIsHeavyAssaultSuit );
};
#endif // C_ITEMS_H
