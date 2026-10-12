//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CSGO_HUDAUTODISCONNECT_H
#define CSGO_HUDAUTODISCONNECT_H

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?

class CCSGO_HudAutoDisconnect : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudAutoDisconnect, panorama::CPanel2D );

public:
	CCSGO_HudAutoDisconnect( panorama::CPanel2D *pParent, const char *pchID );

	// CHudElement overrides
	virtual void ProcessInput( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;

private:
	panorama::CLabel* m_pTopLabel;
	panorama::CLabel* m_pBottomLabel;
	panorama::CImagePanel* m_pTimerIcon;
};

#endif	// CSGO_HUDAUTODISCONNECT_H