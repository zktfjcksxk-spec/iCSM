//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=====================================================================================//

#include "cbase.h"
#include "csgo_outofammo.h"
#include "IGameUIFuncs.h"
#include "panorama/ui_root.h"
#include "panorama/uijsregistration.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "clientsteamcontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_OutOfAmmo, CSGOOutOfAmmo );

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;
extern ConVar sv_outofammo_indicator;

CCSGO_OutOfAmmo::CCSGO_OutOfAmmo( panorama::CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_OutOfAmmo", this )
	, panorama::CPanel2D( pParent, pchID )
{
	RequireLoadLayout( "file://{resources}/layout/outofammo.xml" );
}

bool CCSGO_OutOfAmmo::ShouldDraw( void )
{
	return sv_outofammo_indicator.GetBool() &&
		cl_drawhud.GetBool() && 
		cl_draw_only_deathnotices.GetBool() == false && 
		CPanoramaHudElement::ShouldDraw() &&
		PlayerIsOutOfAmmo();
}

bool CCSGO_OutOfAmmo::PlayerIsOutOfAmmo( void )
{
	CCSPlayer *pLocalPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
	if ( !pLocalPlayer )
		return false;

	if ( !pLocalPlayer->IsAlive() && (pLocalPlayer->GetObserverMode() != OBS_MODE_IN_EYE || pLocalPlayer->GetObserverInterpState() == C_CSPlayer::OBSERVER_INTERP_TRAVELING) )
		return false;

	CCSPlayer* pHudPlayer = ToCSPlayer( CHudElement::GetLocalOrObservedPlayer( 0 ) );
	if ( !pHudPlayer || !pHudPlayer->IsAlive() )
		return false;

	CWeaponCSBase *pWeapon = pHudPlayer->GetActiveCSWeapon();
	if ( !pWeapon )
		return false;

	return (WeaponIsBallistic( pWeapon->GetWeaponType() ) && pWeapon->m_iClip1 == 0 && pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) == 0);
}

void CCSGO_OutOfAmmo::Think( void )
{
	bool bShouldDraw = ShouldDraw();

	if ( BIsVisible() != bShouldDraw )
	{
		SetVisible( bShouldDraw );
	}
}
