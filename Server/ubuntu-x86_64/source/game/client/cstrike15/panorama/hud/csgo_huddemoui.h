//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_HUDDEMOUI_H_
#define CSGO_HUDDEMOUI_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?

DECLARE_PANORAMA_EVENT0( DemoToggleUI );
DECLARE_PANORAMA_EVENT2( DemoPlaybackControl, const char*, float );

class CCSGO_HudDemoPlayback : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_HudDemoPlayback, panorama::CPanel2D );

public:
	CCSGO_HudDemoPlayback( panorama::CPanel2D* pParent, const char* pchID );
	virtual ~CCSGO_HudDemoPlayback();

	virtual void OnLayoutReloaded() OVERRIDE;

private:
	bool HandleDemoToggleUI();
	bool HandleDemoPlaybackControl( const char* szControlType, float flControlAmount );
	bool HandleFrameUpdate();
};

#endif