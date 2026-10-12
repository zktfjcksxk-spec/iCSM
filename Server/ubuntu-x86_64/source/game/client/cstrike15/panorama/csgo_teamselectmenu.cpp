//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Panorama menu for selecting team
//
//=====================================================================================//

#include "cbase.h"
#include "csgo_teamselectmenu.h"
#include "IGameUIFuncs.h"
#include "panorama/ui_root.h"
#include "panorama/uijsregistration.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "clientsteamcontext.h"
#include "uicomponents/uicomponent_gamestate.h"
#include "gametypes/igametypes.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameTypes *g_pGameTypes;
using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_TeamSelectMenu, CSGOTeamSelectMenu );

DEFINE_PANORAMA_EVENT_DOC( CSGOShowTeamSelectMenu, "bool", "Show or hide the team select menu." );
DEFINE_PANORAMA_EVENT_DOC( PlayerTeamChanged, "", "Fired when a player switches to a valid team." );
DEFINE_PANORAMA_EVENT_DOC( LocalPlayerTeamChanged, "", "Fired when the local player switches to a valid team." );
DEFINE_PANORAMA_EVENT_DOC( ServerForcingTeamJoin, "", "Fired when the server starts a countdown to force the local player on a team." );
DEFINE_PANORAMA_EVENT_DOC( TeamJoinFailed, "", "Failed to join the team we requested." );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_TeamSelectMenu::CCSGO_TeamSelectMenu( panorama::CPanel2D *pParent, const char *pchID )
	:
	panorama::CPanel2D( pParent, pchID ),
	m_Capture( this, "TeamSelect", k_EGameInputShareMouse| k_EGameInputUIEnableKeyInput )
{
	ReloadLayout();
	RegisterForUnhandledEvent( CSGOShowTeamSelectMenu(), this, &CCSGO_TeamSelectMenu::Show );
	RegisterForUnhandledEvent( ServerForcingTeamJoin(), this, &CCSGO_TeamSelectMenu::OnServerForcingTeamJoin );
	RegisterForUnhandledEvent( GameEvent_CSMatchEndRestart(), this, &CCSGO_TeamSelectMenu::OnMatchEndRestart );
	RegisterForUnhandledEvent( GameState_ServerSpawn(), this, &CCSGO_TeamSelectMenu::EventServerSpawn );

	GameUI().RegisterGameUIStateListener( this );

	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "jointeam_failed" );

	// Make sure TeamSelect panel has its own input hierarchy and therefore will not lose focus
	// when pushing another input context (peer panels such as scoreboard)
	SetTopOfInputContext( true );
}

void CCSGO_TeamSelectMenu::ReloadLayout()
{
	if ( IsLoaded() )
		UnloadLayout();

	RequireLoadLayout( "file://{resources}/layout/teamselectmenu.xml" );
	m_pTimer = panorama::panel_cast< CCountdown* >( FindChildInLayoutFile( "AutojoinTimer" ) );
	m_pTeamCharacterTerrorist = panel_cast< CUI_ItemPreviewPanel* >( FindChildInLayoutFile( "TeamCharT" ) );
	m_pTeamCharacterCT = panel_cast< CUI_ItemPreviewPanel* >( FindChildInLayoutFile( "TeamCharCT" ) );
}

CCSGO_TeamSelectMenu::~CCSGO_TeamSelectMenu()
{
	GameUI().UnregisterGameUIStateListener( this );
}

void CCSGO_TeamSelectMenu::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "BuildDebugPanel", PANORAMA_DELEGATE( &CCSGO_TeamSelectMenu::BuildDebugPanel ) );
	RegisterJSMethod( "GetPlayerModelCT", PANORAMA_DELEGATE( &CCSGO_TeamSelectMenu::GetCTModel ) );
	RegisterJSMethod( "GetPlayerModelTerrorist", PANORAMA_DELEGATE( &CCSGO_TeamSelectMenu::GetTModel ) );
	RegisterJSMethod( "GetTeamNumber", PANORAMA_DELEGATE( &CCSGO_TeamSelectMenu::GetTeamNumber ) );
}

void CCSGO_TeamSelectMenu::BuildDebugPanel( panorama::CPanel2D* pPanel )
{
	pPanel->BCreateChildren( R"XML( 
	<Panel class="top-bottom-flow">
		<Button onactivate="$.DispatchEvent('CSGOShowTeamSelectMenu', true)">
			<Label text="Show"/>
		</Button>
		<Button onactivate="$.DispatchEvent('CSGOShowTeamSelectMenu', false)">
			<Label text="Hide"/>
		</Button>
		<Button onactivate="$.DispatchEvent('ServerForcingTeamJoin', 15 )">
			<Label text="Countdown"/>
		</Button>
		<Button onactivate="$.DispatchEvent('TeamJoinFailed', '#Cstrike_TitlesTXT_All_Teams_Full' )">
			<Label text="Error Message"/>
		</Button>
	</Panel>
	)XML" );
}

bool CCSGO_TeamSelectMenu::OnMatchEndRestart( void )
{
	if ( !BIsVisible() )
		ReloadLayout();
	panorama::DispatchEvent( CSGOShowTeamSelectMenu(), this, true );
	return false;
}

#if 0 // UNUSED -- JS stomps model when playing animations so no need to set here
bool Helper_SetModelPanel( CUI_ItemPreviewPanel* pPreview, const char *szModelName )
{
	UIItemInfo_t info;
	info.m_manifestName = "resource/ui/econ/ItemModelPanelCharMainMenu.res";
	if ( pPreview && szModelName )
	{
		CFmtStr path( "models/player/custom_player/legacy/%s.mdl", szModelName );
		info.m_itemName = path.Get();
		return pPreview->SetScene( info, true );
	}
	return false;
}
#endif

