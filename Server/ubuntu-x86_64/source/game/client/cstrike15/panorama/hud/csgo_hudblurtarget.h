//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "panorama/csgo_blurtarget.h"

//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Voice and Status message panel
//-----------------------------------------------------------------------------
class CCSGO_HudBlurTarget : public CCSGO_BlurTarget, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudBlurTarget, CCSGO_BlurTarget );

public:
	CCSGO_HudBlurTarget(panorama::CPanel2D *pParent, const char *pchID);
	virtual ~CCSGO_HudBlurTarget();

	// These overload the CHudElement class
	virtual void Think(void);
};
