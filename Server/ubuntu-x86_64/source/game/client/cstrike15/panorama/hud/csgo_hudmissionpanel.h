//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CSGO_HUDMISSIONPANEL_H_
#define CSGO_HUDMISSIONPANEL_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?

class CCSGO_HudMissionPanel : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudMissionPanel, panorama::CPanel2D );

public:
	CCSGO_HudMissionPanel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudMissionPanel();

};


#endif // CSGO_HUDMISSIONPANEL_H_
