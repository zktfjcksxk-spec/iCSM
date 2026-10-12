//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Panorama port of scoreboard_scaleform.h
//
//=============================================================================//

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"

 #include "panorama/csgo_panorama_script_bindings.h"

#include "panorama/ui_root.h"
#include "gameui_interface.h"

#include "cs_shareddefs.h"

#include "cstrike15_item_inventory.h"
#include "panorama/uiinputcapture.h"

//#include "../uicomponents/uicomponent_loadout.h"


DECLARE_PANORAMA_EVENT0( OnOpenScoreboard );
DECLARE_PANORAMA_EVENT0( OnCloseScoreboard );

//
// Component
//
class CCSGO_Scoreboard : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_Scoreboard, panorama::CPanel2D );

public:

	CCSGO_Scoreboard( panorama::CPanel2D *pParent, const char *pchID );
	~CCSGO_Scoreboard();

	bool CloseScoreboard( void );
	void OpenScoreboard( void );

	static CCSGO_Scoreboard *GetInstance() { return s_pScoreboard; }

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// Called once per frame whether the element is visible or not
	virtual void Think( void ) OVERRIDE;

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event );

	// event handlers
	bool EventOnMouseEnableBinding( void );

	void SetCasterIsCameraman( int nAccountID );
	void SetCasterIsHeard( int nAccountID );
	void SetCasterControlsXray( int nAccountID );
	void SetCasterControlsUI( int nAccountID );

	bool IsLockedOpen() const { return m_bLockOpen; }

private:

	static CCSGO_Scoreboard *s_pScoreboard;

	bool m_bLockOpen;
	panorama::CGameInputCapture m_Capture;
};
