//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Display active quest status -- split from sfhud_uniquealerts.cpp
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudmissionpanel.h"

REGISTER_PANEL2D_FACTORY( CCSGO_HudMissionPanel, CSGOHudMissionPanel );


CCSGO_HudMissionPanel::CCSGO_HudMissionPanel( panorama::CPanel2D *pParent, const char *pchID )
	: BaseClass(pParent, pchID)
	, CPanoramaHudElement( "CCSGO_HudMissionPanel", this )
{
}

CCSGO_HudMissionPanel::~CCSGO_HudMissionPanel() = default;

