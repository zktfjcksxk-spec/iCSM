//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Display guardian mission status -- split from sfhud_uniquealerts.cpp
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudguardianpanel.h"

REGISTER_PANEL2D_FACTORY( CCSGO_HudGuardianPanel, CSGOHudGuardianPanel );

CCSGO_HudGuardianPanel::CCSGO_HudGuardianPanel( panorama::CPanel2D *pParent, const char *pchID )
	: BaseClass( pParent, pchID )
	, CPanoramaHudElement( "CCSGO_HudGuardianPanel", this )
{
}

CCSGO_HudGuardianPanel::~CCSGO_HudGuardianPanel() = default;

