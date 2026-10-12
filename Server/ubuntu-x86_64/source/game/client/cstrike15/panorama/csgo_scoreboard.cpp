//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Component for Scoreboard access. Only used by panorama and mirrors the Scoreboard_scaleform singleton in SF.
//
//=============================================================================//


#include "cbase.h"

#include "csgo_scoreboard.h"

#include "c_cs_player.h"
#include "cs_gamerules.h"
#include "c_cs_playerresource.h"
#include "weapon_selection.h"
#include "IGameUIFuncs.h"
#include "inputsystem/iinputsystem.h"
#include "econ/econ_item_description.h"
#include "clientmode_csnormal.h"
#include "panorama/csgo_endofmatch.h"
#include "gameui_hudinterfaces.h"
#include "hltvreplaysystem.h"
#include "hltvcamera.h"
#include "hud_basechat.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "cs_hud_chat.h"

#include "panorama/hud/csgo_hud.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DEFINE_PANORAMA_EVENT( OnOpenScoreboard );
DEFINE_PANORAMA_EVENT( OnCloseScoreboard );

DECLARE_PANORAMA_EVENT0( Scoreboard_UpdateJob );
DEFINE_PANORAMA_EVENT( Scoreboard_UpdateJob );

DECLARE_PANORAMA_EVENT0( Scoreboard_OnMouseActive );
DEFINE_PANORAMA_EVENT( Scoreboard_OnMouseActive );

DECLARE_PANORAMA_EVENT0( Scoreboard_UpdateEverything );
DEFINE_PANORAMA_EVENT( Scoreboard_UpdateEverything );

DECLARE_PANORAMA_EVENT0( Scoreboard_ResetAndInit );
DEFINE_PANORAMA_EVENT( Scoreboard_ResetAndInit );

DECLARE_PANORAMA_EVENT1( Scoreboard_UpdatePlayerByEntIndex, int );
DEFINE_PANORAMA_EVENT( Scoreboard_UpdatePlayerByEntIndex );

DECLARE_PANORAMA_EVENT1( Scoreboard_UpdateHLTVViewers, int );
DEFINE_PANORAMA_EVENT( Scoreboard_UpdateHLTVViewers );

DECLARE_PANORAMA_EVENT0( Scoreboard_ToggleSetCasterIsCameraman );
DEFINE_PANORAMA_EVENT( Scoreboard_ToggleSetCasterIsCameraman );
DECLARE_PANORAMA_EVENT0( Scoreboard_ToggleSetCasterIsHeard );
DEFINE_PANORAMA_EVENT( Scoreboard_ToggleSetCasterIsHeard );
DECLARE_PANORAMA_EVENT0( Scoreboard_ToggleSetCasterControlsXray );
DEFINE_PANORAMA_EVENT( Scoreboard_ToggleSetCasterControlsXray );
DECLARE_PANORAMA_EVENT0( Scoreboard_ToggleSetCasterControlsUI );
DEFINE_PANORAMA_EVENT( Scoreboard_ToggleSetCasterControlsUI );

REGISTER_PANEL2D_FACTORY( CCSGO_Scoreboard, CSGOScoreboard )

CCSGO_Scoreboard *CCSGO_Scoreboard::s_pScoreboard = NULL;

ConVar cl_scoreboard_survivors_always_on( "cl_scoreboard_survivors_always_on", "0", FCVAR_RELEASE | FCVAR_ARCHIVE );

CCSGO_Scoreboard::CCSGO_Scoreboard( CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_Scoreboard", this ),
	panorama::CPanel2D( pParent, pchID ),
	m_Capture( this, "Scoreboard", panorama::k_EGameInputShareMouse, false )
{
	
	Assert( s_pScoreboard == NULL );
	s_pScoreboard = this;

	// Make sure scoreboard panel has its own input hierarchy and therefore will not lose focus
	// when pushing another input context (peer panels such as chat, buy menu, ...)
	SetTopOfInputContext( true );
	
	SetInputNamespace( "scoreboard" );

	SetAcceptsInput( true );
	SetAcceptsFocus( true );
	UIPanel()->SetCanClearFocusByClicking( false );

	// Let mouse input through when scoreboard is up-- People may want to shoot while checking scores; previous SF scoreboard allowed this.
	UIPanel()->SetAlwaysConsumeHoverClicks( false );

	DbgVerify( BLoadLayout( "file://{resources}/layout/Scoreboard.xml" ) );

	SetHasClass( "hidden", true );

	ListenForGameEvent( "announce_phase_end" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "cs_game_disconnected" );
	ListenForGameEvent( "cs_match_end_restart" );
	ListenForGameEvent( "nextlevel_changed" );
	ListenForGameEvent( "begin_new_match" );


	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_spawn" );

	ListenForGameEvent( "hltv_status" );

	m_bWantLateUpdate = true;

	m_bLockOpen = false;
}

CCSGO_Scoreboard::~CCSGO_Scoreboard()
{

	Assert( s_pScoreboard == this );
	s_pScoreboard = NULL;
}

void CCSGO_Scoreboard::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	panorama::RegisterJSMethod( "CloseScoreboard", PANORAMA_DELEGATE( &CCSGO_Scoreboard::CloseScoreboard ) );
}

void CCSGO_Scoreboard::Think()
{
	static float prevUpdateTime = 0;
 	if ( !BIsVisible() )
 	{
 		// gpGlobals->curtime resets on map load
 		prevUpdateTime = 0;
 	}
 	else
 	{
		if ( gpGlobals->curtime - prevUpdateTime >= 0.01f )
		{
			panorama::DispatchEvent( Scoreboard_UpdateJob(), NULL );
			prevUpdateTime = gpGlobals->curtime;
		}
	}
}

