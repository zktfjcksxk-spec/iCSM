//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/ui_root.h"
#include "gameui_interface.h"

class CCSGO_MainMenu;
namespace panorama
{
	class CMoviePlayer;
}

DECLARE_PANORAMA_EVENT0( CSGOSettings );
DECLARE_PANORAMA_EVENT0(CSGOMainMenuUpdate);
DECLARE_PANORAMA_EVENT0( PanoramaMouseEnable );

//-----------------------------------------------------------------------------
// Purpose: CSGO Main Menu
//			Note that main menu is used as the pause menu as well
//-----------------------------------------------------------------------------
class CCSGO_MainMenu : public CUI_Root, public ICSGOGameUIStateListener
{
	DECLARE_PANEL2D( CCSGO_MainMenu, CUI_Root );

public:
	CCSGO_MainMenu(panorama::CPanel2D *pParent, const char *pchID);
	virtual ~CCSGO_MainMenu();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	virtual void OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState ) OVERRIDE;

	void Update( void );

	static CCSGO_MainMenu *GetInstance() { return s_pMainMenu; }

	// CGameEventListener
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

private:

	// Methods exposed to javascript
	bool IsMultiplayer();
	bool IsTraining();
	bool IsGotvSpectating();

	bool EventQuitClicked( void );
	bool EventOnQuitConfirmed( void );
	bool EventSetPopupBackgroundBlur( bool bEnable );
	bool EventResumeGame( void );
	bool EventDisconnect( void );
	bool EventDisconnectConfirmed( void );
	bool EventSwitchTeams( void );

	void LoadBackgroundMovie();
	void UnloadBackgroundMovie();
	void LoadVanityPanel();
	void UnloadVanityPanel();

private:

	panorama::CPanel2D *m_pSteamNotificationsPlaceholderPanel;

	static CCSGO_MainMenu *s_pMainMenu;

	bool m_bIsInGame;
	bool m_bInitialDisplay;

	panorama::CPanel2D* m_pMainMenuMovieParent;
	panorama::CMoviePlayer* m_pMainMenuMovie;
	panorama::CPanel2D* m_pVanityPanelParent;
	panorama::CPanel2D* m_pVanityPanel;
	panorama::CPanel2D *m_pInputPanel;	// Panel having focus
	panorama::CPanelPtr< panorama::CPanel2D > m_pDisconnectPopup;

	uint64 m_hDenyInputToGame;
};
