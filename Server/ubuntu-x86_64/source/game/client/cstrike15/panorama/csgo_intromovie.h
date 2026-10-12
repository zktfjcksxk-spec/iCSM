//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/ui_root.h"
#include "gameui_interface.h"

class CCSGO_IntroMovie;

DECLARE_PANORAMA_EVENT0( CSGOShowIntroMovie );
DECLARE_PANORAMA_EVENT0( CSGOHideIntroMovie );

//-----------------------------------------------------------------------------
// Purpose: CSGO Intro Movie
//-----------------------------------------------------------------------------
class CCSGO_IntroMovie : public CUI_Root, public ICSGOGameUIStateListener
{
	DECLARE_PANEL2D( CCSGO_IntroMovie, CUI_Root );

public:
	CCSGO_IntroMovie( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_IntroMovie();

	virtual void OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState ) OVERRIDE;

	static CCSGO_IntroMovie *GetInstance() { return s_pIntroMovie; }

private:

	bool EventHideIntroMovie( void );

private:

	panorama::CPanel2D *m_pSteamNotificationsPlaceholderPanel;

	static CCSGO_IntroMovie *s_pIntroMovie;

};
