//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_mainmenu.h"

#include "csgo_popup_manager.h"
#include "panorama/ui_context_menu_manager.h"
#include "csgo_ui_tooltip_manager.h"
#include "panorama/controls/movieplayer.h"
#include "panorama/popups/ui_popup_generic.h"
#include "csgo_teamselectmenu.h"

#include "IGameUIFuncs.h"
#include "cs_gamerules.h"
#include "clientmode_csnormal.h"
#include "c_cs_playerresource.h"

#include "uicomponents/uicomponent_settings.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_MainMenu, CSGOMainMenu )

//
// Panorama event declarations
//

DECLARE_PANORAMA_EVENT0( CSGOShowMainMenu );
DECLARE_PANORAMA_EVENT0( CSGOHideMainMenu );
DECLARE_PANORAMA_EVENT0( CSGOQuit );
DECLARE_PANORAMA_EVENT0( CSGOQuitConfirmed );

// Pause menu mode
DECLARE_PANORAMA_EVENT0( CSGOShowPauseMenu );
DECLARE_PANORAMA_EVENT0( CSGOHidePauseMenu );
DECLARE_PANORAMA_EVENT0( CSGOMainMenuResumeGame );
DECLARE_PANORAMA_EVENT0( CSGOMainMenuDisconnect );
DECLARE_PANORAMA_EVENT0( CSGOMainMenuDisconnectConfirmed );
DECLARE_PANORAMA_EVENT0( CSGOMainMenuSwitchTeams );


//
// Panorama event definitions
//

DEFINE_PANORAMA_EVENT( CSGOSettings );
DEFINE_PANORAMA_EVENT( CSGOShowMainMenu );
DEFINE_PANORAMA_EVENT( CSGOHideMainMenu );
DEFINE_PANORAMA_EVENT( CSGOQuit );
DEFINE_PANORAMA_EVENT( CSGOQuitConfirmed );
DEFINE_PANORAMA_EVENT( CSGOMainMenuUpdate );

DEFINE_PANORAMA_EVENT( CSGOShowPauseMenu );
DEFINE_PANORAMA_EVENT( CSGOHidePauseMenu );
DEFINE_PANORAMA_EVENT( CSGOMainMenuResumeGame );
DEFINE_PANORAMA_EVENT( CSGOMainMenuDisconnect );
DEFINE_PANORAMA_EVENT( CSGOMainMenuDisconnectConfirmed );
DEFINE_PANORAMA_EVENT( CSGOMainMenuSwitchTeams );


using namespace panorama;