bool Helper_GetModelForMap( int iTeamNum, CUtlString& outStr )
{
	outStr.Clear();
	const char* szMapName = engine->GetLevelNameShort();
	if ( !szMapName )
		return false; 

	const CUtlStringList *pModels = nullptr;
	if ( iTeamNum == TEAM_CT )
	{
		pModels = g_pGameTypes->GetCTModelsForMap( szMapName );
	}
	else if ( iTeamNum == TEAM_TERRORIST )
	{
		pModels = g_pGameTypes->GetTModelsForMap( szMapName );
	}

	if ( !pModels || !pModels->Count() )
		return false;

	const char* szModel = pModels->Element( RandomInt( 0, pModels->Count() - 1 ) );
	if ( FStrEq( V_GetFileExtensionSafe( szModel ), "mdl" ) && g_pFullFileSystem->FileExists( szModel ) )
	{
		outStr = szModel;
		return true;
	}
	else 
	{
		CUtlString strDefaultModel;
		strDefaultModel.Format( "models/player/custom_player/legacy/%s.mdl", szModel );
		if ( g_pFullFileSystem->FileExists( strDefaultModel.Get() ) )
		{
			outStr = strDefaultModel;
			return true;
		}
	}
	return false;
}

bool CCSGO_TeamSelectMenu::EventServerSpawn( void )
{
	ReloadLayout();

	CUtlString strName;
	if ( Helper_GetModelForMap( TEAM_CT, strName ) )
		m_strCTModel = strName;
	else
		m_strCTModel = "models/player/custom_player/legacy/ctm_sas.mdl";


	if ( Helper_GetModelForMap( TEAM_TERRORIST, strName ) )
		m_strTModel = strName;
	else
		m_strTModel = "models/player/custom_player/legacy/tm_phoenix.mdl";
	
	return false;
}

bool CCSGO_TeamSelectMenu::Show( bool bShow )
{
	SetReadyForDisplay( bShow );
	SetVisible( bShow );

	if ( bShow )
	{
		// TeamSelect panel has its own input context (cf SetTopOfInputContext( true ) in constructor)
		// Therefore calling SetFocus will switch input context. Make sure to remove it from the stack
		// when hiding the TeamSelect panel
		SetFocus();

		m_Capture.Enable();
		m_pTimer->SetUpdateEnabled( true );
	
		GameUI().StartBackgroundMusicFade();

		//Play team selection music.
		CLocalPlayerFilter filter;
		PlayMusicSelection(filter, CSMUSIC_SELECTION);
	}
	else
	{
		// Removing chat panel from the input context stack
		GetParentWindow()->UIWindowInput()->RemoveInputContext( this->UIPanel() );

		m_Capture.Disable();
		m_pTimer->SetUpdateEnabled( false );

		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.StopSelection" );
	}

	return false;
}

void CCSGO_TeamSelectMenu::OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState )
{
	if( BIsVisible() && ( nNewState != CSGO_GAME_UI_STATE_LOADINGSCREEN ) )
	{
		Show( false );
	}
}

void CCSGO_TeamSelectMenu::FireGameEvent( IGameEvent * event )
{
	const char* szName = event->GetName();
	if ( !V_strcmp( szName, "jointeam_failed" ) )
	{
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetUserID() == event->GetInt( "userid" ) )
		{
			extern const char* TeamJoinFailReasonToErrorString( TeamJoinFailedReason::Type iReason );
			TeamJoinFailedReason::Type reason = ( TeamJoinFailedReason::Type )event->GetInt( "reason" );
			const char* msg = TeamJoinFailReasonToErrorString( reason );
			DispatchEvent( TeamJoinFailed(), ( panorama::IUIPanelClient* ) nullptr, msg );
		}
	}
	else if ( !V_strcmp( szName, "player_team" ) )
	{
		int iUserID = event->GetInt( "userid" );
		int iTeam = event->GetInt( "team" );
		int iPrevTeam = event->GetInt( "oldteam" );
		//bool bIsDisconnect = event->GetInt( "disconnect" );
		//bool bWasAutoAssigned = event->GetInt( "autoteam" );
		//bool bIsSilent = event->GetInt( "silent" );

		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		XUID id = GetCSResources() ? GetCSResources()->GetXuid( engine->GetPlayerForUserID( iUserID ) ) : 0;

		// Dispatch async events so GameResources entity has a chance to catch up
		if ( pLocalPlayer && pLocalPlayer->GetUserID() == iUserID )
		{
			Show( false );
			panorama::DispatchEventAsync( 0.1f, LocalPlayerTeamChanged(), ( panorama::IUIPanelClient* ) nullptr, id, iPrevTeam, iTeam );
		}
		panorama::DispatchEventAsync( 0.1f, PlayerTeamChanged(), ( panorama::IUIPanelClient* ) nullptr, id, iPrevTeam, iTeam );
	}
}

bool CCSGO_TeamSelectMenu::OnServerForcingTeamJoin( float flTime )
{
	time_t tTimeNow = time( NULL );
	m_pTimer->SetEndTime( tTimeNow + (time_t)flTime );
	return false;
}

const char* CCSGO_TeamSelectMenu::GetCTModel() const
{
	return m_strCTModel;
}

const char* CCSGO_TeamSelectMenu::GetTModel() const
{
	return m_strTModel;
}

int CCSGO_TeamSelectMenu::GetTeamNumber( void ) const
{
	if ( C_CSPlayer::GetLocalCSPlayer() )
		return C_CSPlayer::GetLocalCSPlayer()->GetTeamNumber();
	return 0;
}