void CCSGO_Scoreboard::FireGameEvent( IGameEvent *event )
{
	if ( !CSGameRules() )
		return;

	CDemoPlaybackParameters_t const *pPlayback = engine->GetDemoPlaybackParameters();
	bool bAnonymous = pPlayback && pPlayback->m_bAnonymousPlayerIdentity;

	const char *type = event->GetName();

	if ( !V_strcmp( type, "announce_phase_end" ) )
	{
		if ( CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME )
		{
			m_bLockOpen = true;
			OpenScoreboard();
		}
	}
	else if ( !V_strcmp( type, "round_start" ) )
	{
		if ( m_bLockOpen )
		{
			m_bLockOpen = false;
			CloseScoreboard();
		}

		panorama::DispatchEvent( Scoreboard_UpdateEverything(), NULL );
	}
	else if ( !V_strcmp( type, "round_end" ) )
	{
		panorama::DispatchEvent( Scoreboard_UpdateEverything(), NULL );
	}
	else if ( !V_strcmp( type, "cs_game_disconnected" ) ||
		!V_strcmp( type, "cs_match_end_restart" ) ||
		!V_strcmp( type, "nextlevel_changed" ) ||
		!V_strcmp( type, "begin_new_match" ) )
	{

		panorama::DispatchEvent( Scoreboard_ResetAndInit(), NULL );
	}
	else if ( !V_strcmp( type, "player_death" ) ||
		( !V_strcmp( type, "player_spawn" ) ) )
	{
		int userid = event->GetInt( "userid", -1 );

		C_BasePlayer * pP = UTIL_PlayerByUserId( userid );

		if ( pP )
		{
			int entindex = pP->entindex();
			panorama::DispatchEvent( Scoreboard_UpdatePlayerByEntIndex(), NULL, entindex );
		}
	}
	if ( !V_strcmp( type, "hltv_status" ) )
	{
		if ( !bAnonymous )
		{
			// spectators = clients - proxies + external viewers
			int nClients = event->GetInt( "clients" );
			nClients -= event->GetInt( "proxies" );
			nClients += event->GetInt( "externaltotal" );

			panorama::DispatchEvent( Scoreboard_UpdateHLTVViewers(), NULL, nClients );
		}
		else
		{
			panorama::DispatchEvent( Scoreboard_UpdateHLTVViewers(), NULL, 0 );
		}
	}


}

bool CCSGO_Scoreboard::CloseScoreboard( void )
{

// 	if( BIsVisible() )
// 	{
// 		GetHud().EnableHud();
// 	}

	if ( m_bLockOpen )
		return true;

	if ( BIsVisible() )
	{
		SetHasClass( "hidden", true );

		UnregisterForUnhandledEvent( OnMouseEnableBinding(), this, &CCSGO_Scoreboard::EventOnMouseEnableBinding );

		panorama::DispatchEvent( OnCloseScoreboard(), NULL );

		// Removing chat panel from the input context stack
		GetParentWindow()->UIWindowInput()->RemoveInputContext( this->UIPanel() );

		if ( m_Capture.BEnabled() )
			SetHitTestChildrenEnabled( false );

		m_Capture.Disable();
	}

	return true;

}



void CCSGO_Scoreboard::OpenScoreboard()
{
	// from baseviewport.cpp
	// don't show input panels during normal demo playback
	if  ( engine->IsPlayingDemo() && !g_bEngineIsHLTV )
		return;

	// don't let the scoreboard open during instant replay
	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
		return;


	SetHasClass( "hidden", false );

	static panorama::CPanoramaSymbol k_symArmRaceScoreboard( "armrace-scoreboard" );
	SetHasClass( k_symArmRaceScoreboard, CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive() );

	// If the chat panel is up, do not switch input context (chat panel should have focus) as scoreboard
	// being opened is not in response to the player direct input (eg. scoreboard at half time)
	// (JIRA CSGO-1610)
	panorama::IUIPanel *pRestoreContext = nullptr;
	IHudChat* pHudChat = GetHudChat();
	if ( pHudChat && pHudChat->ChatRaised() )
	{
		pRestoreContext = GetParentWindow()->UIWindowInput()->GetInputFocusContext();
	}
	
	// Scoreboard panel has its own input context (cf SetTopOfInputContext( true ) in constructor)
	// Therefore calling SetFocus will switch input context. Make sure to remove it from the input context stack
	// when hiding the scoreboard panel
	SetFocus();

	if ( pRestoreContext )
	{
		// Restore input context to the chat panel if necessary
		GetParentWindow()->UIWindowInput()->SetInputFocusContext( pRestoreContext );
	}

	SetHitTestChildrenEnabled( false );

	RegisterForUnhandledEvent( OnMouseEnableBinding(), this, &CCSGO_Scoreboard::EventOnMouseEnableBinding );

	panorama::DispatchEvent( OnOpenScoreboard(), NULL );

	// If mouse is already active, scoreboard should act as if you enabled the mouse
	if ( gameuifuncs->PanoramaDeniesInputToGame( panorama::k_EGameInputUIEnableMouseCursor ) )
		EventOnMouseEnableBinding();
}

bool CCSGO_Scoreboard::EventOnMouseEnableBinding( void )
{
	Assert( BIsVisible() );
	// If we're hearing this event while hidden, this is a problem. Don't swallow the key since we shouldn't be handling
	// it when not shown. Double OpenScoreboard calls shows may do this. 
	if ( !BIsVisible() )
	{
		DevMsg( "Scoreboard: Tried to handle mouse enable binding while not visible." );
		return false;
	}
	
	if ( !m_Capture.BEnabled() )
		SetHitTestChildrenEnabled( true );

	m_Capture.Enable();
	panorama::DispatchEvent( Scoreboard_OnMouseActive(), NULL );

	return true;
}
