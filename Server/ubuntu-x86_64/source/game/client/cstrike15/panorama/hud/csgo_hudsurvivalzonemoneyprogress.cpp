//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_hudsurvivalzonemoneyprogress.h"
#include "c_baseplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DEFINE_PANORAMA_EVENT( SurvivalZoneExplorationProgress );
REGISTER_PANEL2D_FACTORY( CCSGO_HudSurvivalZoneMoneyProgress, CSGOSurvivalZoneMoneyProgress );

using namespace panorama;

CCSGO_HudSurvivalZoneMoneyProgress::CCSGO_HudSurvivalZoneMoneyProgress( panorama::CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( pchID, this )
	, CPanel2D( pParent, pchID )
{
	
	RequireLoadLayout( "file://{resources}/layout/survival/survival_zonemoneyprogress.xml" );
	m_hProgress = RequireChildInLayoutFile( "Progress" );
	RegisterForUnhandledEvent( SurvivalZoneExplorationProgress(), this, &CCSGO_HudSurvivalZoneMoneyProgress::EventExplorationProgress );
	RegisterForUnhandledEvent( ObserverTargetChanged(), this, &CCSGO_HudSurvivalZoneMoneyProgress::EventObserverTargetChanged );
	AddClass( "hidden" );
}

bool CCSGO_HudSurvivalZoneMoneyProgress::EventExplorationProgress( float flProgress )
{
	flProgress = clamp( flProgress, 0.0f, 1.0f );
	if ( flProgress == 1.0f )
	{
		if ( !BHasClass( "hidden" ) )
			AddClass( "hidden" );
	}
	else
	{
		RemoveClass( "hidden" );
		m_hProgress->AccessStyle()->SetHeight( CUILength( 100*flProgress, CUILength::k_EUILengthPercent ) );
	}
	return false;
}

bool CCSGO_HudSurvivalZoneMoneyProgress::EventObserverTargetChanged( C_BaseEntity* pTarget )
{
	if ( !BHasClass( "hidden" ) )
		AddClass( "hidden" );

	return false;
}
