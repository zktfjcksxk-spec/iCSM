//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "panorama/controls/panelptr.h"

DECLARE_PANORAMA_EVENT1( SurvivalZoneExplorationProgress, float );

//-----------------------------------------------------------------------------
// Purpose: 
//          
//-----------------------------------------------------------------------------
class CCSGO_HudSurvivalZoneMoneyProgress : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudSurvivalZoneMoneyProgress, panorama::CPanel2D);

public:
	CCSGO_HudSurvivalZoneMoneyProgress(panorama::CPanel2D *pParent, const char *pchID);

private:
	bool EventExplorationProgress( float flProgress );
	bool EventObserverTargetChanged( C_BaseEntity* pTarget );
	panorama::CPanelPtr< panorama::CPanel2D > m_hProgress;
};