//-----------------------------------------------------------------------------
// Static data members
//-----------------------------------------------------------------------------
/*static*/ CCSGO_MainMenu *CCSGO_MainMenu::s_pMainMenu = NULL;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSGO_MainMenu::CCSGO_MainMenu(CPanel2D *pParent, const char *pchID)
:
	CUI_Root( pParent, pchID ),
	m_hDenyInputToGame( 0 )
{
	Assert(s_pMainMenu == NULL);
	s_pMainMenu = this;

	RequireLoadLayout( "file://{resources}/layout/mainmenu.xml" );


	// Tell the root about these controls
	SetPopupManager( panorama::panel_cast<CCSGO_PopupManager *>( RequireChildInLayoutFile( "PopupManager" ), true ) );
	SetTooltipManager( panorama::panel_cast<CCSGO_UI_TooltipManager *>( RequireChildInLayoutFile( "TooltipManager" ), true ) );
	SetContextMenuManager( panorama::panel_cast<CUI_ContextMenuManager *>( RequireChildInLayoutFile( "ContextMenuManager" ), true ) );

	/*
	CPanel2D* pBrowser = RequireChildInLayoutFile("CSGOBlogPanel");
	if ( pBrowser )
	{
		pBrowser->SetAcceptsInput(true);
		pBrowser->SetAcceptsFocus(true);
	}
	*/

	m_bIsInGame = false;

	m_pSteamNotificationsPlaceholderPanel = RequireChildInLayoutFile( "SteamNotificationsPlaceholder" );
	m_pMainMenuMovieParent = RequireChildInLayoutFile( "MainMenuMovieParent" );
	m_pMainMenuMovie = nullptr;
	m_pVanityPanelParent = RequireChildInLayoutFile( "MainMenuVanityParent" );
	m_pVanityPanel = nullptr;
	m_pInputPanel = RequireChildInLayoutFile( "MainMenuInput" );

	m_pInputPanel->SetAcceptsInput( true );
	m_pInputPanel->SetAcceptsFocus( true );
	m_pInputPanel->SetInputNamespace( "CSGO_mainmenu" );

	// set up events
	RegisterEventHandler(CSGOQuit(), this, &CCSGO_MainMenu::EventQuitClicked);
	RegisterEventHandler(CSGOQuitConfirmed(), this, &CCSGO_MainMenu::EventOnQuitConfirmed);

	RegisterEventHandler( SetPopupBackgroundBlur(), this, &CCSGO_MainMenu::EventSetPopupBackgroundBlur );

	RegisterEventHandler( CSGOMainMenuResumeGame(), this, &CCSGO_MainMenu::EventResumeGame );
	RegisterEventHandler( CSGOMainMenuDisconnect(), this, &CCSGO_MainMenu::EventDisconnect );
	RegisterEventHandler( CSGOMainMenuDisconnectConfirmed(), this, &CCSGO_MainMenu::EventDisconnectConfirmed );
	RegisterEventHandler( CSGOMainMenuSwitchTeams(), this, &CCSGO_MainMenu::EventSwitchTeams );

	GameUI().RegisterGameUIStateListener( this );
	OnCSGOGameUIStateChange( CSGO_GAME_UI_STATE_INVALID, GameUI().GetGameUIState() );

	// FIXME: This allows deferred commands such as item preview or match download to start, but we may not have those panels loaded and listening for events yet. 
	extern float g_flReadyToCheckForPCBootInvite;
	g_flReadyToCheckForPCBootInvite = Plat_FloatTime();

	m_bInitialDisplay = true;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_MainMenu::~CCSGO_MainMenu()
{
	if ( m_hDenyInputToGame )
	{
		gameuifuncs->PanoramaReleaseDenyAllInputToGame( m_hDenyInputToGame );
		m_hDenyInputToGame = 0;
	}

	GameUI().UnregisterGameUIStateListener( this );

	Assert(s_pMainMenu == this);
	s_pMainMenu = NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::Update(void)
{
	IUIWindow *pWindow = GetParentWindow();
	if ( pWindow && pWindow->BIsVisible() )
	{
		DispatchEvent(CSGOMainMenuUpdate(), NULL);

		if ( m_bInitialDisplay )
		{
			RemoveClass( "InitialDisplay" );
			m_bInitialDisplay = false;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState )
{
	bool bWasVisible = GetParentWindow()->BIsVisible();
	DevMsg( "CCSGO_MainMenu - bWasVisible=%s\n", ( bWasVisible ? "true" : "false" ) );
	bool bVisible = false;

	// Hide main menu panel if previous state was CSGO_GAME_UI_STATE_MAINMENU or CSGO_GAME_UI_STATE_PAUSEMENU

	if ( nOldState == CSGO_GAME_UI_STATE_MAINMENU )
	{
		bVisible = false;
		DispatchEvent( CSGOHideMainMenu(), NULL );

		UnloadVanityPanel();
		UnloadBackgroundMovie();
	}
	else if ( nOldState == CSGO_GAME_UI_STATE_PAUSEMENU )
	{
		bVisible = false;
		DispatchEvent( CSGOHidePauseMenu(), NULL );
	}

	// Show main menu panel is the new state is CSGO_GAME_UI_STATE_MAINMENU or CSGO_GAME_UI_STATE_PAUSEMENU

	if ( nNewState == CSGO_GAME_UI_STATE_MAINMENU )
	{
		bVisible = true;
		LoadBackgroundMovie();
		LoadVanityPanel();

		// first time we show main menu we want to make sure to update user keybindings (like scaleform did)
		UTIL_UpdateKeyBindings();

		DispatchEvent( CSGOShowMainMenu(), NULL );
	}
	else if ( nNewState == CSGO_GAME_UI_STATE_PAUSEMENU )
	{
		bVisible = true;
		DispatchEvent( CSGOShowPauseMenu(), NULL );
	}

	if ( bVisible != bWasVisible )
	{
		if ( bVisible )
		{
			AssertMsgAlways( !m_hDenyInputToGame, "MainMenu - Make sure to call PanoramaReleaseDenyAllInputToGame before calling PanoramaAddDenyAllInputToGame again\n" );
			m_hDenyInputToGame = gameuifuncs->PanoramaAddDenyAllInputToGame( this->UIPanel(), "MainMenu" );
			m_pInputPanel->SetFocus();
		}
		else
		{
			// HACK - Calling SetInputFocusContext( NULL ) to clear hover data.
			// Artificially reseting mouse position to (0, 0) in order to avoid using 
			// last mouse position when the main menu top level window becomes 
			// visible again 
			// (JIRA CSGO-1325 tooltips persisting across pause menu dismissal)
			GetParentWindow()->UIWindowInput()->SetInputFocusContext( NULL );
			GetParentWindow()->UIWindowInput()->OnMouseMove( 0.0f, 0.0f );
			
			GetParentWindow()->UIWindowInput()->SetInputFocus( NULL, false, false );
			if ( m_hDenyInputToGame )
			{
				gameuifuncs->PanoramaReleaseDenyAllInputToGame( m_hDenyInputToGame );
				m_hDenyInputToGame = 0;
			}
		}

		GetParentWindow()->SetVisible( bVisible );
	}
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOQuitConfirmed" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventOnQuitConfirmed(void)
{
	// do not explicitly abandon the lobby ( if we were connected to one ), the user may be restarting the client.

#ifdef CSGO_PORT
	g_pHostStateMgr->RequestHS_Quit();
#else
	engine->ClientCmd_Unrestricted("quit");
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOQuit" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventQuitClicked(void)
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "SetPopupBackgroundBlur" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventSetPopupBackgroundBlur( bool bEnable )
{
	SetHasClass( "PopupBackgroundBlur", bEnable );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOMainMenuResumeGame" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventResumeGame()
{
	GameUI().HideGameUI();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOMainMenuDisconnect" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventDisconnect()
{
	// Open Disconnection confirmation dialog box

	CUI_PopupManager *pPopupManager = GetPopupManager();
	bool bGameIsOver = ( !CSGameRules() || ( CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED ) );
	if ( pPopupManager && !bGameIsOver )
	{
		char const *szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_Title";
		char const *szMessageDefault = "#SFUI_PauseMenu_ExitGameConfirmation_Message";
		char const *szMessage = szMessageDefault;
		if ( engine->IsHLTV() || engine->IsPlayingDemo() )
		{
			szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleWatch";
			szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageWatch";

			if ( engine->GetDemoPlaybackParameters() && engine->GetDemoPlaybackParameters()->m_bAnonymousPlayerIdentity )
			{
				szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleOverwatch";
				szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageOverwatch";
			}
		}
		else if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
		{
			szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleQueuedMatchmaking";
			szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageQueuedMatchmaking";

			if ( CSGameRules()->IsPlayingCooperativeGametype() )
			{
				szTitle = "#SFUI_PauseMenu_ExitGameConfirmation_TitleQueuedGuardian";
				szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageQueuedGuardian";
			}
		}

		if ( ( szMessage == szMessageDefault ) && CSGameRules() && CSGameRules()->IsQuestEligible()
			&& ( CSGameRules()->GetGamePhase() != GAMEPHASE_MATCH_ENDED )
			&& !CSGameRules()->IsWarmupPeriod() )
		{
			// See if we have a mission progress?
			bool bMissionProgress = false;
			for ( uint32 i = ClientModeCSNormal::sm_mapQuestProgressUncommitted.FirstInorder();
				i != ClientModeCSNormal::sm_mapQuestProgressUncommitted.InvalidIndex();
				i = ClientModeCSNormal::sm_mapQuestProgressUncommitted.NextInorder( i ) )
			{
				if ( ClientModeCSNormal::sm_mapQuestProgressUncommitted.Element( i ).m_numNormalPoints > 0 )
				{
					bMissionProgress = true;
					break;
				}
			}

			if ( bMissionProgress )
			{
				szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageMission";
			}
			else
			{
				// Check if local user has non-zero score?
				C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
				C_CS_PlayerResource *cs_PR = static_cast<C_CS_PlayerResource *>( g_PR );
				if ( pLocalPlayer && cs_PR )
				{
					if ( cs_PR->GetScore( pLocalPlayer->entindex() ) > 0 )
					{
						szMessage = "#SFUI_PauseMenu_ExitGameConfirmation_MessageXP";
					}
				}
			}
		}

		CUI_Popup_Generic *pPopup = new CUI_Popup_Generic( pPopupManager, nullptr, this );
		pPopup->SetDisplayYesNo( szTitle, szMessage, "CSGOMainMenuDisconnectConfirmed()", nullptr );
		pPopupManager->ShowPopup( pPopup );

		// Keep track of the open popup so that we can close it if we are getting
		// the "cs_game_disconnected" game event
		m_pDisconnectPopup = pPopup;
	}
	else
	{
		EventDisconnectConfirmed();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOMainMenuDisconnectConfirmed" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventDisconnectConfirmed()
{
	m_pDisconnectPopup = nullptr;

// 	extern ConVar gotv_theater_container;	// Disconnecting from a game stops GOTV theater
// 	gotv_theater_container.SetValue( "" );

	engine->ClientCmd_Unrestricted( "disconnect" );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: "CSGOMainMenuSwitchTeams" event handler
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::EventSwitchTeams()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer && pPlayer->CanShowTeamMenu() )
	{

		// delay team select until after we close pause menu so that the DenyInputToGame calls don't overlap
		panorama::DispatchEventAsync( 0.1f, CSGOShowTeamSelectMenu(), ( const panorama::IUIPanelClient* )nullptr, true );

		GameUI().HideGameUI();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::FireGameEvent(IGameEvent *event)
{
	BaseClass::FireGameEvent( event );

	if ( !V_stricmp( event->GetName(), "cs_game_disconnected" ) )
	{
		CUI_Popup *pDisconnectPopup = panorama::panel_cast<CUI_Popup*>( m_pDisconnectPopup.Get() );
		CUI_PopupManager *pPopupManager = GetPopupManager();
		if ( pDisconnectPopup && pPopupManager )
		{
			pPopupManager->CloseIfVisible( pDisconnectPopup, true );
			m_pDisconnectPopup = nullptr;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Expose JS members
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "IsMultiplayer", PANORAMA_DELEGATE( &CCSGO_MainMenu::IsMultiplayer ) );
	RegisterJSMethod( "IsTraining", PANORAMA_DELEGATE( &CCSGO_MainMenu::IsTraining ) );
	RegisterJSMethod( "IsGotvSpectating", PANORAMA_DELEGATE( &CCSGO_MainMenu::IsGotvSpectating ) );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::IsMultiplayer()
{
	bool bMultiplayer = engine->IsInGame() && !engine->IsHLTV();
	if ( bMultiplayer &&
		engine->IsClientLocalToActiveServer() && ( !g_pMatchFramework ||
			!g_pMatchFramework->GetMatchSession() ||
			V_stricmp( g_pMatchFramework->GetMatchSession()->GetSessionSettings()->GetString( "system/network" ), "LIVE" ) )
		)
	{
		bMultiplayer = false;
	}

	return bMultiplayer;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::IsTraining()
{
	bool bTraining = CSGameRules() && CSGameRules()->IsPlayingTraining();

	return bTraining;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSGO_MainMenu::IsGotvSpectating()
{
	bool bQ = ( engine->IsHLTV() || engine->IsPlayingDemo() );

	return bQ;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::LoadBackgroundMovie()
{
	m_pMainMenuMovieParent->RequireLoadLayoutSnippet( "MainMenuMovieSnippet" );
	m_pMainMenuMovie = panorama::panel_cast<CMoviePlayer *>(m_pMainMenuMovieParent->FindChildInLayoutFile( "MainMenuMovie" ));
	if ( m_pMainMenuMovie )
	{
		m_pMainMenuMovie->Play();
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::UnloadBackgroundMovie()
{
	if ( m_pMainMenuMovie )
	{
		m_pMainMenuMovie->Stop();

		// RemoveAndDeleteChildren below used to be DeleteAsync, but that breaks if state toggles from 
		// game->main, main->game, game->main all in the same frame (which can happen if client becomes
		// unresponsive on team select)
		m_pMainMenuMovieParent->RemoveAndDeleteChildren();	
		m_pMainMenuMovie = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::LoadVanityPanel()
{
	m_pVanityPanelParent->RequireLoadLayoutSnippet( "MainMenuVanitySnippet" );
	m_pVanityPanel = m_pVanityPanelParent->FindChildInLayoutFile( "JsMainmenu_Vanity" );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_MainMenu::UnloadVanityPanel()
{
	if ( m_pVanityPanel )
	{
		// RemoveAndDeleteChildren below used to be DeleteAsync, see comment in UnloadBackgroundMovie
		m_pVanityPanelParent->RemoveAndDeleteChildren();
		m_pVanityPanel = nullptr;

	}
}

