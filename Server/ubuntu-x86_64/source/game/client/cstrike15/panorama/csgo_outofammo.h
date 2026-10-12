//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=====================================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"

class CCSGO_OutOfAmmo : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_OutOfAmmo, panorama::CPanel2D );

public:
	explicit CCSGO_OutOfAmmo( panorama::CPanel2D *pParent, const char *pchID );

	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void Think( void );

	bool PlayerIsOutOfAmmo( void );
};
